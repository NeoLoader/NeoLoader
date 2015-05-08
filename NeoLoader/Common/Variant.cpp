#include "GlobalHeader.h"
#include "Variant.h"
#include "../../zlib/zlib.h"
#include "../../Framework/Exception.h"
#include "../../Framework/Cryptography/HashFunction.h"

QVariant CVariant::FromPacket(const CBuffer* pPacket)
{
	size_t Size;

	uint8 Type = pPacket->ReadValue<uint8>();
	ASSERT(Type != 0xFF);
	if((Type & ELen32) == ELen8)
		Size = pPacket->ReadValue<uint8>();
	else if((Type & ELen32) == ELen16)
		Size = pPacket->ReadValue<uint16>();
	else
		Size = pPacket->ReadValue<uint32>();
	if(pPacket->GetSizeLeft() < Size)
		throw CException(LOG_ERROR | LOG_DEBUG, L"incomplete variant");

	CBuffer Payload;

	if((Type & ELen32) == 0) // is it packed
	{
		byte* pData = pPacket->ReadData(Size);

		uLongf newsize = Size*10+300;
		uLongf unpackedsize = 0;
		int result = 0;
		CBuffer Buffer;
		do
		{
			Buffer.AllocBuffer(newsize, true);
			unpackedsize = newsize;
			result = uncompress(Buffer.GetBuffer(),&unpackedsize,pData,Size);
			newsize *= 2; // size for the next try if needed
		}
		while (result == Z_BUF_ERROR && newsize < Max(MB2B(16), Size*100));	// do not allow the unzip buffer to grow infinetly,
																				// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
		if (result == Z_OK)
		{
			Size = unpackedsize;
			Payload.SetBuffer(Buffer.GetBuffer(true), Size);
		}
		else
			return QVariant();
	}
	else
		Payload.SetBuffer(pPacket->ReadData(Size), Size, true);
	

	switch(Type & ~ELen32)
	{
		case EExtended:
		case EMap:
		{
			QVariantMap Map;
			for(size_t Pos = Payload.GetPosition(); Payload.GetPosition() - Pos < Size; )
			{
				uint8 Len = Payload.ReadValue<uint8>();
				if(Payload.GetSizeLeft() < Len)
					throw CException(LOG_ERROR | LOG_DEBUG, L"invalid variant name");
				string Name = string((char*)Payload.ReadData(Len), Len);

				Map.insert(QString::fromStdString(Name), FromPacket(&Payload));
			}
			return Map;
		}
		case EList:
		{
			QVariantList List;
			for(size_t Pos = Payload.GetPosition(); Payload.GetPosition() - Pos < Size; )
				List.append(FromPacket(&Payload));
			return List;
		}

		case EBytes:
			return Payload.ToByteArray();

		case EUtf8:
			return QString::fromUtf8(Payload.ToByteArray());
		case EAscii:
			return QString::fromLatin1(Payload.ToByteArray());

		case ESInt:
			if(Size <= sizeof(sint8))
				return Payload.ReadValue<sint8>();
			else if(Size <= sizeof(sint16))
				return Payload.ReadValue<sint16>();
			else if(Size <= sizeof(sint32))
				return Payload.ReadValue<sint32>();
			else if(Size <= sizeof(uint64))
				return Payload.ReadValue<sint64>();
			else
				return Payload.ToByteArray();
		case EUInt:
			if(Size <= sizeof(uint8))
				return Payload.ReadValue<uint8>();
			else if(Size <= sizeof(uint16))
				return Payload.ReadValue<uint16>();
			else if(Size <= sizeof(uint32))
				return Payload.ReadValue<uint32>();
			else if(Size <= sizeof(uint64))
				return Payload.ReadValue<uint64>();
			else
				return Payload.ToByteArray();

		case EDouble:
			return Payload.ReadValue<double>();

		case EInvalid:
		default:
			return QVariant();
	}
}

