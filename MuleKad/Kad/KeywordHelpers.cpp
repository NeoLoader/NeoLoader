//
// This file is part of the MuleKad Project.
//
// Copyright (c) 2013 David Xanatos ( XanatosDavid@googlemail.com )
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
#include "Types.h"
#include "KeywordHelpers.h"
#include "FileTags.h"

QVariant SSearchTree::ToVariant()
{
	QVariantMap Map;

	if(Type == AND)
		Map["Type"] = "AND";
	else if(Type == OR)
		Map["Type"] = "OR";
	else if(Type == NAND)
		Map["Type"] = "NOT";
	else
	{
		Map["Type"] = "String";

		QStringList StringList;
		for(int i = 0; i < Strings.size(); i++)
			StringList.append(QString::fromStdWString(Strings[i]));
		Map["Value"] = StringList;
	}

	if(Left)
		Map["Left"] = Left->ToVariant();
	if(Right)
		Map["Right"] = Right->ToVariant();

	return Map;
}

void SSearchTree::FromVariant(QVariant Variant)
{
	QVariantMap Map = Variant.toMap();

	if(Map["Type"] == "AND")
		Type = AND;
	else if(Map["Type"] == "OR")
		Type = OR;
	else if(Map["Type"] == "NOT")
		Type = NAND;
	else if(Map["Type"] == "String")
	{
		Type = String;
		foreach(const QString& Value, Map["Value"].toStringList())
			Strings.push_back(Value.toStdWString());
	}

	if(Map.contains("Left"))
	{
		Left = new SSearchTree();
		Left->FromVariant(Map["Left"]);
	}
	if(Map.contains("Right"))
	{
		Right = new SSearchTree();
		Right->FromVariant(Map["Right"]);
	}
}

wstring PrintSearchTree(SSearchTree* pSearchTree)
{
	if(!pSearchTree)
		return L"";
	wstring String;
	if(pSearchTree->Type == SSearchTree::String)
	{
		String += L" ";
		for(int i = 0; i < pSearchTree->Strings.size(); i++)
			String += pSearchTree->Strings[i] + L" ";
	}
	else
	{
		String += L" (";
		String += PrintSearchTree(pSearchTree->Left);
		if(pSearchTree->Type == SSearchTree::AND)
			String += L"AND";
		else if(pSearchTree->Type == SSearchTree::OR)
			String += L"OR";
		else if(pSearchTree->Type == SSearchTree::NAND)
			String += L"NOT";
		String += PrintSearchTree(pSearchTree->Right);
		String += L") ";
	}
	return String;
}

class CSearchTreeWriter
{
public:
	CSearchTreeWriter(CBuffer* pBuffer, bool bSupports64bit, bool bSupportsUnicode)
		: m_pBuffer(pBuffer),
		  m_Supports64bit(bSupports64bit),
		  m_bSupportsUnicode(bSupportsUnicode)
	{
	}

