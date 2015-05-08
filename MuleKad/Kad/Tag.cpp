//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2012 David Xanatos ( XanatosDavid@googlemail.com )
// Copyright (c) 2003-2011 aMule Team ( admin@amule.org / http://www.amule.org )
// Copyright (c) 2002-2011 Merkur ( devs@emule-project.net / http://www.emule-project.net )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//
#include "GlobalHeader.h"

#include "Tag.h"				// Interface declarations

#include "../../Framework/Buffer.h"
#include "../../Framework/Exception.h"

///////////////////////////////////////////////////////////////////////////////
// CTag

CTag::CTag(const wstring& Name)
{
	m_uType = 0;
	m_uName = 0;
	m_Name = Name;
	m_uVal = 0;
	m_nSize = 0;
}

CTag::CTag(uint8 uName)
{
	m_uType = 0;
	m_uName = uName;
	m_uVal = 0;
	m_nSize = 0;
}

CTag::CTag(const CTag& rTag)
{
	m_uType = rTag.m_uType;
	m_uName = rTag.m_uName;
	m_Name = rTag.m_Name;
	m_nSize = 0;
	if (rTag.IsStr()) {
		m_pstrVal = new wstring(rTag.GetStr());
	} else if (rTag.IsInt()) {
		m_uVal = rTag.GetInt();
	} else if (rTag.IsFloat()) {
		m_fVal = rTag.GetFloat();
	} else if (rTag.IsHash()) {
		m_pData = new unsigned char[16];
		memcpy(m_pData, rTag.GetHash(), 16);
	} else if (rTag.IsBlob()) {
		m_nSize = rTag.GetBlobSize();
		m_pData = new unsigned char[rTag.GetBlobSize()];
		memcpy(m_pData, rTag.GetBlob(), rTag.GetBlobSize());
	} else if (rTag.IsBsob()) {
		m_nSize = rTag.GetBsobSize();
		m_pData = new unsigned char[rTag.GetBsobSize()];
		memcpy(m_pData, rTag.GetBsob(), rTag.GetBsobSize());
	} else {
		ASSERT(0);
		m_uVal = 0;
	}
}


CTag::CTag(const CBuffer& data, bool bOptUTF8)
{
	// Zero variables to allow for safe deletion
	m_uType = m_uName = m_nSize = m_uVal = 0;
	m_pData = NULL;	
	
	m_uType = data.ReadValue<uint8_t>();
	if (m_uType & 0x80) {
		m_uType &= 0x7F;
		m_uName = data.ReadValue<uint8_t>();
	} else {
		uint16 length = data.ReadValue<uint16_t>();
		if (length == 1) {
			m_uName = data.ReadValue<uint8_t>();
		} else {
			m_uName = 0;
			m_Name = data.ReadString(CBuffer::eAscii, length);
		}
	}

	// NOTE: It's very important that we read the *entire* packet data,
	// even if we do not use each tag. Otherwise we will get in trouble
	// when the packets are returned in a list - like the search results
	// from a server. If we cannot do this, then we throw an exception.
	switch (m_uType) {
		case TAGTYPE_STRING:
			m_pstrVal = new wstring(data.ReadString(bOptUTF8 ? CBuffer::eUtf8 : CBuffer::eAscii, CBuffer::e16Bit));
			break;
		
		case TAGTYPE_UINT32:
			m_uVal = data.ReadValue<uint32_t>();
			break;

		case TAGTYPE_UINT64:
			m_uVal = data.ReadValue<uint64_t>();
			break;
		
		case TAGTYPE_UINT16:
			m_uVal = data.ReadValue<uint16_t>();
			m_uType = TAGTYPE_UINT32;
			break;
		
		case TAGTYPE_UINT8:
			m_uVal = data.ReadValue<uint8_t>();
			m_uType = TAGTYPE_UINT32;
			break;
		
		case TAGTYPE_FLOAT32:
			//#warning Endianess problem?
			m_fVal = *((float*)data.ReadData(4));
			break;
		
		case TAGTYPE_HASH16:
			m_pData = new unsigned char[16];
			memcpy(m_pData, data.ReadData(16), 16);
			break;
		
		case TAGTYPE_BOOL:
			//printf("***NOTE: %s; Reading BOOL tag\n", __FUNCTION__);
			data.ReadValue<uint8_t>();
			break;
		
		case TAGTYPE_BOOLARRAY: {
			//printf("***NOTE: %s; Reading BOOL Array tag\n", __FUNCTION__);
			uint16 len = data.ReadValue<uint16_t>();
		
			// 07-Apr-2004: eMule versions prior to 0.42e.29 used the formula "(len+7)/8"!
			//#warning This seems to be off by one! 8 / 8 + 1 == 2, etc.
			data.SetPosition(data.GetPosition() + (len / 8) + 1);
			break;
		}

		case TAGTYPE_BLOB:
			// 07-Apr-2004: eMule versions prior to 0.42e.29 handled the "len" as int16!
			m_nSize = data.ReadValue<uint32_t>();
			
			// Since the length is 32b, this check is needed to avoid
			// huge allocations in case of bad tags.
			if (m_nSize > data.GetSize() - data.GetPosition()) {
				throw CException(LOG_ERROR, L"Malformed tag");
			}
				
			m_pData = new unsigned char[m_nSize];
			memcpy(m_pData, data.ReadData(m_nSize), m_nSize);
			break;
	
		default:
			if (m_uType >= TAGTYPE_STR1 && m_uType <= TAGTYPE_STR16) {
				uint8 length = m_uType - TAGTYPE_STR1 + 1;
				m_pstrVal = new wstring(data.ReadString(bOptUTF8 ? CBuffer::eUtf8 : CBuffer::eUtf8_BOM, length));
				m_uType = TAGTYPE_STRING;
			} else {
				// Since we cannot determine the length of this tag, we
				// simply have to abort reading the file.
				throw CException(LOG_ERROR, L"Unknown tag type encounted %d, cannot proceed!", (int)m_uType);
			}
	}

}


