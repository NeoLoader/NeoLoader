#include "GlobalHeader.h"
#include "FileHash.h"
#include "FileHashTree.h"
#include "FileHashTreeEx.h"
#include "FileHashSet.h"
#include "../../../Framework/Cryptography/HashFunction.h"

#if 0

#include "../../../Framework/MEM/MicroAllocator.h"
using namespace MICRO_ALLOCATOR;

HeapManager* g_hm;
struct SMicroAlloc
{
	SMicroAlloc() { g_hm = createHeapManager(); }
	~SMicroAlloc() { releaseHeapManager(g_hm);}
} g_hash_allocator;

void* hash_malloc(size_t size)
{
	return heap_malloc(g_hm, size);
}

void hash_free(void* ptr)
{
	heap_free(g_hm, ptr);
}

#else

void* hash_malloc(size_t size)
{
	return malloc(size);
}

void hash_free(void* ptr)
{
	free(ptr);
}

#endif

CFileHash::CFileHash(EFileHashType eType)
{
	m_eType = eType;
	if(m_eType == HashUnknown)
		m_uSize = KEY_64BIT; // placeholder dummy
	else
		m_uSize = GetSize(eType);
	m_Hash.resize(m_uSize);
	SetHash(NULL);
}

CFileHash::~CFileHash()
{
	ASSERT(m_HashMutex.tryLockForWrite());
}

CFileHash* CFileHash::FromString(QByteArray String, EFileHashType eType, uint64 uFileSize)
{
	if(eType == HashNone)
	{
		QList<QByteArray> Hash = String.split(':');
		if(QString(Hash.first()).compare("urn", Qt::CaseInsensitive) == 0)
			Hash.removeFirst();
		QByteArray Type = Hash.takeFirst();
		if(QString(Type).compare("tree", Qt::CaseInsensitive) == 0 && Hash.size() >= 2)
			Type += ":" + Hash.takeFirst();
		eType = CFileHash::Str2HashType(Type);
		if(eType == CAbstractKey::eNone || Hash.size() < 1)
			return NULL;
		String = Hash.first();
	}

	return FromArray(DecodeHash(eType, String), eType, uFileSize);
}

QByteArray CFileHash::DecodeHash(EFileHashType eType, QByteArray String)
{
	int Base = GetBase(eType);
	size_t uSize = GetSize(eType);
	size_t uLength = String.length();
	if(uLength == 0)
		return QByteArray();
	else if(uLength/2 == uSize)		// 200%
		Base = 16;
	else if((uLength*5)/8 == uSize)	// 160%
		Base = 32;
	else if((uLength*3)/4 == uSize)	// 133%
		Base = 64;

	QByteArray Hash;
	switch(Base)
	{
		case 16:
			Hash = QByteArray::fromHex(String);
			break;
		case 32:
		{
			UINT nBits	= 0;
			int nCount	= 0;
			//size_t uLength = String.size();
			for ( int i=0 ; uLength-- ; i++ )
			{
				if ( String[i] >= 'A' && String[i] <= 'Z' )
					nBits |= ( String[i] - 'A' );
				else if ( String[i] >= 'a' && String[i] <= 'z' )
					nBits |= ( String[i] - 'a' );
				else if ( String[i] >= '2' && String[i] <= '7' )
					nBits |= ( String[i] - '2' + 26 );
				else
					break;
				
				nCount += 5;

				if ( nCount >= 8 )
				{
					Hash += (byte)( nBits >> ( nCount - 8 ) );
					nCount -= 8;
				}

				nBits <<= 5;
			}
			break;
		}
		case 64:
			Hash = QByteArray::fromBase64(String.replace("-","+").replace("_","/"));
			break;
		default:
			ASSERT(0);
	}

	if(Hash.size() != GetSize(eType))
		return "";

	return Hash;
}