	void WriteAND()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x00);						// "AND"
	}

	void WriteOR()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x01);						// "OR"
	}

	void WriteNOT()
	{
		m_pBuffer->WriteValue<uint8>(0);						// boolean operator parameter type
		m_pBuffer->WriteValue<uint8>(0x02);						// "NOT"
	}

	void WriteString(const wstring& Value)
	{
		m_pBuffer->WriteValue<uint8>(1);						// string parameter type
		m_pBuffer->WriteString(Value,							// string value
			m_bSupportsUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);
	}

	void WriteParam(uint8 TagID, const wstring& Value, bool bForceAscii = false)
	{
		m_pBuffer->WriteValue<uint8>(2);						// string parameter type
		m_pBuffer->WriteString(Value,							// string value
			!bForceAscii && m_bSupportsUnicode ? CBuffer::eUtf8 : CBuffer::eAscii);	
		m_pBuffer->WriteValue<uint16>(sizeof(uint8));			// meta tag ID length
		m_pBuffer->WriteValue<uint8>(TagID);					// meta tag ID name
	}
	
	//void WriteParam(const wstring& TagID, const wstring& Value)
	//{
	//	m_pBuffer->WriteValue<uint8>(2);						// string parameter type
	//	m_pBuffer->WriteString(Value, m_eStrEncode);			// string value
	//	m_pBuffer->WriteString(TagID);							// meta tag ID
	//}

	void WriteParam(uint8 TagID, uint8 uOperator, uint64 Value)
	{
		bool Need64bit = Value > 0xFFFFFFFF;
		if (Need64bit && m_Supports64bit) 
		{
			m_pBuffer->WriteValue<uint8>(8);					// numeric parameter type (int64)
			m_pBuffer->WriteValue<uint64>(Value);				// numeric value
		} 
		else 
		{
			if (Need64bit)
				Value = 0xFFFFFFFF;
			m_pBuffer->WriteValue<uint8>(3);					// numeric parameter type (int32)
			m_pBuffer->WriteValue<uint32>(Value);				// numeric value
		}
		m_pBuffer->WriteValue<uint8>(uOperator);				// comparison operator
		m_pBuffer->WriteValue<uint16>(sizeof(uint8));			// meta tag ID length
		m_pBuffer->WriteValue<uint8>(TagID);					// meta tag ID name
	}

	//void WriteParam(const wstring& TagID, uint8 uOperator, uint64 Value)
	//{
	//	bool Need64bit = Value > 0xFFFFFFFF;
	//	if (Need64bit && m_Supports64bit) 
	//	{
	//		m_pBuffer->WriteValue<uint8>(8);					// numeric parameter type (int64)
	//		m_pBuffer->WriteValue<uint64>(Value);				// numeric value
	//	} 
	//	else 
	//	{
	//		if (Need64bit)
	//			Value = 0xFFFFFFFF;
	//		m_pBuffer->WriteValue<uint8>(3);					// numeric parameter type (int32)
	//		m_pBuffer->WriteValue<uint32>(Value);				// numeric value
	//	}
	//	m_pBuffer->WriteValue<uint8>(uOperator);				// comparison operator
	//	m_pBuffer->WriteString(TagID);							// meta tag ID
	//}

	void WriteStrings(vector<wstring> Value)
	{
		wstring String;
		for(int i = 0; i < Value.size(); i++)
		{
			if(i > 0)
				String += L" ";
			String += Value.at(i);
		}
		WriteString(String);
	}

	void WriteTree(SSearchTree* pSearchTree)
	{
		if(!pSearchTree)
			return;
		if(pSearchTree->Type == SSearchTree::String)
		{
			/*for(int i = 0; i < pSearchTree->Strings.size(); i++)
			{
				if(i > 0)
					WriteAND();
				WriteParam(pSearchTree->Strings.at(i));
			}*/
			WriteStrings(pSearchTree->Strings);
		}
		else
		{
			if(pSearchTree->Type == SSearchTree::AND)
				WriteAND();
			else if(pSearchTree->Type == SSearchTree::OR)
				WriteOR();
			else if(pSearchTree->Type == SSearchTree::NAND)
				WriteNOT();
			WriteTree(pSearchTree->Left);
			WriteTree(pSearchTree->Right);
		}
	}

protected:
	CBuffer*			m_pBuffer;
	bool				m_bSupportsUnicode;
	bool				m_Supports64bit;
};