CTag::~CTag()
{
	if (IsStr()) {
		delete m_pstrVal;
	} else if (IsHash() || IsBlob() || IsBsob()) {
		delete[] m_pData;
	} 
}


void CTag::ToBuffer(CBuffer* data)
{
	data->WriteValue<uint8_t>(GetType());
		
	if (!GetName().empty()) {
		data->WriteString(GetName(), CBuffer::eAscii, CBuffer::e16Bit);
	} else {
		data->WriteValue<uint16_t>(1);
		data->WriteValue<uint8_t>(GetNameID());
	}
	
	switch (GetType())
	{
		case TAGTYPE_HASH16:
			// Do NOT use this to transfer any tags for at least half a year!!
			data->WriteData(GetHash(),16);
			break;
		case TAGTYPE_STRING:
			data->WriteString(GetStr(), CBuffer::eUtf8); // Always UTF8
			break;
		case TAGTYPE_UINT64:
			data->WriteValue<uint64_t>(GetInt());
			break;
		case TAGTYPE_UINT32:
			data->WriteValue<uint32_t>(GetInt());
			break;
		case TAGTYPE_FLOAT32:
		{
			float Float = GetFloat();
			data->WriteData((byte*)&Float, 4);
			break;
		}
		case TAGTYPE_BSOB:
			data->WriteValue<uint8_t>(GetBsobSize());
			data->WriteData(GetBsob(), GetBsobSize());
			break;
		case TAGTYPE_UINT16:
			data->WriteValue<uint16_t>(GetInt());
			break;
		case TAGTYPE_UINT8:
			data->WriteValue<uint8_t>(GetInt());
			break;
		case TAGTYPE_BLOB:
			// NOTE: This will break backward compatibility with met files for eMule versions prior to 0.44a
			// and any aMule prior to SVN 26/02/2005
			data->WriteValue<uint32_t>(GetBlobSize());
			data->WriteData(GetBlob(), GetBlobSize());
			break;
		default:
			//TODO: Support more tag types
			// With the if above, this should NEVER happen.
			LogLine(LOG_ERROR, L"CFileDataIO::WriteTag: Unknown tag: type=0x%02X", GetType());
			ASSERT(0);;
			break;
	}			
}