QByteArray CFileHash::EncodeHash(EFileHashType eType, QByteArray Hash, int Base)
{
	if(!Base)
		Base = GetBase(eType);

	switch(Base)
	{
		case 16:
			return Hash.toHex();
		case 32:
		{
			byte* pHash = (byte*)Hash.data();

			QByteArray Base32Buff;
		    
			static byte base32Chars[33] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567"; // Base 32 words are choosen for maximal human readability
			unsigned int i, index;
			unsigned char word;
			for(i = 0, index = 0; i < Hash.size();) 
			{
				// Is the current word going to span a byte boundary?
				if (index > 3)
				{
					word = (byte)(pHash[i] & (0xFF >> index));
					index = (index + 5) % 8;
					word <<= index;
					if (i < Hash.size() - 1)
						word |= pHash[i + 1] >> (8 - index);

					i++;
				}
				else 
				{
					word = (byte)((pHash[i] >> (8 - (index + 5))) & 0x1F);
					index = (index + 5) % 8;
					if (index == 0)
					   i++;
				}

				Base32Buff += (char) base32Chars[word];
			}

			return Base32Buff;
		}
		case 64:
			// Note: we need URL compatible encoding: http://en.wikipedia.org/wiki/Base64#URL_applications
			return Hash.toBase64().replace("+","-").replace("/","_").replace("=","");
		default: 
			ASSERT(0);
	}
	return "";
}

CFileHash* CFileHash::FromArray(QByteArray Hash, EFileHashType eType, uint64 uFileSize)
{
	CFileHash* pHash = New(eType, uFileSize);
	if(pHash)
	{
		if(!pHash->SetHash(Hash))
		{
			delete pHash;
			pHash = NULL;
		}
	}
	return pHash;
}

CFileHash* CFileHash::New(EFileHashType eType, uint64 uFileSize)
{
	CFileHash* pHash;
	if(uFileSize == 0 || eType == HashMD5 || eType == HashSHA1 || eType == HashSHA2 || eType == HashArchive)
		pHash = new CFileHash(eType);
	else if(eType == HashNeo || eType == HashXNeo || eType == HashMule || eType == HashTigerTree)
		pHash = (eType == HashXNeo) ? new CFileHashTreeEx(eType, uFileSize) : new CFileHashTree(eType, uFileSize);
	else if(eType == HashEd2k)
		pHash = new CFileHashSet(eType, uFileSize);
	else if(eType == HashTorrent)
		pHash = new CFileHash(eType);
	else {
		ASSERT(0); // unknown hash
	}
	return pHash;
}

QByteArray CFileHash::ToString(int Base)
{
	QReadLocker Locker(&m_HashMutex);

	return EncodeHash(m_eType, m_Hash, Base);
}

size_t CFileHash::GetSize(EFileHashType eType)
{
	switch(eType)
	{
		case HashSHA2:
		case HashArchive:
		case HashNeo:
		case HashXNeo:		return KEY_256BIT;
		case HashMD5:
		case HashEd2k:		return KEY_128BIT;
		case HashMule:
		case HashTorrent:
		case HashSHA1:		return KEY_160BIT;
		case HashTigerTree:	return KEY_192BIT;
		default: ASSERT(0); return KEY_64BIT;
	}
}

UINT CFileHash::GetAlgorithm()
{
	switch(m_eType)
	{
		case HashSHA2:
		case HashArchive:
		case HashNeo:		
		case HashXNeo:		return CAbstractKey::eSHA256;
		case HashMD5:		return CAbstractKey::eMD5;
		case HashEd2k:		return CAbstractKey::eMD4;
		case HashMule:		
		case HashTorrent:
		case HashSHA1:		return CAbstractKey::eSHA1;
		case HashTigerTree:	return CAbstractKey::eTiger;
		default: ASSERT(0);	return CAbstractKey::eNone;
	}
}


