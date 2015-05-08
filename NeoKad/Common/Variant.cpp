#include "GlobalHeader.h"
#include "Variant.h"
#include "../../Framework/Scope.h"
#include "../../zlib/zlib.h"
#include "../../Framework/Exception.h"
#include "../../Framework/Strings.h"

CVariant::CVariant(EType Type)
{
	if(Type != EInvalid)
		InitValue(Type, 0, NULL);
	else
		m_Variant = NULL;
}

CVariant::CVariant(const CVariant& Variant)
{
	m_Variant = NULL;
	Assign(Variant);
}

CVariant::~CVariant()
{
	Clear();
}

void CVariant::Assign(const CVariant& Variant)
{
	const SVariant* VariantVal = Variant.Val();
	if(m_Variant && m_Variant->Access != eReadWrite)
		throw CException(LOG_ERROR | LOG_DEBUG, L"variant access violation; Assign");
	if(VariantVal->Access == eDerived)
	{
		// Note: We can not asign buffer derivations, as the original variant may be removed at any time,
		//	those we must copy the buffer on accessey by value
		SVariant* VariantCopy = new SVariant;
		VariantCopy->Type = VariantVal->Type;
		VariantCopy->Size = VariantVal->Size;
		VariantCopy->Payload = new byte[VariantCopy->Size];
		memcpy(VariantCopy->Payload,VariantVal->Payload, VariantCopy->Size);
		VariantCopy->Access = eReadOnly;
		Attach(VariantCopy);
	}
	else
	{
		ASSERT(Variant.m_Variant);
		Attach(Variant.m_Variant);
	}
}

void CVariant::Clear()
{
	if(m_Variant && --m_Variant->Refs <= 0)
		delete m_Variant;
	m_Variant = NULL;
}

CVariant CVariant::Clone(bool Full) const
{
	const SVariant* Variant = Val();

	// Note: If we do a shallow copy of the Variant, drop extended informations
	if(!Full && Variant->Type == EExtended) 
		return Variant->At("").Clone(false);

	CVariant NewVariant;
	NewVariant.Attach(Variant->Clone(Full));
	return NewVariant;
}

void CVariant::Attach(SVariant* Variant)
{
	if(m_Variant && --m_Variant->Refs <= 0)
		delete m_Variant;
	m_Variant = Variant;
	m_Variant->Refs++;
}

void CVariant::Extend()
{
	if(Val()->Type == EExtended)
		return;

	Freeze();

	ASSERT(m_Variant && m_Variant->Refs == 1);
	SVariant* Variant = m_Variant;
	Variant->Refs--;

	m_Variant = new SVariant();
	m_Variant->Refs++;
	m_Variant->Init(EExtended);
	m_Variant->At("").Attach(Variant);
}

void CVariant::InitValue(EType Type, size_t Size, const void* Value, bool bTake)
{
	m_Variant = new SVariant;
	m_Variant->Refs++;

	m_Variant->Init(Type, Size, Value, bTake);
}

void CVariant::Detach()
{
	ASSERT(m_Variant);
	ASSERT(m_Variant->Refs > 1);
	m_Variant->Refs--;
	m_Variant = m_Variant->Clone();
	m_Variant->Refs++;
}

void CVariant::Freeze()
{
	SVariant* Variant = Val();
	if(Variant->Access == eReadOnly)
		return;

	// Note: we must flly serialize and deserialize the variant in order to not store maps and list content multiple times in memory
	//	The CPU overhead should be howeever minimal as we imply parse on read for the dictionaries, 
	//	and we usually dont read the variant afterwards but put in on the socket ot save it to a file
	CBuffer Packet;
	ToPacket(&Packet);
	Packet.SetPosition(0);
	FromPacket(&Packet);
}

void CVariant::Unfreeze()
{
	SVariant* Variant = Val();
	if(Variant->Access == eReadWrite)
		return;

	Attach(Variant->Clone());
}

bool CVariant::IsFrozen() const
{
	const SVariant* Variant = Val();
	/*if(Variant->Type == EExtended)
		return Variant->At("").IsFrozen();*/
	return Variant->Access != eReadWrite;
}

void CVariant::FromPacket(const CBuffer* pPacket, bool bDerived)
{
	Clear();
	SVariant* Variant = Val();

	byte Type = pPacket->ReadValue<uint8>();
	ASSERT(Type != 0xFF);
	if((Type & ELen32) == ELen8)
		Variant->Size = pPacket->ReadValue<uint8>();
	else if((Type & ELen32) == ELen16)
		Variant->Size = pPacket->ReadValue<uint16>();
	else
		Variant->Size = pPacket->ReadValue<uint32>();
	if(pPacket->GetSizeLeft() < Variant->Size)
		throw CException(LOG_ERROR | LOG_DEBUG, L"incomplete variant");

	Variant->Type = (EType)(Type & ~ELen32); // clear size flags
	if(bDerived)
	{
		ASSERT((Type & ELen32) != 0); // can not derive a packed variant

		Variant->Access = eDerived;
		Variant->Payload = pPacket->ReadData(Variant->Size);
	}
	else
	{
		Variant->Access = eReadOnly;
		if((Type & ELen32) == 0) // is it packed
		{
			byte* pData = pPacket->ReadData(Variant->Size);

			uLongf newsize = Variant->Size*10+300;
			uLongf unpackedsize = 0;
			int result = 0;
			CBuffer Buffer;
			do
			{
				Buffer.AllocBuffer(newsize, true);
				unpackedsize = newsize;
				result = uncompress(Buffer.GetBuffer(),&unpackedsize,pData,Variant->Size);
				newsize *= 2; // size for the next try if needed
			}
			while (result == Z_BUF_ERROR && newsize < Max(MB2B(16), Variant->Size*100));	// do not allow the unzip buffer to grow infinetly,
																					// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
			if (result == Z_OK)
			{
				Variant->Size = unpackedsize;
				Variant->Payload = new byte[Variant->Size];
				memcpy(Variant->Payload, Buffer.GetBuffer(), Variant->Size);
			}
			else
				return;
		}
		else
		{
			Variant->Payload = new byte[Variant->Size];
			memcpy(Variant->Payload, pPacket->ReadData(Variant->Size), Variant->Size);
		}
	}
}

