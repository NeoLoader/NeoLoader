#include "GlobalHeader.h"
#include "Buffer.h"
#include "../zlib/zlib.h"
#include "Functions.h"
#include "Exception.h"

CBuffer::CBuffer(size_t uLength, bool bUsed)
{
	Init();
	AllocBuffer(uLength, bUsed);
}

CBuffer::CBuffer(const CBuffer& Buffer)
{
	Init();
	if(Buffer.GetSize())
		CopyBuffer(((CBuffer*)&Buffer)->GetBuffer(), Buffer.GetSize());
}

CBuffer::CBuffer(void* pBuffer, size_t uSize, bool bDerived)
{
	Init();
	SetBuffer(pBuffer, uSize, bDerived);
}

CBuffer::CBuffer(const void* pBuffer, const size_t uSize, bool bDerived)
{
	Init();
	SetBuffer((void*)pBuffer, uSize, bDerived);
	SetReadOnly();
}

CBuffer::~CBuffer()
{
	if(m_pBuffer && !IsDerived())
		free(m_pBuffer);
}

CBuffer& CBuffer::operator=(const CBuffer &Buffer)
{
	CopyBuffer(((CBuffer*)&Buffer)->GetBuffer(), Buffer.GetSize());
	return *this;
}

void CBuffer::Init() 
{
	m_pBuffer = NULL;
	m_uSize = 0;
	m_uLength = 0;
	m_uPosition = 0;
	m_eType = eNormal;
	m_bReadOnly = false;
}

void CBuffer::AllocBuffer(size_t uLength, bool bUsed, bool bFixed)
{
	if(m_pBuffer && !IsDerived())
		free(m_pBuffer);

	m_uPosition = 0;
	m_uLength = uLength;
	m_pBuffer = (byte*)malloc(uLength);
	m_uSize = bUsed ? uLength : 0;
	m_eType = bFixed ? eFixed : eNormal;
}

void CBuffer::SetBuffer(void* pBuffer, size_t uSize, bool bDerived)
{
	if(m_pBuffer && !IsDerived())
		delete m_pBuffer;

	if(pBuffer == NULL)
		Init();
	else
	{
		m_uLength = uSize;
		m_uSize = uSize;
		m_eType = bDerived ? eDerived : eFixed;
		m_uPosition = 0;
		m_pBuffer = (byte*)pBuffer;
	}
}

void CBuffer::CopyBuffer(void* pBuffer, size_t uSize)
{
	AllocBuffer(uSize, true);
	ASSERT(m_pBuffer + m_uSize < (byte*)pBuffer || m_pBuffer > (byte*)pBuffer + m_uSize);
	memcpy(m_pBuffer, pBuffer, m_uSize);
}

bool CBuffer::SetSize(size_t uSize, bool bExpend, size_t uPreAlloc)
{
	if(m_uLength < uSize + uPreAlloc)
	{
		if(!bExpend || IsDerived() || IsReadOnly())
		{
			ASSERT(0);
			return false;
		}

		m_uLength = uSize + uPreAlloc;
		m_pBuffer = (byte*)realloc(m_pBuffer,m_uLength);
	}
	m_uSize = uSize;
	if(m_uSize < m_uPosition)
		m_uPosition = m_uSize;
	return true;
}

bool CBuffer::PrepareWrite(size_t uOffset, size_t uLength)
{
	if(IsReadOnly())
		return false;

	// check if there is enough space allocated for the data, and fail if no realocation cna be done
	if(uOffset + uLength > m_uLength || m_pBuffer == NULL)
	{
		if(m_eType != eNormal)
			return false;

		size_t uPreAlloc = Max(Min(uLength*10,m_uSize/2), 16);
		return SetSize(uOffset + uLength, true, uPreAlloc);
	}

	// check if we are overwriting data or adding data, if the later than increase teh size accordingly
	if(uOffset + uLength > m_uSize)
		m_uSize = uOffset + uLength;
	return true;
}

