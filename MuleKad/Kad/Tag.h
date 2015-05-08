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

#ifndef TAG_H
#define TAG_H


#include "Types.h"
#include "../../Framework/Exception.h"

class CBuffer;

enum Tag_Types {
	TAGTYPE_HASH16		= 0x01,
	TAGTYPE_STRING		= 0x02,
	TAGTYPE_UINT32		= 0x03,
	TAGTYPE_FLOAT32		= 0x04,
	TAGTYPE_BOOL		= 0x05,
	TAGTYPE_BOOLARRAY	= 0x06,
	TAGTYPE_BLOB		= 0x07,
	TAGTYPE_UINT16		= 0x08,
	TAGTYPE_UINT8		= 0x09,
	TAGTYPE_BSOB		= 0x0A,
	TAGTYPE_UINT64		= 0x0B,

	// Compressed string types
	TAGTYPE_STR1		= 0x11,
	TAGTYPE_STR2,
	TAGTYPE_STR3,
	TAGTYPE_STR4,
	TAGTYPE_STR5,
	TAGTYPE_STR6,
	TAGTYPE_STR7,
	TAGTYPE_STR8,
	TAGTYPE_STR9,
	TAGTYPE_STR10,
	TAGTYPE_STR11,
	TAGTYPE_STR12,
	TAGTYPE_STR13,
	TAGTYPE_STR14,
	TAGTYPE_STR15,
	TAGTYPE_STR16,
	TAGTYPE_STR17,	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			// only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
	TAGTYPE_STR18,	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			//  only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
	TAGTYPE_STR19,	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			// only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
	TAGTYPE_STR20,	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			// only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
	TAGTYPE_STR21,	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			// only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
	TAGTYPE_STR22	// accepted by eMule 0.42f (02-Mai-2004) in receiving code
			// only because of a flaw, those tags are handled correctly,
			// but should not be handled at all
};

///////////////////////////////////////////////////////////////////////////////
// CTag

class CTag
{
public:
	CTag();
	CTag(const CTag& rTag);
	CTag(const CBuffer& data, bool bOptUTF8);
	virtual ~CTag();

	void ToBuffer(CBuffer* data);
	static CTag* FromBuffer(const CBuffer* data, bool bOptACP = false);

	CTag& operator=(const CTag&);

	uint8 GetType() const		{ return m_uType; }
	uint8 GetNameID() const		{ return m_uName; }
	const wstring& GetName() const	{ return m_Name; }
	
	bool IsStr() const		{ return m_uType == TAGTYPE_STRING; }
	bool IsInt() const		{ return 
		(m_uType == TAGTYPE_UINT64) ||
		(m_uType == TAGTYPE_UINT32) ||
		(m_uType == TAGTYPE_UINT16) ||
		(m_uType == TAGTYPE_UINT8); }
	bool IsFloat() const		{ return m_uType == TAGTYPE_FLOAT32; }
	bool IsHash() const		{ return m_uType == TAGTYPE_HASH16; }
	bool IsBlob() const		{ return m_uType == TAGTYPE_BLOB; }
	bool IsBsob() const		{ return m_uType == TAGTYPE_BSOB; }
	
	uint64 GetInt() const;
	
	const wstring& GetStr() const;
	
	float GetFloat() const;
	
	const unsigned char* GetHash() const;
	
	const byte* GetBlob() const;
	uint32 GetBlobSize() const;
	
	const byte* GetBsob() const;
	uint32 GetBsobSize() const;
	
	CTag* CloneTag()		{ return new CTag(*this); }

	wstring GetFullInfo() const;

protected:
	CTag(const wstring& Name);
	CTag(uint8 uName);

	uint8	m_uType;
	union {
		wstring*		m_pstrVal;
		uint64			m_uVal;
		float			m_fVal;
		unsigned char*	m_pData;
	};

	uint32		m_nSize;
	
private:
	uint8		m_uName;
	wstring		m_Name;
	
};

typedef std::list<CTag*> TagPtrList;

class CTagIntSized : public CTag
{
public:
	CTagIntSized(const wstring& name, uint64 value, uint8 bitsize)
		: CTag(name) {
			Init(value, bitsize);
		}

	CTagIntSized(uint8 name, uint64 value, uint8 bitsize)
		: CTag(name) {
			Init(value, bitsize);			
		}
		
protected:
	CTagIntSized(const wstring& name) : CTag(name) {}
	CTagIntSized(uint8 name) : CTag(name) {}