int CFileHash::GetBase(EFileHashType eType)
{
	switch(eType)
	{
		case HashArchive:
		case HashNeo:
		case HashXNeo:
							return 64;
		case HashMD5:
		case HashEd2k:
		case HashTorrent:	
							return 16;
		case HashSHA1:
		case HashSHA2:
		case HashMule:
		case HashTigerTree:
							return 32;
		default:
			ASSERT(0);
	}
	return 0;
}

EFileHashType CFileHash::Str2HashType(QString Type)
{
	if(Type.compare(HASH_NEO, Qt::CaseInsensitive) == 0 || Type.compare(HASH_NEO_EX, Qt::CaseInsensitive) == 0)
		return HashNeo;
	if(Type.compare(HASH_XNEO, Qt::CaseInsensitive) == 0 || Type.compare(HASH_XNEO_EX, Qt::CaseInsensitive) == 0)
		return HashXNeo;
	if(Type.compare(HASH_ARCH, Qt::CaseInsensitive) == 0)
		return HashArchive;
	if(Type.compare(HASH_MD5, Qt::CaseInsensitive) == 0)
		return HashMD5;
	if(Type.compare(HASH_ED2K, Qt::CaseInsensitive) == 0 || Type.compare("ed2khash", Qt::CaseInsensitive) == 0) // ed2khash is used by Shareaza 
		return HashEd2k;
	if(Type.compare(HASH_SHA1, Qt::CaseInsensitive) == 0)
		return HashSHA1;
	if(Type.compare(HASH_SHA2, Qt::CaseInsensitive) == 0)
		return HashSHA2;
	if(Type.compare(HASH_BT, Qt::CaseInsensitive) == 0)
		return HashTorrent;
	if(Type.compare(HASH_MULE, Qt::CaseInsensitive) == 0)
		return HashMule;
	if(Type.compare(HASH_TTH, Qt::CaseInsensitive) == 0 || Type.compare(HASH_TTH_EX, Qt::CaseInsensitive) == 0)
		return HashTigerTree;
	if(Type.compare("Unknown", Qt::CaseInsensitive) == 0)
		return HashUnknown;
	return HashNone;
}

QString CFileHash::HashType2Str(EFileHashType eType, bool bExt)
{
	switch(eType)
	{
		case HashNeo:			return bExt ? HASH_NEO_EX : HASH_NEO;
		case HashXNeo:			return bExt ? HASH_XNEO_EX : HASH_XNEO;
		case HashArchive:		return HASH_ARCH;
		case HashMD5:			return HASH_MD5;
		case HashEd2k:			return HASH_ED2K;
		case HashSHA1:			return HASH_SHA1;
		case HashSHA2:			return HASH_SHA2;
		case HashTorrent:		return HASH_BT;
		case HashMule:			return HASH_MULE;
		case HashTigerTree:		return bExt ? HASH_TTH_EX : HASH_TTH;
		case HashUnknown:		return "Unknown";
		default:
			ASSERT(0);
	}
	return "Unknown";
}

EFileHashType CFileHash::GetTypeClass() const
{
	switch(GetType())
	{
		case HashNeo:
		case HashMD5:
		case HashEd2k:
		case HashSHA1:
		case HashSHA2:
		case HashMule:
		case HashTigerTree:	return HashSingle;
		case HashXNeo:		return HashMulti;
		case HashTorrent:
		case HashArchive:	
		default:			return HashUnknown;
	}
}

bool CFileHash::IsValid(const byte* pHash, size_t uSize)
{
	ASSERT(uSize % 4 == 0);
	for(size_t i=0; i<uSize; i+=4)
	{
		if(*((uint32*)(pHash+i)) != 0x00000000)
			return true;
	}
	return false;
}

bool CFileHash::IsValid()
{
	QReadLocker Locker(&m_HashMutex);
	return IsValid((byte*)m_Hash.data(), m_uSize);
}