byte* CBuffer::GetBuffer(bool bDetatch)	
{
	if(!bDetatch)
		return m_pBuffer;

	if(IsDerived())
	{
		size_t uSize = GetSize();
		byte* pBuffer = new byte[uSize];
		memcpy(pBuffer,CBuffer::GetBuffer(),uSize);
		return pBuffer;
	}
	else
	{
		byte* pBuffer = m_pBuffer;
		Init();
		return pBuffer;
	}
}

int CBuffer::Compare(const CBuffer& Buffer) const
{
	if(Buffer.m_uSize < m_uSize)
		return -1;
	if(Buffer.m_uSize > m_uSize)
		return 1;
	return memcmp(Buffer.m_pBuffer, m_pBuffer, m_uSize);
}

bool CBuffer::Pack()
{
	ASSERT(m_uSize + 300 < 0xFFFFFFFF);

	uLongf uNewSize = (uLongf)(m_uSize + 300);
	byte* Buffer = new byte[uNewSize];
	int Ret = compress2(Buffer,&uNewSize,m_pBuffer,(uLongf)m_uSize,Z_BEST_COMPRESSION);
	if (Ret != Z_OK || m_uSize < uNewSize) // does the compression helped?
	{
		delete Buffer;
		return false;
	}
	SetBuffer(Buffer, uNewSize);
	return true;
}

bool CBuffer::Unpack()
{
	ASSERT(m_uSize*10+300 < 0xFFFFFFFF);

	byte* Buffer = NULL;
	uLongf uAllocSize = (uLongf)(m_uSize*10+300);
	uLongf uNewSize = 0;
	int Ret = 0;
	do 
	{
		delete Buffer;
		Buffer = new byte[uAllocSize];
		uNewSize = uAllocSize;
		Ret = uncompress(Buffer,&uNewSize,m_pBuffer,(uLongf)m_uSize);
		uAllocSize *= 2; // size for the next try if needed
	} 
	while (Ret == Z_BUF_ERROR && uAllocSize < Max(MB2B(16), m_uSize*100));	// do not allow the unzip buffer to grow infinetly, 
																				// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
	if (Ret != Z_OK)
	{
		delete Buffer;
		return false;
	}
	SetBuffer(Buffer, uNewSize);
	return true;
}

///////////////////////////////////////////////////////////////////////////////////
// Position based Data Operations

bool CBuffer::SetPosition(size_t uPosition) const
{
	if(uPosition == -1)
		uPosition = m_uSize;
	else if(uPosition > m_uSize)
		return false;

	m_uPosition = uPosition;

	return true;
}

byte* CBuffer::GetData(size_t uLength) const
{
	if(uLength == -1)
		uLength = GetSizeLeft();

	if(m_uPosition + uLength > m_uSize)
		return NULL;

	byte* pData = m_pBuffer+m_uPosition;
	m_uPosition += uLength;

	return pData;
}

byte* CBuffer::ReadData(size_t uLength) const
{
	if(byte* pData = GetData(uLength))
		return pData;
	throw CException(LOG_ERROR | LOG_DEBUG, L"CBuffer::ReadError");
}

byte* CBuffer::SetData(const void* pData, size_t uLength)
{
	if(!PrepareWrite(m_uPosition, uLength))
		return NULL;

	if(pData)
	{
		ASSERT(m_pBuffer + m_uPosition + uLength < (byte*)pData || m_pBuffer + m_uPosition > (byte*)pData + uLength);
		memcpy(m_pBuffer + m_uPosition, pData, uLength);
	}
	pData = m_pBuffer + m_uPosition;
	m_uPosition += uLength;

	return (byte*)pData;
}

void CBuffer::WriteData(const void* pData, size_t uLength)
{
	if(SetData(pData, uLength))
		return;
	throw CException(LOG_ERROR | LOG_DEBUG, L"CBuffer::ReadError");
}

///////////////////////////////////////////////////////////////////////////////////
// Offset based Data Operations