void CVariant::ToPacket(CBuffer* pPacket, bool bPack) const
{
	const SVariant* Variant = Val();

	CBuffer Payload;
	Variant->MkPayload(Payload);

	if(bPack)
	{
		ASSERT(Payload.GetSize() + 300 < 0xFFFFFFFF);
		uLongf newsize = (uLongf)(Payload.GetSize() + 300);
		CBuffer Buffer(newsize);
		int result = compress2(Buffer.GetBuffer(),&newsize,Payload.GetBuffer(),(uLongf)Payload.GetSize(),Z_BEST_COMPRESSION);

		if (result == Z_OK && Payload.GetSize() > newsize) // does the compression helped?
		{
			ASSERT(Variant->Type != 0xFF);
			pPacket->WriteValue<uint8>(Variant->Type);
			pPacket->WriteValue<uint32>(newsize);
			pPacket->WriteData(Buffer.GetBuffer(), newsize);
			return;
		}
	}

	
	size_t uLength = Payload.GetSize();
	ASSERT(uLength < 0xFFFFFFFF);

	uint8 Type = Variant->Type;
	if(uLength >= USHRT_MAX || bPack)
		Type |= ELen32;
	else if(uLength >= UCHAR_MAX)
		Type |= ELen16;
	else
		Type |= ELen8;

	ASSERT(Type != 0xFF);
	pPacket->WriteValue<uint8>(Type);
	if((Type & ELen32) == ELen8)
		pPacket->WriteValue<uint8>((uint8)uLength);
	else if((Type & ELen32) == ELen16)
		pPacket->WriteValue<uint16>((uint16)uLength);
	else
		pPacket->WriteValue<uint32>((uint32)uLength);
	pPacket->WriteData(Payload.GetBuffer(), Payload.GetSize());
}

#ifdef USING_QT
bool CVariant::FromQVariant(const QVariant& qVariant)
{
	Clear();
	SVariant* Variant = Val();

	switch(qVariant.type())
	{
		case QVariant::Map:
		//case QVariant::Hash:
		{
			QVariantMap Map = qVariant.toMap();
			Variant->Type = Map.contains("Variant") ? EExtended : EMap;
			Variant->Container.Map = new map<string, CVariant>;
			for(QVariantMap::iterator I = Map.begin(); I != Map.end(); ++I)
			{
				string Name = (I.key() == "Variant") ? "" : I.key().toStdString();

				CVariant Temp;
				Temp.FromQVariant(I.value());
				Variant->Insert(Name.c_str(), Temp);
			}
			break;
		}
		case QVariant::List:
		case QVariant::StringList:
		{
			QVariantList List = qVariant.toList();
			Variant->Type = EList;
			Variant->Container.List = new vector<CVariant>;
			for(QVariantList::iterator I = List.begin(); I != List.end(); ++I)
			{
				CVariant Temp;
				Temp.FromQVariant(*I);
				Variant->Append(Temp);
			}
			break;
		}
		case QVariant::Bool:
		{
			bool Value = qVariant.toBool();
			Variant->Init(EUInt, sizeof(Value), &Value);
			break;
		}
		//case QVariant::Char:
		case QVariant::Int:
		{
			sint32 Value = qVariant.toInt();
			Variant->Init(ESInt, sizeof (sint32), &Value);
			break;
		}
		case QVariant::UInt:
		{
			uint32 Value = qVariant.toUInt();
			Variant->Init(EUInt, sizeof (sint32), &Value);
			break;
		}
		case QVariant::LongLong:
		{
			sint64 Value = qVariant.toLongLong();
			Variant->Init(ESInt, sizeof(Value), &Value);
			break;
		}
		case QVariant::ULongLong:
		{
			uint64 Value = qVariant.toULongLong();
			Variant->Init(EUInt, sizeof(Value), &Value);
			break;
		}
		case QVariant::Double:
		{
			double Value = qVariant.toDouble();
			Variant->Init(EDouble, sizeof(Value), &Value);
			break;
		}
		case QVariant::String:
		{
			QByteArray Value = qVariant.toString().toUtf8();
			Variant->Init(EUtf8, Value.size(), Value.data());
			break;
		}
		case QVariant::ByteArray:
		{
			QByteArray Value = qVariant.toByteArray();
			Variant->Init(EBytes, Value.size(), Value.data());
			break;
		}
		//case QVariant::BitArray:
		/*case QVariant::Date:
		case QVariant::Time:
		case QVariant::DateTime:
		case QVariant::Url:*/
		default:
			ASSERT(0);
			return false;
	}
	return true;
}