bool CFileHash::IsComplete()
{
	switch(GetType())
	{
		case HashMD5:
		case HashSHA1:
		case HashSHA2:
		case HashArchive:
			return true;
	}
	return false;
}

bool CFileHash::Compare(const CFileHash* pHash) const
{
	if(pHash == NULL)
		return false;

	if(m_eType != pHash->m_eType)
		return false; // so false it coudnt be more false!!!

	ASSERT(m_uSize == pHash->m_uSize); // same type, same size, unless somethign is screwed up.
	return Compare((byte*)pHash->m_Hash.data());
}

bool CFileHash::Compare(const byte* pBuffer) const
{
	QReadLocker Locker(&m_HashMutex);
	return memcmp(m_Hash.data(), pBuffer, m_uSize) == 0;
}

bool CFileHash::SetHash(const QByteArray& Hash)
{
	if(Hash.size() != GetSize())
		return false;
	SetHash((const byte*)Hash.data());
	return true;
}

void CFileHash::SetHash(const byte* pBuffer)
{
	QWriteLocker Locker(&m_HashMutex);
	if(pBuffer == NULL)
		memset(m_Hash.data(), 0, m_uSize);
	else
		memcpy(m_Hash.data(), pBuffer, m_uSize);
}

////////////////////////////////////////////////////////////////////////////////
// operative part

bool CFileHash::Calculate(QIODevice* pFile)
{
	switch(GetType())
	{
		// plain hashed
		case HashMD5:
		case HashSHA1:
		case HashSHA2:
		{
			CHashFunction Hash(GetAlgorithm());
			ASSERT(Hash.IsValid());
			
			pFile->seek(0);
			const size_t BuffSize = 16*1024;
			char Buffer[BuffSize];
			for(qint64 uPos = 0; uPos < pFile->size();)
			{
				qint64 uRead = pFile->read(Buffer, BuffSize);
				if(uRead < 1)
					return false;
				Hash.Add((byte*)Buffer, uRead);
				uPos += uRead;
			}

			Hash.Finish();
			ASSERT(Hash.GetSize() == GetSize());
			SetHash(Hash.GetKey());
			return true;
		}

		default:
			ASSERT(0); // dont calculate complex hashes with this class
	}
	return false;
}

//////////////////////////////
// CFileHashEx

uint32 CFileHashEx::GetStatusCount()
{
	uint64 uTotalSize = GetTotalSize();
	uint64 uBlockSize = GetBlockSize();
	uint64 uPartSize = GetPartSize();
	uint16 uBlockCount = uBlockSize ? DivUp(uPartSize, uBlockSize) : 1;

	// Note: part and block size are only set booth of the block dont fit into a part
	return (uPartSize && uBlockSize) ? ((uTotalSize / uPartSize) * uBlockCount) + DivUp(uTotalSize % uPartSize, uBlockSize) : DivUp(uTotalSize, uPartSize ? uPartSize : uBlockSize);
}

QPair<uint32, uint32> CFileHashEx::IndexRange(uint64 uBegin, uint64 uEnd, bool bEnvelope)
{
	uint64 uBlockSize = GetBlockSize();
	uint64 uPartSize = GetPartSize();
	uint16 uBlockCount = 0; // Note: booth can only be set if we have a intermediate part level that does not accomodate a full count of blocks
	if(uBlockSize && uPartSize)
		uBlockCount = DivUp(uPartSize, uBlockSize);
	else if(!uPartSize)
		uPartSize = uBlockSize;
	ASSERT(uPartSize);

	// Note: this function calculates the proper bit map ofsets for the case where blocks does nto fit into parts, a.k.a. last block per part is smaller
	QPair<uint32, uint32> Range;
	if(uBlockCount)
	{
		if(bEnvelope)
			Range.first = ((uBegin / uPartSize) * uBlockCount) + ((uBegin % uPartSize) / uBlockSize);
		else
			Range.first = ((uBegin / uPartSize) * uBlockCount) + DivUp((uBegin % uPartSize), uBlockSize);

		if(bEnvelope || uEnd >= m_TotalSize)
			Range.second = ((uEnd / uPartSize) * uBlockCount) + DivUp((uEnd % uPartSize), uBlockSize);
		else
			Range.second = ((uEnd / uPartSize) * uBlockCount) + ((uEnd % uPartSize) / uBlockSize);
	}
	else
	{
		if(bEnvelope)
			Range.first = (uBegin / uPartSize);
		else
			Range.first = DivUp(uBegin, uPartSize);

		if(bEnvelope || uEnd >= m_TotalSize)
			Range.second = DivUp(uEnd, uPartSize);
		else
			Range.second = (uEnd / uPartSize);
	}
	if(Range.first >= Range.second)
	{
		ASSERT(!bEnvelope);
		Range.first = Range.second;
	}
	return Range;
}

