#include "GlobalHeader.h"
#include "TuringTools.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
// SExpression

SExpression::SExpression(const wstring &Word)
{
	ASSERT(!Word.empty());
	if(CTuringOps::IsOperator(Word.at(0)))
		Type = eOperator;
	else if(Word.at(0) == L'"')
		Type = eString;
	else
		Type = eWord;
	Exp.Word = new wstring(Word);
}
SExpression::SExpression(CExpressions* Expressions)
{
	Type = eMulti;
	Exp.Multi = Expressions;
}
SExpression::~SExpression()
{
	if(Type != eMulti)
		delete Exp.Word;
	else if(Exp.Multi)
		delete Exp.Multi;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CExpressions

CExpressions::~CExpressions()
{
	for (vector<SExpression*>::iterator K = m_Expressions.begin(); K != m_Expressions.end(); K++)
		delete (*K);
}

SExpression** CExpressions::GetExp(UINT Index)
{
	if(m_Expressions.size() <= Index)
		return NULL;
	SExpression** Ret = &m_Expressions.at(Index);
	ASSERT(*Ret != NULL);
	return Ret;
}

wstring CExpressions::GetExpStr(UINT Index)
{
	SExpression** Expression = GetExp(Index);
	if(!Expression || (*Expression)->IsMulti())
		return L"";
	return *(*Expression)->Exp.Word;
}

bool CExpressions::IsExpMulti(UINT Index)
{
	SExpression** Expression = GetExp(Index);
	if(!Expression)
		return false;
	return (*Expression)->IsMulti();
}

bool CExpressions::IsExpOp(UINT Index)
{
	SExpression** Expression = GetExp(Index);
	if(!Expression)
		return false;
	return (*Expression)->IsOperator();
}

bool CExpressions::SubordinateExp(int Index, int ToGo)
{
	if(ToGo == 0)
		return true;
	if(Index < 0 || Index + ToGo > m_Expressions.size())
		return false;

	CExpressions* SubExpression = new CExpressions;
	for (; ToGo > 0; ToGo--)
    {
		vector<SExpression*>::iterator I = m_Expressions.begin() + Index;
		SubExpression->m_Expressions.push_back(*I);
		m_Expressions.erase(I);
	}
	m_Expressions.insert(m_Expressions.begin() + Index, new SExpression(SubExpression));
	return true;
}

void CExpressions::Del(UINT Index)
{
	vector<SExpression*>::iterator I = m_Expressions.begin() + Index;
	if(I != m_Expressions.end())
	{
		delete *I;
		m_Expressions.erase(I);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////
// CTuringParser

CExpressions* CTuringParser::GetExpressions(const wstring &Line, wstring* Comment)
{
	CExpressions* Expressions = new CExpressions;
	wstring Expression;

	bool bEsc = false;
	bool bString = false;
	int iBlocks = 0;
	int iBrackets = 0;
	bool bOperator = false;

	for(wstring::size_type i = 0; i < Line.size(); i++)
	{
		wchar_t Char = Line.at(i);

		int iEnd = 0;
		if(bString) // inside a string
		{
			if(bEsc)
			{
				bEsc = false;

				if(iBrackets == 0 && iBlocks == 0) // we interprete the ESC sequence only in the last dissection stage
				{
					switch(Char)
					{
						case L'\\':	Expression += L'\\';	break;
						case L'\'':	Expression += L'\'';	break;
						case L'\"':	Expression += L'\"';	break;
						case L'a':	Expression += L'\a';	break;
						case L'b':	Expression += L'\b';	break;
						case L'f':	Expression += L'\f';	break;
						case L'n':	Expression += L'\n';	break;
						case L'r':	Expression += L'\r';	break;
						case L't':	Expression += L'\t';	break;
						case L'v':	Expression += L'\v';	break;
						default:	Expression += L'?';		break;
					}
				}
				else
				{
					Expression += L'\\' ;
					Expression += Char;
				}
				continue;
			}
			else if(Char == L'\\') // ESC sequence handling
			{
				bEsc = true;
				continue;
			}

			if(Char == L'"') // end of a string is always end of expression
			{
				bString = false;
				if(iBrackets == 0 && iBlocks == 0)
					iEnd = 1;
			}
		}
		else if(Char == L'"') // begin of a string
		{
			bString = true;
			if(iBrackets == 0 && iBlocks == 0)
				iEnd = -1;
		}
		else if(Char == L'\'')
		{
			if(Comment)
				*Comment = Line.substr(i);
			break; // from here a comment starts
		}

		else if(iBlocks || Char == L'[') // a block
		{
			if(Char == L'[')
				iBlocks++;
			else if(Char == L']')
				iBlocks--;
		}

		else if(Char == L'(')
		{
			bOperator = false;
			if(iBrackets == 0)
				iEnd = 2; //-1;
			iBrackets++;
		}
		else if(Char == L')')
		{
			iBrackets--;
			if(iBrackets == 0)
				iEnd = -2; //1;
		}

		else if (iBrackets == 0)
		{
			if(Char == L' ' || Char == L'\t' || Char == L'\r' || Char == L'\n')
				iEnd = 2;
			else if(CTuringOps::IsOperator(Char) != bOperator)
			//else if(bOperator 
			// ? (!IsOperator(Char) && Expression[0] != L'%') // Note: the % can be extended with a text, it must be terminated with a white space
			// : (IsOperator(Char)))
			{
				bOperator = !bOperator;
				iEnd = -1;
			}
		}

		if(iEnd == 0 || iEnd == 1)
			Expression += Char;
		if(iEnd != 0)
		{
			if(iEnd == -2)
			{
				CExpressions* SubExpressions = GetExpressions(Expression);
				if(SubExpressions)
					Expressions->Add(new SExpression(SubExpressions));
				else
				{
					delete Expressions;
					return NULL;
				}
			}
			else if(!Expression.empty())
				Expressions->Add(new SExpression(Expression));
			Expression.clear();
		}
		if(iEnd == -1)
			Expression += Char;
	}

	if(!Expression.empty())
		Expressions->Add(new SExpression(Expression));

	if(bString || iBrackets)
	{
		delete Expressions;
		return NULL;
	}
	return Expressions;
}

wstring EscapeString(wstring str)
{
	for(string::size_type i=0; i<str.size(); i++)
	{
		switch(str[i])
		{
			case L'\\': str[i] = L'\\';	break;
			case L'\'': str[i] = L'\'';	break;
			case L'\"': str[i] = L'\"';	break;
			case L'\a': str[i] = L'a';	break;
			case L'\b': str[i] = L'b';	break;
			case L'\f': str[i] = L'f';	break;
			case L'\n': str[i] = L'n';	break;
			case L'\r': str[i] = L'r';	break;
			case L'\t': str[i] = L't';	break;
			case L'\v': str[i] = L'v';	break;
			default:
				continue;
		}
		str.insert(i,L"\\"); 
		i++;
	}
	return str;
}

wstring CTuringParser::PrintExpressions(CExpressions* Expressions)
{
	wstring Printer;
	for(UINT i=0; i < Expressions->Count(); i++)
	{
		SExpression* Expression = *Expressions->GetExp(i);
		if(!Printer.empty() && Printer.at(Printer.size()-1) != L' ')
			Printer.append(L" ");

		if(Expression->IsMulti())
		{
			Printer.append(L"(");
			if(Expression->Exp.Multi) // in some cases the item might have been detached
				Printer.append(PrintExpressions(Expression->Exp.Multi));
			Printer.append(L")");
		}
		else if(Expression->IsString())
			Printer.append(L"\"" + EscapeString(Expression->Exp.Word->substr(1, Expression->Exp.Word->size()-2)) + L"\"");
		else
			Printer.append(*Expression->Exp.Word);
	}
	return Printer;
}

////////////////////////////////////////////////////////////////////////////////////////////
// CTuringOps

/*
*	Operators:	"*+-/%&^|!=<>,:?~"
*	Brackets:	"()[]"
*	Modifyers:	".#"
*	Numbers:	"0123456789"
*	Unused:		"{};$@"
*/

wstring g_Operators = L"*+-/%&^|!=<>,:?~";

bool CTuringOps::IsOperator(wchar_t Char)
{
	return g_Operators.find_first_of(Char) != wstring::npos;
}