QVariant CVariant::ToQVariant() const
{
	const SVariant* Variant = Val();

	switch(Variant->Type)
	{
		case EExtended:
		case EMap:
		{
			QVariantMap Map;
			for(uint32 i=0; i<Variant->Count();i++)
			{
				QString Name = QString::fromStdString(Variant->Key(i));
				ASSERT(Name != "Variant");
				if(Name.isEmpty())
					Name = "Variant"; // payload, empty names are not supported in text representations
				Map[Name] = Variant->At(i).ToQVariant();
			}
			return Map;
		}
		case EList:
		{
			QVariantList List;
			for(uint32 i=0; i<Variant->Count();i++)
				List.append(Variant->At(i).ToQVariant());
			return List;
		}

		case EBytes:
			return QByteArray((char*)Variant->Payload, Variant->Size);

		case EUtf8:
			return QString::fromUtf8((char*)Variant->Payload, Variant->Size);
		case EAscii:
			return QString::fromLatin1((char*)Variant->Payload, Variant->Size);

		case ESInt:
			if(Variant->Size <= sizeof(sint8))
				return sint8(*this);
			else if(Variant->Size <= sizeof(sint16))
				return sint16(*this);
			else if(Variant->Size <= sizeof(sint32))
				return sint32(*this);
			else if(Variant->Size <= sizeof(uint64))
				return sint64(*this);
			else
				return QByteArray((char*)Variant->Payload, Variant->Size);
		case EUInt:
			if(Variant->Size <= sizeof(uint8))
				return uint8(*this);
			else if(Variant->Size <= sizeof(uint16))
				return uint16(*this);
			else if(Variant->Size <= sizeof(uint32))
				return uint32(*this);
			else if(Variant->Size <= sizeof(uint64))
				return uint64(*this);
			else
				return QByteArray((char*)Variant->Payload, Variant->Size);

		case EDouble:
			return double(*this);

		case EInvalid:
		default:
			return QVariant();
	}
}
#endif

uint32 CVariant::Count() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Count();
	return Variant->Count();
}

bool CVariant::IsMap() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").IsMap();
	return Variant->Type == EMap;
}

const string& CVariant::Key(uint32 Index) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Key(Index);
	return Variant->Key(Index);
}

CVariant& CVariant::At(const char* Name)
{
	SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").At(Name);
	if(Variant->Access != eReadWrite && !Variant->Has(Name))
		throw CException(LOG_ERROR | LOG_DEBUG, L"variant access violation; Map Member: %S is not present", Name);
	if(Variant->Type != EMap)
		Variant->Init(EMap);
	return Variant->At(Name);
}

CVariant CVariant::Get(const char* Name, const CVariant& Default) const
{
	if(!Has(Name))
		return Default;
	return At(Name);
}

const CVariant& CVariant::At(const char* Name) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").At(Name);
	return Variant->At(Name);
}

bool CVariant::Has(const char* Name) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Has(Name);
	return Variant->Has(Name);
}

void CVariant::Remove(const char* Name)
{
	SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Remove(Name);
	return Variant->Remove(Name);
}

bool CVariant::IsList() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").IsList();
	return Variant->Type == EList;
}

void CVariant::Insert(const char* Name, const CVariant& variant)
{
	SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Insert(Name, variant);
	if(Variant->Access != eReadWrite)
		throw CException(LOG_ERROR | LOG_DEBUG, L"variant access violation; Map Insert");
	if(Variant->Type != EMap)
		Variant->Init(EMap);
	Variant->Insert(Name, variant);
}

void CVariant::Append(const CVariant& variant)
{
	SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Append(variant);
	if(Variant->Access != eReadWrite)
		throw CException(LOG_ERROR | LOG_DEBUG, L"variant access violation; List Append");
	if(Variant->Type != EList)
		Variant->Init(EList);
	Variant->Append(variant);
}

CVariant& CVariant::At(uint32 Index) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").At(Index);
	return Variant->At(Index);
}

void CVariant::Remove(uint32 Index)
{
	SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").Remove(Index);
	return Variant->Remove(Index);
}

void CVariant::Merge(const CVariant& Variant)
{
	if(!IsValid()) // if this valiant is void we can merge with anything
		Assign(Variant);
	else if(Variant.IsList() && IsList())
	{
		for(int i=0; i < Variant.Count(); i++)
			Append(Variant.At(i));
	}
	else if(Variant.IsMap() && IsMap())
	{
		for(int i=0; i < Variant.Count(); i++)
			Insert(Variant.Key(i).c_str(), Variant.At(i));
	}
	else
		throw CException(LOG_ERROR | LOG_DEBUG, L"Variant could not be merged, type mismatch");
}


void CVariant::GetInt(size_t Size, void* Value) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EInvalid)
	{
		memset(Value, 0, Size);
		return;
	}

	if(Variant->Type != ESInt && Variant->Type != EUInt)
		throw CException(LOG_ERROR | LOG_DEBUG, L"Int Value can not be pulled form a incomatible type");
	if(Size < Variant->Size)
		throw CException(LOG_ERROR | LOG_DEBUG, L"Int Value pulled is shorter than int value present");

	memcpy(Value, Variant->Payload, Variant->Size);
	if(Size > Variant->Size)
		memset((byte*)Value + Variant->Size, 0, Size - Variant->Size);
}

void CVariant::GetDouble(size_t Size, void* Value) const
{
	const SVariant* Variant = Val();

	if(Variant->Type != EDouble)
		throw CException(LOG_ERROR | LOG_DEBUG, L"double Value can not be pulled form a incomatible type");
	if(Size != Variant->Size)
		throw CException(LOG_ERROR | LOG_DEBUG, L"double Value pulled is shorter than int value present");

	memcpy(Value, Variant->Payload, Size);
}

string CVariant::ToString() const
{
	const SVariant* Variant = Val();
	string str;
	if(Variant->Type == EAscii)
		str = string((char*)Variant->Payload, Variant->Size);
	else if(Variant->Type == EUtf8)
	{
		wstring wstr = ToWString();
		WStrToAscii(str, wstr);
	}
	else if(Variant->Type != EInvalid)
		throw CException(LOG_ERROR | LOG_DEBUG, L"string Value can not be pulled form a incomatible type");
	return str;
}

