#pragma once

#include "../Common/Variant.h"

class CUIntX
{
public:
	CUIntX(){}

	void				SetRandomValue();

	bool				GetBit(UINT uBit, bool bBigEndian = true) const;
	void				SetBit(UINT uBit, bool bValue, bool bBigEndian = true);

	void				And(const CUIntX& uValue);
	void				Or(const CUIntX& uValue);
	void				Xor(const CUIntX& uValue);
	void				Add(const CUIntX& uValue);
	void				Subtract(const CUIntX& uValue);
	void				Multi(sint32 uValue);
	void				Div(sint32 uValue);

	bool				IsNull() const;
	int					CompareTo(const CUIntX& uOther) const;

	void				ShiftLeft(UINT uBits);
	void				ShiftRight(UINT uBits);
	void				Invert();

	wstring				ToHex() const;
	bool				FromHex(const wstring& Hex);
	wstring				ToBin() const;

	virtual byte*		GetData() = 0;
	virtual const byte*	GetData() const = 0;
	virtual size_t		GetSize() const = 0;
	virtual UINT		GetBitSize() const = 0;

	virtual void		SetDWord(UINT i, uint32 v) = 0;
	virtual uint32		GetDWord(UINT i) const = 0;
	virtual UINT		GetDWordCount() const = 0;

protected:
	void				Init(bool bFill);
	void				Init(const CUIntX& uValue, UINT uBits, bool bRand);
	void				Init(sint32 uValue);

	void				Init(const CVariant& Variant);
};

template <class T>
class CUIntXtmpl: public CUIntX
{
public:
	CUIntXtmpl(bool bFill = false)							{Init(bFill);}
	CUIntXtmpl(const CUIntXtmpl& uValue, UINT uBits = -1, bool bRand = true)	{Init(uValue, uBits, bRand);}
	CUIntXtmpl(sint32 uValue)								{Init(uValue);}

	CUIntXtmpl(const CVariant& Variant)						{Init(Variant);}

	virtual void		SetValue(const CUIntXtmpl& uValue){m_Data = uValue.m_Data;}

	operator CVariant() const								{return CVariant(GetData(),GetSize(),CVariant::EUInt);}

	//CUIntXtmpl& operator^	(const CUIntXtmpl &uValue)		{Xor(uValue); return *this;}
	CUIntXtmpl operator^	(const CUIntXtmpl &uValue) const{CUIntXtmpl This = *this; This.Xor(uValue); return This;}
	//CUIntXtmpl& operator+	(const CUIntXtmpl &uValue)		{Add(uValue); return *this;}
	CUIntXtmpl operator+	(const CUIntXtmpl &uValue) const{CUIntXtmpl This = *this; This.Add(uValue); return This;}
	//CUIntXtmpl& operator-	(const CUIntXtmpl &uValue)		{Subtract(uValue); return *this;}
	CUIntXtmpl operator-	(const CUIntXtmpl &uValue) const{CUIntXtmpl This = *this; This.Subtract(uValue); return This;}
	//CUIntXtmpl& operator*	(sint32 uValue)					{Multi(uValue); return *this;}
	CUIntXtmpl operator*	(sint32 uValue) const			{CUIntXtmpl This = *this; This.Multi(uValue); return This;}
	//CUIntXtmpl& operator/	(sint32 uValue)					{Div(uValue); return *this;}
	CUIntXtmpl operator/	(sint32 uValue) const			{CUIntXtmpl This = *this; This.Div(uValue); return This;}
	CUIntXtmpl& operator=	(const CUIntXtmpl &uValue)		{SetValue(uValue); return *this;}
	bool operator<		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) <  0);}
	bool operator>		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) >  0);}
	bool operator<=		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) <= 0);}
	bool operator>=		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) >= 0);}
	bool operator==		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) == 0);}
	bool operator!=		(const CUIntXtmpl &uValue) const	{return (CompareTo(uValue) != 0);}
	//CUIntXtmpl& operator<<=(UINT uBits)  					{ShiftLeft(uBits); return *this;}
	CUIntXtmpl  operator<< (UINT uBits) const  				{CUIntXtmpl This = *this; This.ShiftLeft(uBits); return This;}
	//CUIntXtmpl& operator>>=(UINT uBits) 					{ShiftRight(uBits); return *this;}
	CUIntXtmpl  operator>> (UINT uBits) const 	 			{CUIntXtmpl This = *this; This.ShiftRight(uBits); return This;}



	virtual byte*		GetData()							{return m_Data.Bytes;}
	virtual const byte*	GetData() const						{return m_Data.Bytes;}
	virtual size_t		GetSize() const 					{return GetStaticSize();}
	virtual UINT		GetBitSize() const 					{return sizeof(T)*8;}
	static size_t		GetStaticSize()						{return sizeof(T);}

	virtual void		SetDWord(UINT i, uint32 v)			{m_Data.int32[i] = v;}
	virtual uint32		GetDWord(UINT i) const				{return m_Data.int32[i];}
	virtual UINT		GetDWordCount()	const 				{return (UINT)(sizeof(T)/sizeof(uint32));}

protected:
	T					m_Data;
};

union uUInt128
{
	byte	Bytes[16];
	uint32	int32[4];
};

typedef CUIntXtmpl<uUInt128> CUInt128;