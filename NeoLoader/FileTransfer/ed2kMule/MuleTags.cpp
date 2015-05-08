#include "GlobalHeader.h"
#include "MuleTags.h"
#include "../../../Framework/Buffer.h"
#include "../../FileList/File.h"
#include "../../../Framework/Scope.h"
#include "../../FileList/Hashing/FileHashSet.h"
#include "../../FileList/Hashing/FileHashTree.h"
#include "../../../Framework/Exception.h"

int CMuleHash::m_MyType = qRegisterMetaType<CMuleHash>("CMuleHash");

QVariantMap CMuleTags::ReadTags(const CBuffer* Data, bool bShort)
{
	QVariantMap Tags;
	uint32 uCount = bShort ? Data->ReadValue<uint8>() : Data->ReadValue<uint32>();
	for(uint32 i=0; i < uCount; i++)
	{
		QString Name;
		QVariant Value;

		uint8 uType = Data->ReadValue<uint8>();
		if (uType & 0x80)
		{
			uType &= 0x7F;
			Name = QString::number(Data->ReadValue<uint8>());
		} 
		else 
		{
			size_t uRawLength = Data->ReadValue<uint16>();
			if(uRawLength == 1)
				Name = QString::number(Data->ReadValue<uint8>());
			else
				Name = QString::fromStdWString(Data->ReadString(CBuffer::eAscii, uRawLength));
		}
	
		// Comment copyed form eMule (Fair Use):
		// NOTE: It's very important that we read the *entire* packet data,
		// even if we do not use each tag. Otherwise we will get in trouble
		// when the packets are returned in a list - like the search results
		// from a server. If we cannot do this, then we throw an exception.
		switch (uType) 
		{
			case TAGTYPE_STRING:
				Value = QString::fromStdWString(Data->ReadString());
				ASSERT(Value.type() == QVariant::String);
				break;
			
			case TAGTYPE_UINT32:
				Value = Data->ReadValue<uint32>();
				ASSERT(Value.type() == QVariant::UInt);
				break;

			case TAGTYPE_UINT64:
				Value = Data->ReadValue<uint64>();
				ASSERT(Value.type() == QVariant::ULongLong);
				break;
			
			case TAGTYPE_UINT16:
				Value = (uint32)Data->ReadValue<uint16>();
				ASSERT(Value.type() == QVariant::UInt);
				//uType = TAGTYPE_UINT32;
				break;
			
			case TAGTYPE_UINT8:
				Value = (uint32)Data->ReadValue<uint8>();
				ASSERT(Value.type() == QVariant::UInt);
				//uType = TAGTYPE_UINT32;
				break;
			
			case TAGTYPE_FLOAT32:
				Value = *((float*)Data->ReadData(4));
				ASSERT(Value.type() == QVariant::Double);
				break;
			
			case TAGTYPE_HASH16:
			{
				CMuleHash Hash(Data->ReadData(CMuleHash::GetSize()));
				Value = QVariant(CMuleHash::GetVariantType(), &Hash);
				break;
			}
			case TAGTYPE_BOOL:
				Value = (bool)Data->ReadValue<uint8>();
				ASSERT(Value.type() == QVariant::Bool);
				break;
			
			case TAGTYPE_BOOLARRAY:
			{
				uint16 uLen = Data->ReadValue<uint16>();

				Data->SetPosition(Data->GetPosition() + (uLen / 8) + 1);
				break;
			}
	
			case TAGTYPE_BLOB:
			{
				size_t uSize = Data->ReadValue<uint32>();
				
				// Since the length is 32b, this check is needed to avoid huge allocations in case of bad tags.
				if (uSize > Data->GetSize() - Data->GetPosition())
					throw CException(LOG_ERROR | LOG_DEBUG, L"Malformed tag");

				Value = QByteArray((char*)Data->ReadData(uSize), (int)uSize);
				ASSERT(Value.type() == QVariant::ByteArray);
				break;
			}
			default:
				if (uType >= TAGTYPE_STR1 && uType <= TAGTYPE_STR16) 
				{
					uint8 uLength = uType - TAGTYPE_STR1 + 1;
					Value = QString::fromStdWString(Data->ReadString(CBuffer::eUtf8, uLength));
					ASSERT(Value.type() == QVariant::String);
					//uType = TAGTYPE_STRING;
				} 
				else
					throw CException(LOG_ERROR | LOG_DEBUG, L"Unknown tag type encounted %s, cannot proceed!", Name.toStdWString().c_str());
		}

		Tags.insert(Name, Value);
	}
	return Tags;
}

uint8 CMuleTags::GetMuleTagType(const QVariant& Value)
{
	QVariant::Type Type = Value.type();
	switch(Type)
	{
		case QVariant::String:		
			return TAGTYPE_STRING;
		case QVariant::Int:
		case QVariant::UInt:
			return TAGTYPE_UINT32;
		case QVariant::LongLong:
		case QVariant::ULongLong:	
			return TAGTYPE_UINT64;
		case QVariant::Double:		
			return TAGTYPE_FLOAT32;
		case QVariant::Bool:		
			return TAGTYPE_BOOL;
		case QVariant::ByteArray:	
			return TAGTYPE_BLOB;
		case QVariant::UserType:
			if(Value.userType() == CMuleHash::GetVariantType())
				return TAGTYPE_HASH16;
		default:
			ASSERT(0);
			return 0;
	}
}

