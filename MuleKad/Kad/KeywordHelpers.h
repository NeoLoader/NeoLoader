#pragma once
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

#include "../../Framework/Buffer.h"

struct SSearchTree
{
	SSearchTree(){
		Type = AND;
		Left = NULL;
		Right = NULL;
	}
	~SSearchTree()
	{
		delete Left;
		delete Right;
	}
	
	enum ESearchTermType {
		AND,
		OR,
		NAND,
		String,
	}				Type;
	vector<wstring>	Strings;
	SSearchTree*	Left;
	SSearchTree*	Right;

	QVariant ToVariant();
	void	 FromVariant(QVariant Variant);
};

struct SSearchRoot
{
	SSearchRoot()
	{
		pSearchTree = NULL;
		minSize = 0;
		maxSize = 0;
		availability = 0;
	}
	~SSearchRoot()
	{
		delete pSearchTree;
	}

	SSearchTree* pSearchTree;

	wstring typeText;
	wstring extension;
	uint64 minSize;
	uint64 maxSize;
	uint32 availability;
};

void WriteSearchTree(CBuffer& Packet, const SSearchRoot& SearchRoot, bool bSupports64bit = true, bool bSupportsUnicode = true);
wstring PrintSearchTree(SSearchTree* pSearchTree);