CVariant::CVariant(const wstring& wstr)
{
	string str;
	WStrToUtf8(str, wstr);
	InitValue(EUtf8, str.length(), str.c_str());
}

CVariant::CVariant(const wchar_t* wstr)
{
	string str;
	WStrToUtf8(str, wstring(wstr));
	InitValue(EUtf8, str.length(), str.c_str());
}

wstring CVariant::ToWString() const
{
	const SVariant* Variant = Val();
	wstring wstr;
	if(Variant->Type == EAscii)
	{
		string str = string((char*)Variant->Payload, Variant->Size);
		AsciiToWStr(wstr, str);
	}
	else if(Variant->Type == EUtf8)
	{
		string str = string((char*)Variant->Payload, Variant->Size);
		Utf8ToWStr(wstr, str);
	}
	else if(Variant->Type != EInvalid)
		throw CException(LOG_ERROR | LOG_DEBUG, L"string Value can not be pulled form a incomatible type");
	return wstr;
}

wstring ToHex(const byte* Data, size_t uSize)
{
	wstring Hex;
	for (size_t i = 0; i < uSize; i++)
	{
		wchar_t buf[3];

		buf[0] = (Data[i] >> 4) & 0xf;
		if (buf[0] < 10)
            buf[0] += '0';
        else
            buf[0] += 'A' - 10;

		buf[1] = (Data[i]) & 0xf;
		if (buf[1] < 10)
            buf[1] += '0';
        else
            buf[1] += 'A' - 10;

		buf[2] = 0;
	
		Hex.append(buf);
	}
	return Hex;
}

CBuffer FromHex(wstring str)
{
	size_t length = str.length();
	if(length%2 == 1)
	{
		str = L"0" + str;
		length++;
	}

	CBuffer Buffer(length/2, true);
	byte* buffer = Buffer.GetBuffer();
	for(size_t i=0; i < length/2; i++)
	{
		wchar_t ch1 = str[i*2];
		int dig1;
		if(isdigit(ch1)) 
			dig1 = ch1 - '0';
		else if(ch1>='A' && ch1<='F') 
			dig1 = ch1 - 'A' + 10;
		else if(ch1>='a' && ch1<='f') 
			dig1 = ch1 - 'a' + 10;

		wchar_t ch2 = str[i*2 + 1];
		int dig2;
		if(isdigit(ch2)) 
			dig2 = ch2 - '0';
		else if(ch2>='A' && ch2<='F') 
			dig2 = ch2 - 'A' + 10;
		else if(ch2>='a' && ch2<='f') 
			dig2 = ch2 - 'a' + 10;

		buffer[i] = dig1*16 + dig2;
	}
	return Buffer;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Crypto Stuff

void CVariant::Sign(CPrivateKey* pPrivKey, UINT eAlgorithm)
{
	Extend();
	SVariant* Variant = Val();

	UINT eAsymCipher = pPrivKey->GetAlgorithm();
	UINT eAsymMode = eAlgorithm & CAbstractKey::eAsymMode;
	if(eAsymMode == 0)
		eAsymMode = CPublicKey::GetDefaultSS(eAsymCipher);
	UINT eAsymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eAsymHash == 0)
		eAsymHash = CPublicKey::eSHA1;

	CBuffer Packet;
	Variant->At("").ToPacket(&Packet);
	CBuffer Signature;
	if(!pPrivKey->Sign(&Packet, &Signature, eAsymMode | eAsymHash))
		throw CException(LOG_ERROR | LOG_DEBUG, L"Not an publi key for signature");
	if(eAlgorithm != 0)
		Variant->At("SS") = CAbstractKey::Algorithm2Str(eAsymCipher) + "/" + CAbstractKey::Algorithm2Str(eAsymMode) + "/" + CAbstractKey::Algorithm2Str(eAsymHash);
	Variant->At("VS") = Signature;
}

bool CVariant::Verify(CPublicKey* pPubKey) const
{
	const SVariant* Variant = Val();

	if(!Variant->Has("VS"))
		return false; // unsigned

	UINT eAlgorithm = 0;
	if(Variant->Has("SS"))
	{
		vector<string> SS = SplitStr(Variant->At("SS"), "/");
		if(SS.size() < 2)
			throw CException(LOG_ERROR | LOG_DEBUG, L"invalid Signature scheme");
		UINT eAsymCipher = CAbstractKey::Str2Algorithm(SS.at(0)) & CAbstractKey::eAsymCipher;
		if(eAsymCipher != pPubKey->GetAlgorithm())
			throw CException(LOG_ERROR | LOG_DEBUG, L"incompatible Signature scheme");
		UINT eAsymMode = CAbstractKey::Str2Algorithm(SS.at(1)) & CAbstractKey::eAsymMode;
		UINT eAsymHash = 0;
		if(SS.size() >= 3)
		{
			eAsymHash = CAbstractKey::Str2Algorithm(SS.at(2)) & CAbstractKey::eHashFunkt;
			if(eAsymHash == CAbstractKey::eNone)
				throw CException(LOG_ERROR | LOG_DEBUG, L"incompatible Signature hash");
		}
		eAlgorithm = eAsymMode | eAsymHash;
	}

	CBuffer Packet;
	Variant->At("").ToPacket(&Packet);
	CBuffer Signature = Variant->At("VS");
	return pPubKey->Verify(&Packet, &Signature, eAlgorithm);
}

