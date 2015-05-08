#pragma once

class CExpressions;

///////////////////////////////////////////////////////////////////////////////////////////////////////
// SExpression

struct SExpression
{
	SExpression(const wstring &Word);
	SExpression(CExpressions* Expressions);
	~SExpression();

	__inline bool IsWord()		{return Type == eWord;}
	__inline bool IsString()	{return Type == eString;}
	__inline bool IsMulti()		{return Type == eMulti;}
	__inline bool IsOperator()	{return Type == eOperator;}

	enum
	{
		eWord = 0,
		eOperator,
		eString,
		eMulti
	}
	Type;
	union
	{
		wstring* Word;
		CExpressions* Multi;
	}
	Exp;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CExpressions

class CExpressions
{
public:
	~CExpressions();

	SExpression**	GetExp(UINT Index);
	wstring			GetExpStr(UINT Index);
	bool			IsExpMulti(UINT Index);
	bool			IsExpOp(UINT Index);

	bool			SubordinateExp(int Index, int ToGo);

	void			Add(SExpression* Expression)	{m_Expressions.push_back(Expression);}
	UINT			Count()							{return m_Expressions.size();}
	void			Del(UINT Index);

protected:
	vector<SExpression*> m_Expressions;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CTuringParser

class CTuringParser
{
public:

	static	CExpressions*	GetExpressions(const wstring &Line, wstring* Comment = NULL);
	static	wstring			PrintExpressions(CExpressions* Expressions);
};

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CTuringOps

class CTuringOps
{
public:
	static bool				IsOperator(wchar_t Char);
};