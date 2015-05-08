#pragma once
//#include "GlobalHeader.h"
#include "../../Common/crc64.h"

inline bool IntegrityCheck()
{
	QMap<QString, uint64> Files;
#ifndef WIN32
	// Linux-ToDo
#else
	Files["NeoHelper.dll"] = 0x03bf453dbb0528b7;
	Files["cryptopp.dll"] = 0x6f83d101ac37fe74;
	Files["QtCore4.dll"] = 0xa04fa9899f72beb1;
	Files["QtNetwork4.dll"] = 0xb3f6522bed436d25;
#endif

	QStringList Paths = QCoreApplication::libraryPaths();

	CCRC64 CRC64;
	foreach(const QString& FileName, Files.keys())
	{
		QFile File;
		foreach(const QString& Path, Paths)
		{
			File.setFileName(Path + "/" + FileName);
			if(File.open(QFile::ReadOnly))
				break;
		}
		ASSERT(File.isOpen());
		const size_t BuffSize = 16*1024;
		char Buffer[BuffSize];
		for(quint64 uPos = 0; uPos < File.size();)
		{
			quint64 uRead = File.read(Buffer, BuffSize);
			if(uRead == -1)
				break;
			CRC64.Update(Buffer,uRead);
			uPos += uRead;
		}
		uint64 Test = CRC64.Finish();
		if(Test != Files[FileName])
			return false;
	}
	return true;
}

inline int ExceptionFilter(uint16 &Atempt, uint64 Tick)
{
	if(++Atempt > 20)
		return 0; //EXCEPTION_CONTINUE_SEARCH
	if(GetCurTick() - Tick > 500)
	{
		Tick = GetCurTick();
		return -1; //EXCEPTION_CONTINUE_EXECUTION;
	}
	return 1; //EXCEPTION_EXECUTE_HANDLER;
}

inline bool Int3Check()
{
	uint16 Atempt = 0;
	uint64 Tick = GetCurTick();
	__try
	{
		__asm int 3;
	}
	__except(ExceptionFilter(Atempt, Tick))
	{
		return false;
	}
	return true;
}

// The Int2DCheck function will check to see if a debugger
// is attached to the current process. It does this by setting up
// SEH and using the Int 2D instruction which will only cause an
// exception if there is no debugger. Also when used in OllyDBG
// it will skip a byte in the disassembly and will create
// some havoc.
inline bool Int2DCheck()
{
	uint16 Atempt = 0;
	uint64 Tick = GetCurTick();
	__try
	{
		__asm
		{
			int 0x2d
			xor eax, eax
			add eax, 2
		}
	}
	__except(ExceptionFilter(Atempt, Tick))
	{
		return false;
	}
	return true;
}

// The IsDbgPresentPrefixCheck works in at least two debuggers
// OllyDBG and VS 2008, by utilizing the way the debuggers handle
// prefixes we can determine their presence. Specifically if this code
// is ran under a debugger it will simply be stepped over;
// however, if there is no debugger SEH will fire :D
inline bool IsDbgPresentPrefixCheck()
{
	uint16 Atempt = 0;
	uint64 Tick = GetCurTick();
	__try
	{
		__asm __emit 0xF3 // 0xF3 0x64 disassembles as PREFIX REP:
		__asm __emit 0x64
		__asm __emit 0xF1 // One byte INT 1
	}
	__except(ExceptionFilter(Atempt, Tick))
	{
		return false;
	}
	return true;
}

inline bool IceBreakpointCheck()
{
	uint16 Atempt = 0;
	uint64 Tick = GetCurTick();
	int flag = 0;
	//Set the trap flag
	__try {
		__asm {
			PUSHFD; //Saves the flag registers
			OR BYTE PTR[ESP+1], 1; // Sets the Trap Flag in
			POPFD; //Restore the flag registers
			NOP; // NOP
		}
	}
	__except (ExceptionFilter(Atempt, Tick)) 
	{
		flag = 1;
		return false;
	}
	return true;
}

inline bool AntiDebug(bool bFull)
{
	bool bRet = false;

	if(!bRet && (bFull || rand() % 5 == 0) && Int3Check())
		bRet = true;
	if(!bRet && (bFull || rand() % 5 == 0) && Int2DCheck())
		bRet = true;
	if(!bRet && (bFull || rand() % 5 == 0) && IsDbgPresentPrefixCheck())
		bRet = true;
	if(!bRet && (bFull || rand() % 5 == 0) && IceBreakpointCheck())
		bRet = true;

	if(bRet)
		delete theCore;
	return bRet;
}

inline void AntiDebug()
{
	if(AntiDebug(false))
		return;

	if(!IntegrityCheck())
		delete theCore;
}