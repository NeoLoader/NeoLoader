#pragma once

/**************************************************************
*
*	Operation:	[uint8 - NameLen][bytes - Name]
*	Variant:	[uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
*					0 - Extended					Map with Paylaod and extensions
*
*					1 - Map							[uint8 - NameLen][bytes - Name][Variant]
*													[uint8 - NameLen][bytes - Name][Variant]
*													...
*													
*					2 - List						[Variant]
*													[Variant]
*													...
*						
*					7 - ByteArray					[byte n]
*
*					8 - StringUtf8					[byte n]
*													
*					16 - Int						[intn]
*					17 - UInt						[uintn]
*					18 - Double/Float				[double/float]
*
*					32,		// xx0x xxxx			reserved must be 0
*
*							// 00xx xxxx			flag Len uint32 compresses
*					64,		// 01xx xxxx			flag Len uint8
*					128,	// 10xx xxxx			flag Len uint16
*					192,	// 11xx xxxx			flag Len uint32
*
*	Note: On loading from binary index all list items and map items
*
*/

#include "../../Framework/Buffer.h"
#include "../../Framework/Strings.h"
#include "../../Framework/Cryptography/AsymmetricKey.h"
#include "../../Framework/Cryptography/SymmetricKey.h"
#include "../../Framework/Cryptography/HashFunction.h"

class CVariant
{
public:
	enum EType
	{
		EExtended	= 0,	// An extended variant signed or encrypted
		EMap		= 1,	// A Map
		EList		= 2,	// A List
							// 3 4 5 6
		EBytes		= 7,	// Binary BLOB of arbitrary length
							//
		EUtf8		= 8,	// String UTF8 Encoded
		EAscii		= 9,	// Strongm ASCII Encoded
							// 10 11 12 13 14 15
		ESInt		= 16,	// Signed Integer 8 16 32 or 64 bit
		EUInt		= 17,	// unsigned Integer 8 16 32 or 64 bit
		EDouble		= 18,	// Floating point Number 32 or 64 bit precision
							// 19 20 21 22 23 24 25 26 27 28 29 30 31
		//ECustom	= 32,	// 0010 0000
							// 33 34 35 36 37 38 39 40 41 42 43 44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62
		EInvalid	= 63,	// 0011 1111
							//
		ELen8		= 64,	// 0100 0000
		ELen16		= 128,	// 1000 0000
		ELen32		= 192,	// 1100 0000
	};

	CVariant(EType Type = EInvalid);
	CVariant(const CVariant& Variant);
	~CVariant();

	void					FromPacket(const CBuffer* pPacket, bool bDerived = false);
	void					ToPacket(CBuffer* pPacket, bool bPack = false) const;

#ifdef USING_QT
	bool					FromQVariant(const QVariant& qVariant);
	QVariant				ToQVariant() const;
#endif

	CVariant& operator=(const CVariant& Variant) {Assign(Variant); return *this;}
	int						Compare(const CVariant &R) const;
	bool					CompareTo(const CVariant &Variant) const	{return Compare(Variant) == 0;}
	bool operator==(const CVariant &Variant) const						{return Compare(Variant) == 0;}
	bool operator!=(const CVariant &Variant) const						{return Compare(Variant) != 0;}

	void					Freeze();
	void					Unfreeze();
	bool					IsFrozen() const;

	uint32					Count() const;

	bool					IsMap() const;
	const string&			Key(uint32 Index) const;
	CVariant&				At(const char* Name);
	const CVariant&			At(const char* Name) const;
	CVariant				Get(const char* Name, const CVariant& Default = CVariant()) const;
	bool					Has(const char* Name) const;
	void					Remove(const char* Name);

	bool					IsList() const;
	void					Insert(const char* Name, const CVariant& Variant);
	void					Append(const CVariant& Variant);
	CVariant&				At(uint32 Index) const;
	void					Remove(uint32 Index);

	void					Merge(const CVariant& Variant);

	void					Sign(CPrivateKey* pPrivKey, UINT eAlgorithm = 0);
	bool					Verify(CPublicKey* pPubKey) const;