byte* CBuffer::GetData(size_t uOffset, size_t uLength) const
{
	if(uOffset + uLength > m_uSize)
		return NULL;

	return m_pBuffer+uOffset;
}

byte* CBuffer::SetData(size_t uOffset, void* pData, size_t uLength)
{
	if(uOffset == -1)
		uOffset = m_uSize;

	ASSERT(uOffset <= m_uSize);

	if(!PrepareWrite(uOffset, uLength))
		return NULL;

	if(pData)
	{
		ASSERT(m_pBuffer + uOffset + uLength < (byte*)pData || m_pBuffer + uOffset > (byte*)pData + uLength);
		memcpy(m_pBuffer + uOffset, pData, uLength);
	}
	else
		memset(m_pBuffer + uOffset, 0, uLength);

	return m_pBuffer + uOffset;
}

byte* CBuffer::InsertData(size_t uOffset, void* pData, size_t uLength)
{
	ASSERT(uOffset <= m_uSize);

	size_t uSize = m_uSize;
	if(!PrepareWrite(m_uSize, uLength))
		return NULL;

	memmove(m_pBuffer + uOffset + uLength, m_pBuffer + uOffset, uSize - uOffset);

	if(pData)
	{
		ASSERT(m_pBuffer + uOffset + uLength < (byte*)pData || m_pBuffer + uOffset > (byte*)pData + uLength);
		memcpy(m_pBuffer + uOffset, pData, uLength);
	}

	return m_pBuffer + uOffset;
}

byte* CBuffer::ReplaceData(size_t uOffset, size_t uOldLength, void* pData, size_t uNewLength)
{
	if(!RemoveData(uOffset, uOldLength))
		return NULL;
	if(!InsertData(uOffset, pData, uNewLength))
		return NULL;
	return m_pBuffer + uOffset;
}

bool CBuffer::AppendData(const void* pData, size_t uLength)
{
	size_t uOffset = m_uSize; // append to the very end
	if(!PrepareWrite(uOffset, uLength))
	{
		ASSERT(0); // appen must usually always success
		return false;
	}

	ASSERT(pData);
	ASSERT(m_pBuffer + uOffset + uLength < (byte*)pData || m_pBuffer + uOffset > (byte*)pData + uLength);
	memcpy(m_pBuffer + uOffset, pData, uLength);
	return true;
}

bool CBuffer::ShiftData(size_t uOffset)
{
	if(uOffset > m_uSize)
	{
		ASSERT(0); // shift must usually always success
		return false;
	}

	m_uSize -= uOffset;

	memmove(m_pBuffer, m_pBuffer + uOffset, m_uSize);

	if(m_uPosition > uOffset)
		m_uPosition -= uOffset;
	else
		m_uPosition = 0;
	return true;
}