void CVariant::Hash(CHashFunction* pHash)
{
	Extend();
	SVariant* Variant = Val();

	CBuffer Packet;
	Variant->At("").ToPacket(&Packet);
	pHash->Add(&Packet);
	pHash->Finish();
	CVariant Hash(pHash->GetKey(), pHash->GetSize());

	Variant->At("HV") = Hash;
}

bool CVariant::Test(CHashFunction* pHash) const
{
	const SVariant* Variant = Val();

	if(!Variant->Has("HV"))
		return false; // unsigned

	CBuffer Packet;
	Variant->At("").ToPacket(&Packet);
	pHash->Add(&Packet);
	pHash->Finish();
	CVariant Hash(pHash->GetKey(), pHash->GetSize());

	return Variant->At("HV") == Hash;
}

CVariant CVariant::GetFP(UINT eAlgorithm) const
{
	if(!eAlgorithm)
		eAlgorithm = CHashFunction::eSHA256;

	CBuffer Buffer;
	Val()->MkPayload(Buffer);

	// Note: we odul like to use a as simple as possible fingerprinting hash function
	//		but a smart adversarry will create spoofed paylaod matching a simple hash result
	//		so even its for internal use only we must use here a proper cryptograhinic hash function
	CHashFunction HashFx(eAlgorithm);
	HashFx.Add(&Buffer);
	HashFx.Finish();
	return CVariant(HashFx.GetKey(), HashFx.GetSize());
}

void CVariant::Encrypt(CPublicKey* pPubKey, UINT eAlgorithm, size_t KeySize)
{
	Extend();
	SVariant* Variant = Val();

	UINT eAsymCipher = pPubKey->GetAlgorithm();
	UINT eAsymMode = eAlgorithm & CAbstractKey::eAsymMode;
	if(eAsymMode == 0)
		eAsymMode = CPublicKey::GetDefaultES(eAsymCipher);
	UINT eAsymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eAsymHash == 0)
		eAsymHash = CPublicKey::eSHA1;

	if(KeySize == 0)
		KeySize = KEY_128BIT;
	CBuffer SymKey(CAbstractKey::GenerateRandomKey(KeySize), KeySize);
	CBuffer CryptKey;
	if(!pPubKey->Encrypt(&SymKey, &CryptKey, eAsymMode | eAsymHash))
		throw CException(LOG_ERROR | LOG_DEBUG, L"Not an publi key for encryption");
	if(eAlgorithm == 0)
		Variant->At("ES") = ""; // we must ad this field always to makr its encrypted
	else
		Variant->At("ES") = CAbstractKey::Algorithm2Str(eAsymCipher) + "/" + CAbstractKey::Algorithm2Str(eAsymMode) + "/" + CAbstractKey::Algorithm2Str(eAsymHash);
	Variant->At("VK") = CryptKey;

	CAbstractKey Key(SymKey.GetBuffer(), SymKey.GetSize(), eAlgorithm & (CAbstractKey::eSymCipher | CAbstractKey::eSymMode | CAbstractKey::eHashFunkt));
	CAbstractKey IV(CryptKey.GetBuffer(), CryptKey.GetSize());
	return Encrypt(&Key, &IV);
}

bool CVariant::Decrypt(CPrivateKey* pPrivKey)
{
	SVariant* Variant = Val();

	UINT eAlgorithm = 0;
	if(Variant->Has("ES"))
	{
		vector<string> ES = SplitStr(Variant->At("ES"), "/");
		if(!ES.empty())
		{
			if(ES.size() < 2)
				throw CException(LOG_ERROR | LOG_DEBUG, L"invalid Signature scheme");
			UINT eAsymCipher = CAbstractKey::Str2Algorithm(ES.at(0)) & CAbstractKey::eAsymCipher;
			if(eAsymCipher != pPrivKey->GetAlgorithm())
				throw CException(LOG_ERROR | LOG_DEBUG, L"incompatible Signature scheme");
			UINT eAsymMode = CAbstractKey::Str2Algorithm(ES.at(1)) & CAbstractKey::eAsymMode;
			UINT eAsymHash = 0;
			if(ES.size() >= 3)
			{
				eAsymHash = CAbstractKey::Str2Algorithm(ES.at(2)) & CAbstractKey::eHashFunkt;
				if(eAsymHash == CAbstractKey::eNone)
					throw CException(LOG_ERROR | LOG_DEBUG, L"incompatible Signature hash");
			}
			eAlgorithm = eAsymMode | eAsymHash;
		}
	}

	CBuffer CryptKey = Variant->At("VK");
	CBuffer SymKey;
	if(!pPrivKey->Decrypt(&CryptKey, &SymKey, eAlgorithm))
		throw CException(LOG_ERROR | LOG_DEBUG, L"decryption failed");

	CAbstractKey Key(SymKey.GetBuffer(), SymKey.GetSize());
	CAbstractKey IV(CryptKey.GetBuffer(), CryptKey.GetSize());
	if(!Decrypt(&Key, &IV))
		return false;
	
	Variant->Remove("ES");
	Variant->Remove("VK");
	return true;
}