	void					Hash(CHashFunction* pHash);
	bool					Test(CHashFunction* pHash) const;

	CVariant				GetFP(UINT eAlgorithm = 0) const;

	void					Encrypt(CPublicKey* pPubKey, UINT eAlgorithm = 0, size_t KeySize = 0);
	bool					Decrypt(CPrivateKey* pPrivKey);

	void					Encrypt(CAbstractKey* pSymKey, CAbstractKey* pIV = NULL);
	bool					Decrypt(CAbstractKey* pSymKey, CAbstractKey* pIV = NULL);

	enum EEncryption
	{
		ePlaintext = 0,
		eAsymmetric = 1,
		eSymmetric = 2,
	};
	EEncryption				IsEncrypted() const;
	bool					IsSigned() const;

	CVariant& operator[](const char* Name)				{return At(Name);}
	const CVariant& operator[](const char* Name) const	{return At(Name);}
	CVariant& operator[](uint32 Index)					{return At(Index);}
	const CVariant& operator[](uint32 Index)	const	{return At(Index);}

	CVariant(const byte* Payload, size_t Size, EType Type = EBytes)		{InitValue(Type, Size, Payload);}
	operator byte*() const							{const SVariant* Variant = Val(); if(Variant->IsRaw()) return Variant->Payload; return NULL;}
	byte*					GetData() const			{const SVariant* Variant = Val(); return Variant->Payload;}
	uint32					GetSize() const			{const SVariant* Variant = Val(); return Variant->Size;}
	EType					GetType() const;
	bool					IsValid() const;

	void					Clear();
	CVariant				Clone(bool Full = true) const;

	CVariant(const CBuffer& Buffer)					{InitValue(EBytes, Buffer.GetSize(), Buffer.GetBuffer());}
	CVariant(CBuffer& Buffer, bool bTake = false)	{size_t uSize = Buffer.GetSize(); InitValue(EBytes, uSize, Buffer.GetBuffer(bTake), bTake);}
	operator CBuffer() const						{const SVariant* Variant = Val(); if(Variant->IsRaw()) return CBuffer(Variant->Payload, Variant->Size, true); return CBuffer();}

	template <class T> T To() const					{return (T)*this;}
	template <class T> T AsNum() const
	{
		if(GetType() == EDouble)
			return To<double>();
		else if(GetType() == ESInt || GetType() == EUInt)
			return To<sint32>();
		else if(GetType() == EAscii || GetType() == EUtf8)
		{
			wstring Num = To<wstring>();
			if(Num.find(L".") != wstring::npos)
				return wstring2double(Num);
			return wstring2int(Num);
		}
		else
			return 0;
	}
	wstring AsStr() const
	{
		if(GetType() == EAscii || GetType() == EUtf8)
			return To<wstring>();
		else if(GetType() == EDouble)
			return double2wstring(To<double>());
		else if(GetType() == ESInt || GetType() == EUInt)
			return int2wstring(To<double>());
		else
			return L"";
	}

	CVariant(const bool& b)				{InitValue(ESInt, sizeof(bool), &b);}
	operator bool() const				{bool b = false; GetInt(sizeof(bool), &b); return b;}
	CVariant(const sint8& sint)			{InitValue(ESInt, sizeof(sint8), &sint);}
	operator sint8() const				{sint8 sint = 0; GetInt(sizeof(sint8), &sint); return sint;}
	CVariant(const uint8& uint)			{InitValue(EUInt, sizeof(uint8), &uint);}
	operator uint8() const				{uint8 uint = 0; GetInt(sizeof(uint8), &uint); return uint;}
	CVariant(const sint16& sint)		{InitValue(ESInt, sizeof(sint16), &sint);}
	operator sint16() const				{sint16 sint = 0; GetInt(sizeof(sint16), &sint); return sint;}
	CVariant(const uint16& uint)		{InitValue(EUInt, sizeof(uint16), &uint);}
	operator uint16() const				{uint16 uint = 0; GetInt(sizeof(uint16), &uint); return uint;}
	CVariant(const sint32& sint)		{InitValue(ESInt, sizeof(sint32), &sint);}
	operator sint32() const				{sint32 sint = 0; GetInt(sizeof(sint32), &sint); return sint;}
	CVariant(const uint32& uint)		{InitValue(EUInt, sizeof(uint32), &uint);}
	operator uint32() const				{uint32 uint = 0; GetInt(sizeof(uint32), &uint); return uint;}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	CVariant(const time_t& time)		{sint64 uint = time; InitValue(EUInt, sizeof(sint64), &uint);}
	operator time_t() const				{sint64 uint = 0; GetInt(sizeof(sint64), &uint); return uint;}
#endif
	CVariant(const sint64& sint)		{InitValue(ESInt, sizeof(sint64), &sint);}
	operator sint64() const				{sint64 sint = 0; GetInt(sizeof(sint64), &sint); return sint;}
	CVariant(const uint64& uint)		{InitValue(EUInt, sizeof(uint64), &uint);}
	operator uint64() const				{uint64 uint = 0; GetInt(sizeof(uint64), &uint); return uint;}