bool CBuffer::RemoveData(size_t uOffset, size_t uLength)
{
	if(IsReadOnly())
		return false;

	if(uLength == -1)
		uLength = m_uSize;
	else if(uOffset + uLength > m_uSize)
		return false;

	memmove(m_pBuffer + uOffset, m_pBuffer + uOffset + uLength, m_uSize - uLength);

	m_uSize -= uLength;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////
// Specifyed Data Operations


void CBuffer::WriteString(const wstring& String, EStrSet Set, EStrLen Len)
{
	string RawString;
	if(Set == eAscii)
		WStrToAscii(RawString, String);
	else
		WStrToUtf8(RawString, String);

	if(Set == eUtf8_BOM)
		RawString.insert(0,"\xEF\xBB\xBF");

	size_t uLength = RawString.length();
	ASSERT(uLength < 0xFFFFFFFF);

	switch(Len)
	{
		case e8Bit:		ASSERT(RawString.length() < 0xFF);			WriteValue<uint8>((uint8)uLength);	break;
		case e16Bit:	ASSERT(RawString.length() < 0xFFFF);		WriteValue<uint16>((uint16)uLength);	break;
		case e32Bit:	ASSERT(RawString.length() < 0xFFFFFFFF);	WriteValue<uint32>((uint32)uLength);	break;
	}

	WriteData(RawString.c_str(), RawString.length());
}

void WStrToAscii(string& dest, const wstring& src)
{
	dest.clear();
	for (size_t i = 0; i < src.size(); i++)
	{
		wchar_t w = src[i];
		if (w <= 0xff)
			dest.push_back((char)w);
		else
			dest.push_back('?');
	}
}

void WStrToUtf8(string& dest, const wstring& src)
{
	dest.clear();
	for (size_t i = 0; i < src.size(); i++)
	{
		wchar_t w = src[i];
		if (w <= 0x7f)
			dest.push_back((char)w);
		else if (w <= 0x7ff)
		{
			dest.push_back(0xc0 | ((w >> 6)& 0x1f));
			dest.push_back(0x80| (w & 0x3f));
		}
		else if (w <= 0xffff)
		{
			dest.push_back(0xe0 | ((w >> 12)& 0x0f));
			dest.push_back(0x80| ((w >> 6) & 0x3f));
			dest.push_back(0x80| (w & 0x3f));
		}
		/*else if (w <= 0x10ffff)  // utf32
		{
			dest.push_back(0xf0 | ((w >> 18)& 0x07));
			dest.push_back(0x80| ((w >> 12) & 0x3f));
			dest.push_back(0x80| ((w >> 6) & 0x3f));
			dest.push_back(0x80| (w & 0x3f));
		}*/
		else
			dest.push_back('?');
	}
}

wstring CBuffer::ReadString(EStrSet Set, EStrLen Len) const
{
	size_t uRawLength = 0;
	switch(Len)
	{
		case e8Bit:		uRawLength = ReadValue<uint8>();	break;
		case e16Bit:	uRawLength = ReadValue<uint16>();	break;
		case e32Bit:	uRawLength = ReadValue<uint32>();	break;
	}
	return ReadString(Set, uRawLength);
}

wstring CBuffer::ReadString(EStrSet Set, size_t uRawLength) const
{
	if(uRawLength == -1)
		uRawLength = GetSizeLeft();
	char* Data = (char*)ReadData(uRawLength) ;
	string RawString(Data, uRawLength);

	if(RawString.compare(0,3,"\xEF\xBB\xBF") == 0)
	{
		Set = eUtf8;
		RawString.erase(0,3);
	}
	else if(Set == eUtf8_BOM)
		Set = eAscii;

	wstring String;
	if(Set == eAscii)
		AsciiToWStr(String, RawString);
	else
		Utf8ToWStr(String, RawString);

	return String;
}

void AsciiToWStr(wstring& dest, const string& src)
{
	dest.clear();
	for (size_t i = 0; i < src.size(); i++)
	{
		unsigned char c = (unsigned char)src[i];
		dest.push_back(c);
	}
}

void Utf8ToWStr(wstring& dest, const string& src)
{
	dest.clear();
	wchar_t w = 0;
	int bytes = 0;
	wchar_t err = L'�';
	for (size_t i = 0; i < src.size(); i++)
	{
		unsigned char c = (unsigned char)src[i];
		if (c <= 0x7f) //first byte
		{
			if (bytes)
			{
				dest.push_back(err);
				bytes = 0;
			}
			dest.push_back((wchar_t)c);
		}
		else if (c <= 0xbf) //second/third/etc byte
		{
			if (bytes)
			{
				w = ((w << 6)|(c & 0x3f));
				bytes--;
				if (bytes == 0)
					dest.push_back(w);
			}
			else
				dest.push_back(err);
		}
		else if (c <= 0xdf)//2byte sequence start
		{
			bytes = 1;
			w = c & 0x1f;
		}
		else if (c <= 0xef)//3byte sequence start
		{
			bytes = 2;
			w = c & 0x0f;
		}
		else if (c <= 0xf7)//3byte sequence start
		{
			bytes = 3;
			w = c & 0x07;
		}
		else
		{
			dest.push_back(err);
			bytes = 0;
		}
	}
	if (bytes)
		dest.push_back(err);
}