void CVariant::ToPacket(const QVariant& qVariant, CBuffer* pPacket, bool bPack)
{
	CBuffer Payload;
	uint8 Type = EInvalid;

	switch(qVariant.type())
	{
		case QVariant::Map:
		//case QVariant::Hash:
		{
			Type = EMap;
			QVariantMap Map = qVariant.toMap();
			for(QVariantMap::iterator I = Map.begin(); I != Map.end(); I++)
			{
				const string& sKey = I.key().toStdString();
				ASSERT(sKey.length() < 0xFF);
				uint8 Len = (uint8)sKey.length();
				Payload.WriteValue<uint8>(Len);
				Payload.WriteData(sKey.c_str(), Len);
				ToPacket(*I, &Payload);
			}
			break;
		}
		case QVariant::List:
		case QVariant::StringList:
		{
			Type = EList;
			QVariantList List = qVariant.toList();
			for(QVariantList::iterator I = List.begin(); I != List.end(); I++)
				ToPacket(*I, &Payload);
			break;
		}
		case QVariant::Bool:
		{
			Type = EUInt;
			uint8 Value = qVariant.toBool();
			Payload.WriteValue<uint8>(Value);
			break;
		}
		//case QVariant::Char:
		case QVariant::Int:
		{
			Type = ESInt;
			sint32 Value = qVariant.toInt();
			Payload.WriteValue<sint32>(Value);
			break;
		}
		case QVariant::UInt:
		{
			Type = EUInt;
			uint32 Value = qVariant.toUInt();
			Payload.WriteValue<uint32>(Value);
			break;
		}
		case QVariant::LongLong:
		{
			Type = EUInt;
			sint64 Value = qVariant.toLongLong();
			Payload.WriteValue<sint64>(Value);
			break;
		}
		case QVariant::ULongLong:
		{
			Type = EUInt;
			uint64 Value = qVariant.toULongLong();
			Payload.WriteValue<uint64>(Value);
			break;
		}
		case QVariant::Double:
		{
			Type = EDouble;
			double Value = qVariant.toDouble();
			Payload.WriteValue<double>(Value);
			break;
		}
		case QVariant::String:
		{
			Type = EUtf8;
			QByteArray Value = qVariant.toString().toUtf8();
			Payload.WriteQData(Value);
			break;
		}
		case QVariant::ByteArray:
		{
			Type = EBytes;
			QByteArray Value = qVariant.toByteArray();
			Payload.WriteQData(Value);
			break;
		}
		//case QVariant::BitArray:
		/*case QVariant::Date:
		case QVariant::Time:
		case QVariant::DateTime:
		case QVariant::Url:*/
		default:
			ASSERT(0);
	}

	ASSERT(Type != EInvalid);

	if(bPack)
	{
		ASSERT(Payload.GetSize() + 300 < 0xFFFFFFFF);
		uLongf newsize = (uLongf)(Payload.GetSize() + 300);
		CBuffer Buffer(newsize);
		int result = compress2(Buffer.GetBuffer(),&newsize,Payload.GetBuffer(),(uLongf)Payload.GetSize(),Z_BEST_COMPRESSION);

		if (result == Z_OK && Payload.GetSize() > newsize) // does the compression helped?
		{
			ASSERT(Type != 0xFF);
			pPacket->WriteValue<uint8>(Type);
			pPacket->WriteValue<uint32>(newsize);
			pPacket->WriteData(Buffer.GetBuffer(), newsize);
			return;
		}
	}

	
	size_t uLength = Payload.GetSize();
	ASSERT(uLength < 0xFFFFFFFF);

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

//////////////////////////////////////////////////////////////////////////////////////////////
// Streaming

void MakePacket(const string& Name, const QVariant& Packet, CBuffer& Buffer)
{
	bool bCheckSumm = false; // this is usefull only for debugging
	uint8 Len = (uint8)Name.length();
	ASSERT(Len < 0x7F);
	if(bCheckSumm)
		Len |= 0x80;
	size_t uPos = Buffer.GetPosition();
	Buffer.WriteValue<uint8>(Len);							// [uint8 - NameLen]
	Buffer.WriteData((byte*)Name.c_str(), Name.length());	// [bytes - Name]
	CVariant::ToPacket(Packet, &Buffer, true);				// [uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
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

bool StreamPacket(CBuffer& Buffer, string& Name, QVariant& Packet)
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

	Name = string(pName, Len);
	try
	{
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

		Packet = CVariant::FromPacket(&Buffer);
	}
	catch(const CException& Exception)
	{
		Packet = QVariant();
	}
	Buffer.ShiftData(sizeof(uint8) + Len + Size);

	return true;
}