	CVariant(const float& val)			{InitValue(EDouble, sizeof(float), &val);}
	operator float() const				{float val = 0; GetDouble(sizeof(float), &val); return val;}
	CVariant(const double& val)			{InitValue(EDouble, sizeof(double), &val);}
	operator double() const				{double val = 0; GetDouble(sizeof(double), &val); return val;}

	CVariant(const string& str)			{InitValue(EAscii, str.length(), str.c_str());}
	CVariant(const char* str)			{InitValue(EAscii, strlen(str), str);}
	operator string() const				{return ToString();}
	CVariant(const wstring& wstr);
	CVariant(const wchar_t* wstr);
	operator wstring() const			{return ToWString();}

protected:
	void					Assign(const CVariant& Variant);
	void					Extend();

	void					InitValue(EType Type, size_t Size, const void* Value, bool bTake = false);
	void					GetInt(size_t Size, void* Value) const;
	void					GetDouble(size_t Size, void* Value) const;

	string					ToString() const;
	wstring					ToWString() const;


	enum EAccess
	{
		eReadWrite,
		eReadOnly,
		eDerived
	};

	struct SVariant
	{
		SVariant();
		~SVariant();

		void				Init(EType type, size_t size = 0, const void* payload = 0, bool bTake = false);
		SVariant*			Clone(bool Full = true) const;

		bool				IsRaw() const {return Type != EExtended && Type != EMap && Type != EList;}

		uint32				Count() const;

		const string&		Key(uint32 Index) const;
		CVariant&			At(const char* Name) const;
		bool				Has(const char* Name) const;
		void				Remove(const char* Name);

		CVariant&			Insert(const char* Name, const CVariant& Variant);
		CVariant&			Append(const CVariant& Variant);
		CVariant&			At(uint32 Index) const;
		void				Remove(uint32 Index);

		EType				Type;
		uint32				Size;
		byte*				Payload;
		mutable union UContainer
		{
			void*					Void;
			map<string, CVariant>*	Map;
			vector<CVariant>*		List;
		}					Container;
		map<string, CVariant>*	Map() const;
		vector<CVariant>*		List() const;
		void					MkPayload(CBuffer& Payload) const;
		EAccess				Access;

		int					Refs;
	}*						m_Variant;

	void					Attach(SVariant* Variant);
	const SVariant*			Val() const	{if(!m_Variant) ((CVariant*)this)->InitValue(EInvalid,0,NULL); return m_Variant;}
	SVariant*				Val() {if(!m_Variant) InitValue(EInvalid,0,NULL); else if(m_Variant->Refs > 1) Detach(); return m_Variant;}
	void					Detach();
};

__inline bool operator<(const CVariant &L, const CVariant &R) {return L.Compare(R) > 0;}

wstring		ToHex(const byte* Data, size_t uSize);
CBuffer		FromHex(wstring Hex);

void MakePacket(const string& Name, const CVariant& Packet, CBuffer& Buffer);
bool StreamPacket(CBuffer& Buffer, string& Name, CVariant& Packet);