	void Init(uint64 value, uint8 bitsize) {
			switch (bitsize) {
				case 64:
					ASSERT(value <= ULONGLONG(0xFFFFFFFFFFFFFFFF)); 
					m_uVal = value;
					m_uType = TAGTYPE_UINT64;
					break;
				case 32:
					ASSERT(value <= 0xFFFFFFFF); 
					m_uVal = value;
					m_uType = TAGTYPE_UINT32;
					break;
				case 16:
					ASSERT(value <= 0xFFFF); 
					m_uVal = value;
					m_uType = TAGTYPE_UINT16;
					break;
				case 8:
					ASSERT(value <= 0xFF); 
					m_uVal = value;
					m_uType = TAGTYPE_UINT8;
					break;
				default:
					throw CException(LOG_ERROR, L"Invalid bitsize on int tag");
			}
	}
};

class CTagVarInt : public CTagIntSized
{
public:
	CTagVarInt(const wstring& name, uint64 value, uint8 forced_bits = 0)
		: CTagIntSized(name) {
			SizedInit(value, forced_bits);
		}
	CTagVarInt(uint8 name, uint64 value, uint8 forced_bits = 0)
		: CTagIntSized(name) {
			SizedInit(value, forced_bits);
		}
private:
	void SizedInit(uint64 value, uint8 forced_bits) {
		if (forced_bits) {
			// The bitsize was forced.
			Init(value,forced_bits);
		} else { 
			m_uVal = value;
			if (value <= 0xFF) {
				m_uType = TAGTYPE_UINT8;
			} else if (value <= 0xFFFF) {
				m_uType = TAGTYPE_UINT16;
			} else if (value <= 0xFFFFFFFF) {
				m_uType = TAGTYPE_UINT32;
			} else {
				m_uType = TAGTYPE_UINT64;
			}
		}		
	}
};

class CTagInt64 : public CTagIntSized
{
public:
	CTagInt64(const wstring& name, uint64 value)
		: CTagIntSized(name, value, 64) {	}

	CTagInt64(uint8 name, uint64 value)
		: CTagIntSized(name, value, 64) { }
};

class CTagInt32 : public CTagIntSized
{
public:
	CTagInt32(const wstring& name, uint64 value)
		: CTagIntSized(name, value, 32) {	}

	CTagInt32(uint8 name, uint64 value)
		: CTagIntSized(name, value, 32) { }
};

class CTagInt16 : public CTagIntSized
{
public:
	CTagInt16(const wstring& name, uint64 value)
		: CTagIntSized(name, value, 16) {	}

	CTagInt16(uint8 name, uint64 value)
		: CTagIntSized(name, value, 16) { }
};

class CTagInt8 : public CTagIntSized
{
public:
	CTagInt8(const wstring& name, uint64 value)
		: CTagIntSized(name, value, 8) {	}

	CTagInt8(uint8 name, uint64 value)
		: CTagIntSized(name, value, 8) { }
};

class CTagFloat : public CTag
{
public:
	CTagFloat(const wstring& name, float value)
		: CTag(name) {
			m_fVal = value;
			m_uType = TAGTYPE_FLOAT32;
		}

	CTagFloat(uint8 name, float value)
		: CTag(name) {
			m_fVal = value;
			m_uType = TAGTYPE_FLOAT32;
		}
};

class CTagString : public CTag
{
public:
	CTagString(const wstring& name, const wstring& value)
		: CTag(name) {
			m_pstrVal = new wstring(value);
			m_uType = TAGTYPE_STRING;
		}

	CTagString(uint8 name, const wstring& value)
		: CTag(name) {
			m_pstrVal = new wstring(value);
			m_uType = TAGTYPE_STRING;
		}
};

class CTagHash : public CTag
{
public:
	CTagHash(const wstring& name, const unsigned char* value);
	CTagHash(uint8 name, const unsigned char* value);
};

class CTagBsob : public CTag
{
public:
	CTagBsob(const wstring& name, const byte* value, uint8 nSize)
		: CTag(name)
	{
		m_uType = TAGTYPE_BSOB;
		m_pData = new byte[nSize];
		memcpy(m_pData, value, nSize);
		m_nSize = nSize;
	}
};

class CTagBlob : public CTag
{
public:
	CTagBlob(const wstring& name, const byte* value, uint8 nSize)
		: CTag(name)
	{
		m_uType = TAGTYPE_BLOB;
		m_pData = new byte[nSize];
		memcpy(m_pData, value, nSize);
		m_nSize = nSize;
	}
};

void deleteTagPtrListEntries(TagPtrList* taglist);

#endif // TAG_H
// File_checked_for_headers