void CMuleTags::WriteTags(const QVariantMap& Tags, CBuffer* Data, bool bAllowNew, bool bShort)
{
	if(bShort)
		Data->WriteValue<uint8>(Tags.size());
	else
		Data->WriteValue<uint32>(Tags.size());

	for(QVariantMap::const_iterator I = Tags.begin(); I != Tags.end(); ++I)
	{
		const QString& Name = I.key();
		QVariant Value = I.value();

		uint8 uType = GetMuleTagType(Value);

		if(bAllowNew)
		{
			if(uType == TAGTYPE_UINT32)
			{
				uint32 uValue = Value.toUInt();
				if (uValue <= 0xFF)
					uType = TAGTYPE_UINT8;
				else if (uValue <= 0xFFFF)
					uType = TAGTYPE_UINT16;
				//else if (uValue <= 0xFFFFFFFF)
				//	uType = TAGTYPE_UINT32;
			}
			//else if(uType == TAGTYPE_STRING)
			//{
			//}
		}
		else{
			ASSERT((uType == TAGTYPE_HASH16 || uType == TAGTYPE_STRING || uType == TAGTYPE_UINT32 || uType == TAGTYPE_FLOAT32 || uType == TAGTYPE_BLOB || uType == TAGTYPE_UINT64));}

		bool bNummeric = false;
		uint32 uName = Name.toUInt(&bNummeric);
		if(uName > 0xFF)
			bNummeric = false;
		
		if (bNummeric)
		{
			if(bAllowNew)
			{
				Data->WriteValue<uint8>(uType | 0x80);
				Data->WriteValue<uint8>(uName);
			}
			else
			{
				Data->WriteValue<uint8>(uType);
				Data->WriteValue<uint16>(1);
				Data->WriteValue<uint8>(uName);
			}
		} 
		else
		{
			Data->WriteValue<uint8>(uType);
			Data->WriteString(Name.toStdWString(), CBuffer::eAscii, CBuffer::e16Bit);
		}
		
		switch (uType)
		{
			case TAGTYPE_HASH16:
			{
				CMuleHash Hash = Value.value<CMuleHash>();
				Data->WriteData(Hash.GetData(),16);
				break;
			}
			case TAGTYPE_STRING:
				Data->WriteString(Value.toString().toStdWString(), CBuffer::eUtf8);
				break;
			case TAGTYPE_UINT64:
				Data->WriteValue<uint64>(Value.toULongLong());
				break;
			case TAGTYPE_UINT32:
				Data->WriteValue<uint32>(Value.toUInt());
				break;
			case TAGTYPE_FLOAT32:
			{
				float Float = Value.toDouble();
				Data->WriteData((byte*)&Float, 4);
				break;
			}
			case TAGTYPE_UINT16:
				Data->WriteValue<uint16>(Value.toUInt());
				break;
			case TAGTYPE_UINT8:
				Data->WriteValue<uint8>(Value.toUInt());
				break;
			case TAGTYPE_BLOB:
			{
				QByteArray blob = Value.toByteArray();
				Data->WriteValue<uint32>(blob.size());
				Data->WriteData(blob.data(), blob.size());
				break;
			}
			default:
				ASSERT(0);
				break;
		}	
	}
}

// Comment copyed form aMule (Fair Use):
// UInt128 values are stored a little weird way...
// Four little-endian 32-bit numbers, stored in
// big-endian order
void CMuleTags::WriteUInt128(const QByteArray& uUInt128, CBuffer* Data)
{
	uint32 data[4] = {0,0,0,0};
	for (int iIndex=0; iIndex<16; iIndex++)
		data[iIndex/4] |= (uint32)((byte*)uUInt128.data())[iIndex] << (8*(3-(iIndex%4)));
	for (int i=0; i<4; i++)
		Data->WriteValue<uint32>(data[i]);
}

QByteArray CMuleTags::ReadUInt128(CBuffer* Data)
{
	uint32 data[4];
	for (int i=0; i<4; i++)
		data[i] = Data->ReadValue<uint32>();
	QByteArray uUInt128('0',16);
	for (int iIndex=0; iIndex<16; iIndex++)
		((byte*)uUInt128.data())[iIndex] = (byte)(data[iIndex/4] >> (8*(3-(iIndex%4))));
	return uUInt128;
}

void CMuleTags::WriteHashSet(CFileHashEx* pHashSet, CBuffer* Data)
{
	Data->WriteQData(pHashSet->GetHash());
	QList<QByteArray> HashSet = pHashSet->GetHashSet();
	uint32 uHashCount = HashSet.size();
	Data->WriteValue<uint16>(uHashCount);
	for (uint32 i = 0; i < uHashCount; i++)
		Data->WriteQData(HashSet.at(i));
}