void CVariant::Encrypt(CAbstractKey* pSymKey, CAbstractKey* pIV)
{
	Extend();
	SVariant* Variant = Val();

	UINT eAlgorithm = pSymKey->GetAlgorithm();

	UINT eSymCipher = eAlgorithm & CAbstractKey::eSymCipher;
	if(eSymCipher == 0)
		eSymCipher = CPublicKey::eAES;
	UINT eSymMode = eAlgorithm & CAbstractKey::eSymMode;
	if(eSymMode == 0)
		eSymMode = CPublicKey::eCFB;
	UINT eSymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eSymHash == 0)
		eSymHash = CPublicKey::eSHA256;

	CScoped<CEncryptionKey> pKey = CEncryptionKey::Make(eSymCipher | eSymMode | eSymHash);
	if(!pKey)
		throw CException(LOG_ERROR | LOG_DEBUG, L"invalid symetric alghorytmus");
	if(!pKey->SetKey(pSymKey->GetKey(), pSymKey->GetSize()))
		throw CException(LOG_ERROR | LOG_DEBUG, L"Key schedulign error");

	byte* IV = NULL;
	size_t IVSize = 0;
	CScoped<CBuffer> pTmp = NULL;
	if(pIV)
	{
		IV = pIV->GetKey();
		IVSize = pIV->GetSize();
	}
	else if(IVSize = pKey->GetIVSize())
	{
		pTmp = new CBuffer(CAbstractKey::GenerateRandomKey(IVSize), IVSize);
		IV = pTmp->GetBuffer();
	}
	if(!pKey->Setup(IV, IVSize))
		throw CException(LOG_ERROR | LOG_DEBUG, L"crypto initialisation error");

	CBuffer Packet;
	Variant->At("").ToPacket(&Packet);
	pKey->Process(&Packet);
	Variant->At("").Clear();
	Variant->At("") = Packet;

	if(eAlgorithm == 0)
		Variant->At("SC") = ""; // we must ad this field always to makr its encrypted
	else
		Variant->At("SC") = CAbstractKey::Algorithm2Str(eSymCipher) + "/" + CAbstractKey::Algorithm2Str(eSymMode) + "/" + CAbstractKey::Algorithm2Str(eSymHash);

	switch(eSymMode & CAbstractKey::eSymMode)
	{
		case CAbstractKey::eCTS:
		{
			if(CAbstractKey* pStolenIV = pKey->GetStolenIV())
			{
				delete pTmp.Detache();
				pTmp = new CBuffer(pStolenIV->GetKey(), pStolenIV->GetSize(), true); // the IV is cached in pKey
			}
			break;
		}
		case CAbstractKey::eEAX:
		case CAbstractKey::eGCM:
		case CAbstractKey::eCCM:
		{
			CAbstractKey* pMac = pKey->Finish();
			Variant->At("AC") = CVariant(pMac->GetKey(), pMac->GetSize());
			break;
		}
	}

	if(pTmp)
		Variant->At("IV") = *pTmp;
}

bool CVariant::Decrypt(CAbstractKey* pSymKey, CAbstractKey* pIV)
{
	SVariant* Variant = Val();

	UINT eAlgorithm = pSymKey->GetAlgorithm();

	UINT eSymCipher = eAlgorithm & CAbstractKey::eSymCipher;
	if(eSymCipher == 0)
		eSymCipher = CPublicKey::eAES;
	UINT eSymMode = eAlgorithm & CAbstractKey::eSymMode;
	if(eSymMode == 0)
		eSymMode = CPublicKey::eCFB;
	UINT eSymHash = eAlgorithm & CAbstractKey::eHashFunkt;
	if(eSymHash == 0)
		eSymHash = CPublicKey::eSHA256;

	if(Variant->Has("SC"))
	{
		vector<string> SC = SplitStr(Variant->At("SC"), "/", false);
		if(!SC.empty())
		{
			eSymCipher = CAbstractKey::Str2Algorithm(SC.at(0)) & CAbstractKey::eSymCipher;
			if(SC.size() >= 2)
				eSymMode = CAbstractKey::Str2Algorithm(SC.at(1)) & CAbstractKey::eSymMode;
			if(SC.size() >= 3)
			{
				eSymHash = CAbstractKey::Str2Algorithm(SC.at(2)) & CAbstractKey::eHashFunkt;
				if(eSymHash == CAbstractKey::eNone)
					throw CException(LOG_ERROR | LOG_DEBUG, L"incompatible key adjustement hash");
			}
		}
	}

	CScoped<CDecryptionKey> pKey = CDecryptionKey::Make(eSymCipher | eSymMode | eSymHash);
	if(!pKey)
		throw CException(LOG_ERROR | LOG_DEBUG, L"invalid symetric alghorytmus");
	if(!pKey->SetKey(pSymKey->GetKey(), pSymKey->GetSize()))
		throw CException(LOG_ERROR | LOG_DEBUG, L"Key schedulign error");

	byte* IV = NULL;
	size_t IVSize = 0;
	CScoped<CBuffer> pTmp = NULL;
	if(Variant->Has("IV"))
	{
		IV = Variant->At("IV").GetData();
		IVSize = Variant->At("IV").GetSize();
	}
	else if(pIV)
	{
		IV = pIV->GetKey();
		IVSize = pIV->GetSize();
	}
	if(!pKey->Setup(IV, IVSize))
		throw CException(LOG_ERROR | LOG_DEBUG, L"crypto initialisation error");

	CBuffer Packet = Variant->At("");
	pKey->Process(&Packet);
	Packet.SetPosition(0);
	Variant->At("").FromPacket(&Packet);

	if(Variant->Has("AC"))
	{
		CScoped<CAbstractKey> pMac = new CAbstractKey(Variant->At("AC").GetData(), Variant->At("AC").GetSize());
		if(!pKey->Verify(pMac))
			return false;
	}

	Variant->Remove("SC");
	Variant->Remove("IV");
	Variant->Remove("AC");
	return true;
}

CVariant::EEncryption CVariant::IsEncrypted() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
	{
		if(Variant->Has("ES")) // variant key - PK Encrypted
			return eAsymmetric;
		if(Variant->Has("SC")) // initialisation vector - Sym Encrypted
			return eSymmetric;
	}
	return ePlaintext;
}

bool CVariant::IsSigned() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->Has("VS"); // variant signature
	return false;
}