CTag* CTag::FromBuffer(const CBuffer* data, bool bOptACP)
{
	CTag *retVal = NULL;
	wstring name;
	byte type = 0;
	try {
		type = data->ReadValue<uint8_t>();
		name = data->ReadString(CBuffer::eUtf8_BOM);

		switch (type)
		{
			// NOTE: This tag data type is accepted and stored only to give us the possibility to upgrade 
			// the net in some months.
			//
			// And still.. it doesnt't work this way without breaking backward compatibility. To properly
			// do this without messing up the network the following would have to be done:
			//	 -	those tag types have to be ignored by any client, otherwise those tags would also be sent (and 
			//		that's really the problem)
			//
			//	 -	ignoring means, each client has to read and right throw away those tags, so those tags get
			//		get never stored in any tag list which might be sent by that client to some other client.
			//
			//	 -	all calling functions have to be changed to deal with the 'nr. of tags' attribute (which was 
			//		already parsed) correctly.. just ignoring those tags here is not enough, any taglists have to 
			//		be built with the knowledge that the 'nr. of tags' attribute may get decreased during the tag 
			//		reading..
			// 
			// If those new tags would just be stored and sent to remote clients, any malicious or just bugged
			// client could let send a lot of nodes "corrupted" packets...
			//
			case TAGTYPE_HASH16:
			{
				retVal = new CTagHash(name, data->ReadData(16));
				break;
			}

			case TAGTYPE_STRING:
				retVal = new CTagString(name, data->ReadString(bOptACP ? CBuffer::eUtf8 : CBuffer::eUtf8_BOM));
				break;

			case TAGTYPE_UINT64:
				retVal = new CTagInt64(name, data->ReadValue<uint64_t>());
				break;

			case TAGTYPE_UINT32:
				retVal = new CTagInt32(name, data->ReadValue<uint32_t>());
				break;

			case TAGTYPE_UINT16:
				retVal = new CTagInt16(name, data->ReadValue<uint16_t>());
				break;

			case TAGTYPE_UINT8:
				retVal = new CTagInt8(name, data->ReadValue<uint8_t>());
				break;

			case TAGTYPE_FLOAT32:
				retVal = new CTagInt8(name, *((float*)data->ReadData(4)));
				break;

			// NOTE: This tag data type is accepted and stored only to give us the possibility to upgrade 
			// the net in some months.
			//
			// And still.. it doesnt't work this way without breaking backward compatibility
			case TAGTYPE_BSOB:
			{
				uint8_t size = data->ReadValue<uint8_t>();
				retVal = new CTagBsob(name, data->ReadData(size), size);
				break;
			}

			default:
			{
				throw CException(LOG_ERROR, L"Invalid Kad tag type; type=%d name=%s", (int)type, name.c_str());
			}
		}
	} catch(const CException&) {
		//ASSERT(0);
		delete retVal;
		throw;
	}
	
	return retVal;
}


CTag &CTag::operator=(const CTag &rhs)
{
	if (&rhs != this) {
		m_uType = rhs.m_uType;
		m_uName = rhs.m_uName;
		m_Name = rhs.m_Name;
		m_nSize = 0;
		if (rhs.IsStr()) {
			wstring *p = new wstring(rhs.GetStr());
			delete m_pstrVal;
			m_pstrVal = p;
		} else if (rhs.IsInt()) {
			m_uVal = rhs.GetInt();
		} else if (rhs.IsFloat()) {
			m_fVal = rhs.GetFloat();
		} else if (rhs.IsHash()) {
			delete m_pData;
			m_pData = new unsigned char[16];
			memcpy(m_pData, rhs.GetHash(), 16);
		} else if (rhs.IsBlob()) {
			m_nSize = rhs.GetBlobSize();
			unsigned char *p = new unsigned char[rhs.GetBlobSize()];
			delete [] m_pData;
			m_pData = p;
			memcpy(m_pData, rhs.GetBlob(), rhs.GetBlobSize());
		} else if (rhs.IsBsob()) {
			m_nSize = rhs.GetBsobSize();
			unsigned char *p = new unsigned char[rhs.GetBsobSize()];
			delete [] m_pData;
			m_pData = p;
			memcpy(m_pData, rhs.GetBsob(), rhs.GetBsobSize());
		} else {
			ASSERT(0);
			m_uVal = 0;
		}
	}
	return *this;
}


