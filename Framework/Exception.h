#pragma once

#include "./NeoHelper/neohelper_global.h"

class CException
{
public:
	CException(uint32 uFlag) {m_uFlag = uFlag;}
	CException(uint32 uFlag, const wchar_t *sLine, ...)
	{
		m_uFlag = uFlag;

		va_list argptr; va_start(argptr, sLine); StrFormat(sLine, argptr); va_end(argptr);
	}

	uint32				GetFlag() const {return m_uFlag;}
	const wstring&		GetLine() const {return m_sLine;}

protected:
	void StrFormat(const wchar_t *sLine, va_list argptr)
	{
		ASSERT(sLine != NULL);

		const size_t bufferSize = 10241;
		wchar_t bufferline[bufferSize];
#ifndef WIN32
		if (vswprintf_l(bufferline, bufferSize, sLine, argptr) == -1)
#else
		if (vswprintf(bufferline, bufferSize, sLine, argptr) == -1)
#endif
			bufferline[bufferSize - 1] = L'\0';

		m_sLine = bufferline;
	}

	uint32 m_uFlag;
	wstring m_sLine;
};