CVariant::EType CVariant::GetType() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").GetType();
	return Variant->Type;
}

bool CVariant::IsValid() const
{
	const SVariant* Variant = Val();
	if(Variant->Type == EExtended)
		return Variant->At("").IsValid();
	return Variant->Type != EInvalid;
}

///////////////////////////////////////////////////////////////////////////////////////////
// Internal Stuff

CVariant::SVariant::SVariant()
{
	Type = EInvalid;
	Size = 0;
	Payload = NULL;
	Access = eReadWrite;
	Refs = 0;
	Container.Void = NULL;
}

CVariant::SVariant::~SVariant()
{
	switch(Type)
	{
		case EExtended:
		case EMap:		delete Container.Map; break;
		case EList:		delete Container.List; break;
	}

	if(Access != eDerived)
		delete Payload;
}

void CVariant::SVariant::Init(EType type, size_t size, const void* payload, bool bTake)
{
	ASSERT(Access == eReadWrite);
	ASSERT(Type == EInvalid);
	ASSERT(size < 0xFFFFFFFF); // Note: one single variant can not be bigegr than 4 GB

	Type = type;
	Size = (uint32)size;
	if(bTake)
		Payload = (byte*)payload;
	else if(Size)
	{
		Payload = new byte[Size];
		if(payload)
			memcpy(Payload, payload, Size);
		else
			memset(Payload, 0, Size);
	}
	else
		Payload = NULL;

	switch(Type)
	{
		case EExtended:
		case EMap:		Container.Map = new map<string, CVariant>;	break;
		case EList:		Container.List = new vector<CVariant>;		break;
	}
}

map<string, CVariant>* CVariant::SVariant::Map() const
{
	if(Container.Map == NULL)
	{
		CBuffer Packet(Payload, Size, true);
		Container.Map = new map<string, CVariant>;
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			uint8 Len = Packet.ReadValue<uint8>();
			if(Packet.GetSizeLeft() < Len)
				throw CException(LOG_ERROR | LOG_DEBUG, L"invalid variant name");
			string Name = string((char*)Packet.ReadData(Len), Len);

			CVariant Temp;
			Temp.FromPacket(&Packet,true);

			// Note: this wil fail if the entry is already in the map
			Container.Map->insert(map<string, CVariant>::value_type(Name.c_str(), Temp));
		}
	}
	return Container.Map;
}

vector<CVariant>* CVariant::SVariant::List() const
{
	if(Container.List == NULL)
	{
		CBuffer Packet(Payload, Size, true);
		Container.List = new vector<CVariant>;
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			CVariant Temp;
			Temp.FromPacket(&Packet,true);

			Container.List->push_back(Temp);
		}
	}
	return Container.List;
}

void CVariant::SVariant::MkPayload(CBuffer& Buffer) const
{
	if(Access == eReadWrite) // write the packet
	{
		switch(Type)
		{
			case EExtended:
			case EMap:
			{
				for(uint32 i=0; i<Count(); i++)
				{
					const string& sKey = Key(i);
					ASSERT(sKey.length() < 0xFF);
					uint8 Len = (uint8)sKey.length();
					Buffer.WriteValue<uint8>(Len);
					Buffer.WriteData(sKey.c_str(), Len);
					At(i).ToPacket(&Buffer);
				}
				break;
			}
			case EList:
			{
				for(uint32 i=0; i<Count();i++)
				{
					At(i).ToPacket(&Buffer);
				}
				break;
			}
			default:
				Buffer.SetBuffer(Payload, Size, true);
		}
	}
	else
		Buffer.SetBuffer(Payload, Size, true);
}


CVariant::SVariant* CVariant::SVariant::Clone(bool Full) const
{
	SVariant* Variant = new SVariant;
	Variant->Type = Type;
	switch(Type)
	{
		case EExtended:
		case EMap:
			Variant->Container.Map = new map<string, CVariant>;
			for(uint32 i=0; i < Count(); i++)
				Variant->Insert(Key(i).c_str(), Full ? At(i).Clone() : At(i));
			break;
		case EList:
			Variant->Container.List = new vector<CVariant>;
			for(uint32 i=0; i < Count(); i++)
				Variant->Append(Full ? At(i).Clone() : At(i));
			break;
		default:
			if(Size > 0)
			{
				Variant->Size = Size;
				Variant->Payload = new byte[Variant->Size];
				memcpy(Variant->Payload, Payload, Variant->Size);
			}
	}
	return Variant;
}

uint32 CVariant::SVariant::Count() const
{
	switch(Type)
	{
		case EExtended:
		case EMap:		return (uint32)Map()->size();
		case EList:		return (uint32)List()->size();
		default:		return 0;
	}
}

const string& CVariant::SVariant::Key(uint32 Index) const
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			if(Index >= pMap->size())
				throw CException(LOG_ERROR | LOG_DEBUG, L"Index out of bound");

			map<string, CVariant>::iterator I = pMap->begin();
			while(Index--)
				I++;
			return I->first;
		}
		case EList:
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::Insert(const char* Name, const CVariant& Variant)
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			pair<map<string, CVariant>::iterator, bool> Ret = Map()->insert(map<string, CVariant>::value_type(Name, Variant));
			if(!Ret.second)
				Ret.first->second.Attach(Variant.m_Variant);
			return Ret.first->second;
		}
		case EList:
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::At(const char* Name) const
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			map<string, CVariant>::iterator I = pMap->find(Name);
			if(I == pMap->end())
				I = pMap->insert(map<string, CVariant>::value_type(Name, CVariant())).first;
			return I->second;
		}
		case EList:
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a Map Variant");
	}
}