uint64 CFileHashEx::IndexOffset(uint32 Index)
{
	uint64 uBlockSize = GetBlockSize();
	uint64 uPartSize = GetPartSize();
	uint16 uBlockCount = 0; // Note: booth can only be set if we have a intermediate part level that does not accomodate a full count of blocks
	if(uBlockSize && uPartSize)
		uBlockCount = DivUp(uPartSize, uBlockSize);
	else if(!uPartSize)
		uPartSize = uBlockSize;

	uint64 Offset;
	if(uBlockCount)
		Offset = ((Index / uBlockCount) * uPartSize) + ((Index % uBlockCount) * uBlockSize);
	else
		Offset = uPartSize * Index;
	
	if(Offset > m_TotalSize)
		Offset = m_TotalSize;
	return Offset;
}

void CFileHashEx::SetResult(bool bResult, uint64 uBegin, uint64 uEnd)
{
	QWriteLocker Locker(&m_StatusMutex);

	ASSERT(uBegin < uEnd);
	ASSERT(uEnd <= m_TotalSize);

	uint32 uStatusCount = GetStatusCount();
	ASSERT(m_StatusMap.isEmpty() || m_StatusMap.size() == uStatusCount);
	if(m_StatusMap.isEmpty())
		m_StatusMap.resize(uStatusCount);

	QPair<uint32, uint32> Range = IndexRange(uBegin, uEnd);
	for(uint32 Index = Range.first; Index < Range.second; Index++)
		m_StatusMap.setBit(Index, bResult);

	// Note: m_CorruptionSet mist be cleared befoure verify run so we dont need to clere on demand
	if(!bResult)
		m_CorruptionSet.append(qMakePair(uBegin, uEnd));
}

void CFileHashEx::ClearResult(uint64 uBegin, uint64 uEnd)
{
	QWriteLocker Locker(&m_StatusMutex);

	ASSERT(uBegin < uEnd);
	ASSERT(uEnd <= m_TotalSize);

	uint32 uStatusCount = GetStatusCount();
	ASSERT(m_StatusMap.isEmpty() || m_StatusMap.size() == uStatusCount);
	if(m_StatusMap.isEmpty())
		m_StatusMap.resize(uStatusCount);

	QPair<uint32, uint32> Range = IndexRange(uBegin, uEnd);
	for(uint32 Index = Range.first; Index < Range.second; Index++)
	{
		if(m_StatusMap.testBit(Index))
			m_StatusMap.setBit(Index, 0);
	}

	for(QList<TPair64>::iterator I = m_CorruptionSet.begin(); I != m_CorruptionSet.end(); )
	{
		if(I->first < uEnd && I->second > uBegin)
			I = m_CorruptionSet.erase(I);
		else
			I++;
	}
}

bool CFileHashEx::GetResult(uint64 uBegin, uint64 uEnd)
{
	QReadLocker Locker(&m_StatusMutex);
	return testBits(m_StatusMap, IndexRange(uBegin, uEnd));
}