bool CMuleTags::ReadHashSet(const CBuffer* Data, CFileHashEx* pHashSet, QByteArray Hash)
{
	if(Hash.isEmpty())
		Hash = Data->ReadQData(pHashSet->GetSize());	
	QList<QByteArray> HashSet;
	uint32 uHashCount = Data->ReadValue<uint16>();
	for (uint32 i = 0; i < uHashCount; i++)
		HashSet.append(Data->ReadQData(pHashSet->GetSize()));
	if(Hash != pHashSet->GetHash())
		return false;
	if(pHashSet->CanHashParts())
		return true; // already completed
	return pHashSet->SetHashSet(HashSet);
}

void CMuleTags::WriteAICHRecovery(CFileHashTree* pBranche, CBuffer* Data)
{
	SHashTreeDump Leafs(CFileHash::GetSize(HashMule));
	pBranche->Save(Leafs);

	/* Comment copyed form eMule (Fair Use):
		V2 AICH Hash Packet:
		<count1 uint16>											16bit-hashs-to-read
		(<identifier uint16><hash HASHSIZE>)[count1]			AICH hashs
		<count2 uint16>											32bit-hashs-to-read
		(<identifier uint32><hash HASHSIZE>)[count2]			AICH hashs
	*/
	bool bI64 = IsLargeEd2kMuleFile(pBranche->GetTotalSize()); // is it a large file?
	if (bI64)
		Data->WriteValue<uint16>(0); // no 16bit hashs to write
	Data->WriteValue<uint16>(Leafs.Count());
	for(int Index = 0; Index < Leafs.Count(); Index++)
	{
		if(bI64)
			Data->WriteValue<uint32>(Leafs.ID(Index));
		else
			Data->WriteValue<uint16>(Leafs.ID(Index));
		Data->WriteData(Leafs.Hash(Index), Leafs.Size());
	}
	if (!bI64)
		Data->WriteValue<uint16>(0); // no 32bit hashs to write
}

void CMuleTags::ReadAICHRecovery(const CBuffer* Data, CFileHashTree* pBranche)
{
	bool bI64 = false;
	uint32 uHashCount = Data->ReadValue<uint16>();
	if(uHashCount == 0)
	{
		bI64 = true;
		uHashCount = Data->ReadValue<uint16>();
	}
	SHashTreeDump Leafs(CFileHash::GetSize(HashMule), uHashCount);
	for(uint32 i = 0; i < uHashCount; i++)
	{
		uint32 TreePath = bI64 ? Data->ReadValue<uint32>() : Data->ReadValue<uint16>();
		Leafs.Add(TreePath, Data->ReadData(Leafs.Size()));
	}
	pBranche->Load(Leafs);
	if (!bI64)
		Data->ReadValue<uint16>();
}

void CMuleTags::WriteIdentifier(CFile* pFile, CBuffer* Data)
{
	CFileHash* pEd2kHash = pFile->GetHash(HashEd2k);
	uint64 uFileSize = pFile->GetFileSize();
	CFileHash* pAICHHash = pFile->GetHash(HashMule);

	UMuleIdentOpt IdentOpt;
	IdentOpt.Fields.uIncludesMD4		= pEd2kHash ? 1 : 0;
	IdentOpt.Fields.uIncludesSize		= uFileSize ? 1 : 0;
	IdentOpt.Fields.uIncludesAICH		= pAICHHash ? 1 : 0;
	IdentOpt.Fields.uMandatoryOptions	= 0; // RESERVED - Identifier invalid if unknown options set
	IdentOpt.Fields.uOptions			= 0; // RESERVED
	
	Data->WriteValue<uint8>(IdentOpt.Bits);
	if(pEd2kHash)
		Data->WriteQData(pEd2kHash->GetHash());
	if(uFileSize)
		Data->WriteValue<uint64>(uFileSize);
	if(pAICHHash)
		Data->WriteQData(pAICHHash->GetHash());
}

QMap<EFileHashType,CFileHashPtr> CMuleTags::ReadIdentifier(const CBuffer* Data, uint64 &uFileSize)
{
	UMuleIdentOpt IdentOpt;
	IdentOpt.Bits = Data->ReadValue<uint8>();

	QMap<EFileHashType,CFileHashPtr> HashMap;
	if(IdentOpt.Fields.uIncludesMD4)
	{
		CScoped<CFileHash> pEd2kHash = new CFileHash(HashEd2k); // a read error will throw, don't loose memory
		pEd2kHash->SetHash(Data->ReadQData(pEd2kHash->GetSize()));
		HashMap.insert(HashEd2k, CFileHashPtr(pEd2kHash.Detache()));
	}
	if(IdentOpt.Fields.uIncludesSize)
		uFileSize = Data->ReadValue<uint64>();
	if(IdentOpt.Fields.uIncludesAICH)
	{
		CScoped<CFileHash> pAICHHash = new CFileHash(HashMule); // a read error will throw, don't loose memory
		pAICHHash->SetHash(Data->ReadQData(pAICHHash->GetSize()));
		HashMap.insert(HashMule, CFileHashPtr(pAICHHash.Detache()));
	}
	return HashMap;
}