#define CHECK_TAG_TYPE(check, expected) \
	if (!(check)) { \
		throw CException(LOG_ERROR, L"%s tag expected, but found %s", expected, GetFullInfo().c_str()); \
	}

uint64 CTag::GetInt() const
{
	CHECK_TAG_TYPE(IsInt(), L"Integer");
	
	return m_uVal; 
}


const wstring& CTag::GetStr() const
{
	CHECK_TAG_TYPE(IsStr(), L"String");
	
	return *m_pstrVal; 	
}


float CTag::GetFloat() const
{
	CHECK_TAG_TYPE(IsFloat(), L"Float");

	return m_fVal;
}


const unsigned char* CTag::GetHash() const
{
	CHECK_TAG_TYPE(IsHash(), L"Hash");
	
	return m_pData;
}
	

uint32 CTag::GetBlobSize() const
{
	CHECK_TAG_TYPE(IsBlob(), L"Blob");
	
	return m_nSize;
}
	

const byte* CTag::GetBlob() const
{
	CHECK_TAG_TYPE(IsBlob(), L"Blob");
	
	return m_pData;
}


uint32 CTag::GetBsobSize() const
{
	CHECK_TAG_TYPE(IsBsob(), L"Bsob");
	
	return m_nSize;
}


const byte* CTag::GetBsob() const
{
	CHECK_TAG_TYPE(IsBsob(), L"Bsob");
	
	return m_pData;
}


wstring CTag::GetFullInfo() const
{
	wstringstream strTag;
	if (!m_Name.empty()) {
		// Special case: Kad tags, and some ED2k tags ...
		if (m_Name.length() == 1) {
			strTag << L"u" << (unsigned)m_Name[0];
		} else {
			strTag << L"\"";
			strTag << m_Name;
			strTag << L"\"";
		}
	} else {
		strTag << L"u" << m_Name;
	}
	strTag << L"=";
	if (m_uType == TAGTYPE_STRING) {
		strTag << L"\"";
		strTag << *m_pstrVal;
		strTag << L"\"";
	} else if (m_uType >= TAGTYPE_STR1 && m_uType <= TAGTYPE_STR16) {
		strTag << L"(Str" << (m_uType - TAGTYPE_STR1 + 1) << L")\"" << *m_pstrVal << L"\"";
	} else if (m_uType == TAGTYPE_UINT64) {
		strTag << L"(Int64)" << m_uVal;
	} else if (m_uType == TAGTYPE_UINT32) {
		strTag << L"(Int32)" << m_uVal;
	} else if (m_uType == TAGTYPE_UINT16) {
		strTag << L"(Int16)" << m_uVal;
	} else if (m_uType == TAGTYPE_UINT8) {
		strTag << L"(Int8)" << m_uVal;
	} else if (m_uType == TAGTYPE_FLOAT32) {
		strTag << L"(Float32)" << m_fVal;
	} else if (m_uType == TAGTYPE_BLOB) {
		strTag << L"(Blob)" << m_nSize;
	} else if (m_uType == TAGTYPE_BSOB) {
		strTag << L"(Blob)" << m_nSize;
	} else {
		strTag << L"Type=" << m_uType;
	}
	return strTag.str();
}

CTagHash::CTagHash(const wstring& name, const unsigned char* value)
	: CTag(name) {
		m_pData = new unsigned char[16];
		memcpy(m_pData, value, 16);
		m_uType = TAGTYPE_HASH16;
	}

CTagHash::CTagHash(uint8 name, const unsigned char* value)
	: CTag(name) {
		m_pData = new unsigned char[16];
		memcpy(m_pData, value, 16);
		m_uType = TAGTYPE_HASH16;
	}

void deleteTagPtrListEntries(TagPtrList* taglist)
{
	for (TagPtrList::iterator itTagPtrList = taglist->begin(); itTagPtrList != taglist->end(); ++itTagPtrList)
		delete *itTagPtrList;
	taglist->clear();
}

// File_checked_for_headers