bool CVariant::SVariant::Has(const char* Name) const
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			return pMap->find(Name) != pMap->end();
		}
		case EList:
		default:		return false;
	}
}

void CVariant::SVariant::Remove(const char* Name)
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			map<string, CVariant>::iterator I = pMap->find(Name);
			if(I != pMap->end())
				pMap->erase(I);
			break;
		}
		case EList:
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::Append(const CVariant& Variant)
{
	switch(Type)
	{
		case EList:
		{
			vector<CVariant>* pList = List();
			pList->push_back(Variant);
			return pList->back();
		}
		case EExtended:
		case EMap:
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a List Variant");
	}
}

CVariant& CVariant::SVariant::At(uint32 Index) const
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			if(Index >= pMap->size())
				throw CException(LOG_ERROR | LOG_DEBUG, L"Index out of bound");
			map<string, CVariant>::iterator I = pMap->begin();
			while(Index--)
				I++;
			return I->second;
		}
		case EList:
		{
			vector<CVariant>* pList = List();
			if(Index >= pList->size())
				throw CException(LOG_ERROR | LOG_DEBUG, L"Index out of bound");
			return (*pList)[Index];
		}
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a List Variant");
	}
}

void CVariant::SVariant::Remove(uint32 Index)
{
	switch(Type)
	{
		case EExtended:
		case EMap:
		{
			map<string, CVariant>* pMap = Map();
			if(Index >= pMap->size())
				throw CException(LOG_ERROR | LOG_DEBUG, L"Index out of bound");
			map<string, CVariant>::iterator I = pMap->begin();
			while(Index--)
				I++;
			pMap->erase(I);
		}
		case EList:
		{
			vector<CVariant>* pList = List();
			if(Index >= pList->size())
				throw CException(LOG_ERROR | LOG_DEBUG, L"Index out of bound");
			vector<CVariant>::iterator I = pList->begin();
			while(Index--)
				I++;
			pList->erase(I);
		}
		default:		throw CException(LOG_ERROR | LOG_DEBUG, L"Not a List Variant");
	}
}

int	CVariant::Compare(const CVariant &R) const
{
	// we compare only actual payload, content not the declared type or auxyliary informations
	CBuffer l;
	Val()->MkPayload(l);
	CBuffer r;
	R.Val()->MkPayload(r);
	return l.Compare(r); 
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Streaming

void MakePacket(const string& Name, const CVariant& Packet, CBuffer& Buffer)
{
	// Note: the transport layer should ensure the packets arive uncorrupted, but 
	bool bCheckSumm = false;  // to test the system its usefull to have a optional checksumm
	ASSERT(Name.length() < 0xFF);
	uint8 Len = (uint8)Name.length();
	ASSERT(Len < 0x7F);
	if(bCheckSumm)
		Len |= 0x80;
	size_t uPos = Buffer.GetPosition();
	Buffer.WriteValue<uint8>(Len);							// [uint8 - NameLen]
	Buffer.WriteData((byte*)Name.c_str(), Name.length());	// [bytes - Name]
	Packet.ToPacket(&Buffer);								// [uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
	if(bCheckSumm)
	{
		size_t uLen = Buffer.GetPosition() - uPos;
		CHashFunction Hash(CHashFunction::eCRC);
		Hash.Add(Buffer.GetData(uPos, uLen), uLen);
		Hash.Finish();
		ASSERT(Hash.GetSize() == 4);
		Buffer.WriteData(Hash.GetKey(), 4);					// [uint32 - CRC CheckSumm]
	}
}

bool StreamPacket(CBuffer& Buffer, string& Name, CVariant& Packet)
{
	// [uint8 - NameLen][bytes - Name][uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]([uint32 - CRC CheckSumm])

	Buffer.SetPosition(0);

	uint8 Len = *((uint8*)Buffer.GetData(sizeof(uint8)));
	bool bCheckSumm = (Len & 0x80) != 0;
	Len &= 0x7F;
	char* pName = (char*)Buffer.GetData(Len);
	if(!pName)
		return false; // incomplete header

	uint8* pType = ((uint8*)Buffer.GetData(sizeof(uint8) + Len, sizeof(uint8)));
	if(pType == NULL)
		return false; // incomplete header

	void* pSize; // = (uint32*)Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));
	if((*pType & CVariant::ELen32) == CVariant::ELen8)
		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint8));
	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint16));
	else
		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));

	if(pSize == NULL)
		return false; // incomplete header

	size_t Size; // = sizeof(uint8) + sizeof(uint32) + *pSize;
	if((*pType & CVariant::ELen32) == CVariant::ELen8)
		Size = sizeof(uint8) + sizeof(uint8) + *((uint8*)pSize);
	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
		Size = sizeof(uint8) + sizeof(uint16) + *((uint16*)pSize);
	else
		Size = sizeof(uint8) + sizeof(uint32) + *((uint32*)pSize);

	if(Buffer.GetSizeLeft() < (bCheckSumm ? Size + 4 : Size))
		return false; // incomplete data

	if(bCheckSumm)
	{
		size_t uLen = sizeof(uint8) + Len + Size;
		CHashFunction Hash(CHashFunction::eCRC);
		Hash.Add(Buffer.GetData(0, uLen), uLen);
		Hash.Finish();
		ASSERT(Hash.GetSize() == 4);
		if(memcmp(Hash.GetKey(), Buffer.GetData(uLen, 4), 4) != 0)
			throw CException(LOG_ERROR, L"Packet CRC Invalid");

		Size += 4;
	}

	Name = string(pName, Len);
	Packet.FromPacket(&Buffer);
	Buffer.ShiftData(sizeof(uint8) + Len + Size);

	return true;
}