void WriteSearchTree(CBuffer& Packet, const SSearchRoot& SearchRoot, bool bSupports64bit, bool bSupportsUnicode)
{
	int iParameterTotal = 0;
	if ( !SearchRoot.typeText.empty() )	iParameterTotal++;
	if ( SearchRoot.minSize > 0 )			iParameterTotal++;
	if ( SearchRoot.maxSize > 0 )			iParameterTotal++;
	if ( SearchRoot.availability > 0 )		iParameterTotal++;
	if ( !SearchRoot.extension.empty() )	iParameterTotal++;

	wstring typeText = SearchRoot.typeText;
	if (typeText == ED2KFTSTR_ARCHIVE){
		// eDonkeyHybrid 0.48 uses type "Pro" for archives files
		// www.filedonkey.com uses type "Pro" for archives files
		typeText = ED2KFTSTR_PROGRAM;
	} else if (typeText == ED2KFTSTR_CDIMAGE){
		// eDonkeyHybrid 0.48 uses *no* type for iso/nrg/cue/img files
		// www.filedonkey.com uses type "Pro" for CD-image files
		typeText = ED2KFTSTR_PROGRAM;
	}
	
	CSearchTreeWriter SearchTree(&Packet, bSupports64bit, bSupportsUnicode);

	int iParameterCount = 0;
	if (SearchRoot.pSearchTree->Type == SSearchTree::String) // && SearchRoot.pSearchTree->Strings.size() <= 1)
	{
		// lugdunummaster requested that searchs without OR or NOT operators,
		// and hence with no more expressions than the string itself, be sent
		// using a series of ANDed terms, intersecting the ANDs on the terms 
		// (but prepending them) instead of putting the boolean tree at the start 
		// like other searches. This type of search is supposed to take less load 
		// on servers. Go figure.
		//
		// input:      "a" AND min=1 AND max=2
		// instead of: AND AND "a" min=1 max=2
		// we use:     AND "a" AND min=1 max=2

		if(SearchRoot.pSearchTree->Strings.size() > 0) 
		{
			iParameterTotal += 1;

			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteStrings(SearchRoot.pSearchTree->Strings); //SearchTree.WriteParam(SearchRoot.pSearchTree->Strings.at(0));
		}

		if (!typeText.empty()) 
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILETYPE, typeText, true);
		}
		
		if (SearchRoot.minSize > 0) 
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILESIZE, 0x01, SearchRoot.minSize);
		}

		if (SearchRoot.maxSize > 0)
		{
			if (++iParameterCount < iParameterTotal) 
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILESIZE, 0x02, SearchRoot.maxSize);
		}
		
		if (SearchRoot.availability > 0)
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_SOURCES, 0x01, SearchRoot.availability);
		}

		if (!SearchRoot.extension.empty())
		{
			if (++iParameterCount < iParameterTotal)
				SearchTree.WriteAND();
			SearchTree.WriteParam(FT_FILEFORMAT, SearchRoot.extension);
		}
		
		// ...

		// If this assert fails... we're seriously fucked up 
		
		ASSERT( iParameterCount == iParameterTotal );
		
	} 
	else 
	{
		if (!SearchRoot.extension.empty() && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();

		if (SearchRoot.availability > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
	  
		if (SearchRoot.maxSize > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
        
		if (SearchRoot.minSize > 0 && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
        
		if (!typeText.empty() && ++iParameterCount < iParameterTotal)
			SearchTree.WriteAND();
 
		// ...

		// As above, if this fails, we're seriously fucked up.
		ASSERT( iParameterCount == iParameterTotal );

		SearchTree.WriteTree(SearchRoot.pSearchTree);

		if (!SearchRoot.typeText.empty())
			SearchTree.WriteParam(FT_FILETYPE, SearchRoot.typeText, true);

		if (SearchRoot.minSize > 0)
			SearchTree.WriteParam(FT_FILESIZE, 0x01, SearchRoot.minSize);

		if (SearchRoot.maxSize > 0)
			SearchTree.WriteParam(FT_FILESIZE, 0x02, SearchRoot.maxSize);

		if (SearchRoot.availability > 0)
			SearchTree.WriteParam(FT_SOURCES, 0x01, SearchRoot.availability);

		if (!SearchRoot.extension.empty())
			SearchTree.WriteParam(FT_FILEFORMAT, SearchRoot.extension);

		// ...
	}
}