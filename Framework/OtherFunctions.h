#pragma once

#include "./NeoHelper/neohelper_global.h"

/////////////////////////
// HeapSort

template <class T>
void HeapSortAux(T &refArray, uint32 first, uint32 last, void (*HeapMov)(T &, uint32, uint32), bool (*HeapCmp)(T &, uint32, uint32))
{
	uint32 r;
	for (r = first; !(r & 0x80000000) && (r<<1) < last;)
	{
		uint32 r2 = (r<<1)+1;
		if (r2 != last)
		{
			if(HeapCmp(refArray, r2, r2+1))
				r2++;
		}
		if(HeapCmp(refArray, r, r2))
		{
			HeapMov(refArray, r2, r);
			r = r2;
		}
		else
			break;
	}
}

template <class T>
void HeapSort(T &refArray, void (*HeapMov)(T &, uint32, uint32), bool (*HeapCmp)(T &, uint32, uint32))
{
	int n = refArray.size();
	if (n > 0)
	{
		int r;
		for (r = n/2; r--;)
			HeapSortAux(refArray, r, n-1, HeapMov, HeapCmp);
		for (r = n; --r;)
		{
			HeapMov(refArray, r, 0);
			HeapSortAux(refArray, 0, r-1, HeapMov, HeapCmp);
		}
	}
}

/////////////////////////
// MergeSort

template<typename I> void MergeSortAux(I& lst, I& res, uint32 begin, uint32 middle, uint32 end, void (*Cpy)(I& T, I& S, uint32 t, uint32 s), bool (*Cmp)(I& A, uint32 l, uint32 r))
{
	uint32 a = begin;
	uint32 b = middle;
	uint32 r = 0;

	while (a < middle && b < end)
	{
		if (Cmp(lst, a, b)) 
			Cpy(res, lst, r++, a++);
		else
			Cpy(res, lst, r++, b++);
	}

	while (a < middle) 
		Cpy(res, lst, r++, a++);
	while (b < end) 
		Cpy(res, lst, r++, b++);

	a = begin;
	r = 0;
	while (a < end) 
		Cpy(lst, res, a++, r++);
}

template<typename I> void MergeSort(I& lst, I& res, int begin, int end, void (*Cpy)(I& T, I& S, uint32 t, uint32 s), bool (*Cmp)(I& A, uint32 l, uint32 r))
{
	uint32 s = end-begin;
	if (s > 1)
	{
		uint32 middle = begin+s/2;
		MergeSort(lst, res, begin, middle, Cpy, Cmp);
		MergeSort(lst, res, middle, end, Cpy, Cmp);
		MergeSortAux(lst, res, begin, middle, end, Cpy, Cmp);
	}
}

template<typename I> void MergeSort(I& lst, void (*Cpy)(I& T, I& S, uint32 t, uint32 s), bool (*Cmp)(I& A, uint32 l, uint32 r))
{
	I res = lst; // temporary space
	MergeSort(lst, res, 0, lst.size(), Cpy, Cmp);
}

/////////////////////////
// Reverse

template <class T>
void Reverse(T* Data, size_t Size)
{
	for(size_t i=0; i < Size/2; i++)
	{
		T Temp = Data[i];
		Data[i] = Data[Size - 1 - i];
		Data[Size - 1 - i] = Temp;
	}
}

//inline QString	tr(const char* s)							{return QObject::tr(s);}

//////////////////////////////////////////////////////////////////////////////////////////
// File system functions
// 



NEOHELPER_EXPORT QString		ReadFileAsString(const QString& filename);
NEOHELPER_EXPORT bool			WriteStringToFile(const QString& filename, const QString& content);
NEOHELPER_EXPORT bool			CreateDir(const QString& path);
NEOHELPER_EXPORT bool			DeleteDir(const QString& path, bool bEmpty = false);
NEOHELPER_EXPORT bool			CopyDir(const QString& srcDirPath, const QString& destDirPath, bool bMove = false);
NEOHELPER_EXPORT QStringList	ListDir(const QString& srcDirPath);
NEOHELPER_EXPORT bool			SafeRemove(const QString& path);

NEOHELPER_EXPORT QString GetRelativeSharedPath(const QString& fullPath, const QStringList& shared, QString& rootPath);

NEOHELPER_EXPORT QString NameOfFile(const QString& FileName);
