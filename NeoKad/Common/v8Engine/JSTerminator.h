#pragma once

#include "JSEngine.h"
#include "../../../Framework/Strings.h"
#include "../MT/Thread.h"

class CJSTerminatorThread
{
public:
	CJSTerminatorThread(v8::Isolate* Isolate) : m_Thread(RunProc, this)
	{
		m_Isolate = Isolate;

		m_Active = false;
		m_TimeOut = 0;
		m_StartTime = 0;
	}
	~CJSTerminatorThread()
	{
		Reset(true);
	}

	void	Set(int TimeOut = 1500)
	{
		if(!m_Active)
		{
			m_Active = true;
			m_Thread.Start();
		}

		m_StartTime = GetCurTick();
		m_TimeOut = m_StartTime + TimeOut;
	}

	uint64	Reset(bool bStop = false)
	{
		if(bStop && m_Active)
		{
			m_Active = false;
			m_Thread.Stop();
		}

		m_TimeOut = 0;
		return GetCurTick() - m_StartTime;
	}

	static void RunProc(const void* param) 
	{
		((CJSTerminatorThread*)param)->Run();
	}

	void Run() 
	{
		while(m_Active)
		{
			CThread::Sleep(100);
			if(m_TimeOut && m_TimeOut < GetCurTick())
			{
				m_TimeOut = 0;

				// Issue execution Termination
				ASSERT(0);
				v8::V8::TerminateExecution(m_Isolate);

				// Wait for execution to terminate
				//while(v8::V8::IsExecutionTerminating(m_Isolate))
				//	Sleep(10);
			}
		}
	}

protected:
	CThread				m_Thread;
	v8::Isolate*		m_Isolate;

	volatile uint64		m_StartTime;
	volatile uint64		m_TimeOut;
	volatile bool		m_Active;
};

class CJSTerminator
{
public:
	CJSTerminator(CJSTerminatorThread* pThread, int TimeOut = 15000)
	{
		m_pThread = pThread;
		if(m_pThread)
			m_pThread->Set(TimeOut);
	}
	~CJSTerminator()
	{
		if(m_pThread)
			m_pThread->Reset();
	}

	bool HasException()
	{
		return m_TryCatch.HasCaught();
	}

	wstring GetException()
	{
		if(!HasException())
			return L"";

		v8::HandleScope HandleScope(v8::Isolate::GetCurrent());
		wstring ExceptionString = CJSEngine::GetWStr(m_TryCatch.Exception());
		v8::Local<v8::Message> Message = m_TryCatch.Message();
		if (Message.IsEmpty()) 
		{
			// V8 didn't provide any extra information about this error; just print the exception.
			return ExceptionString;
		}
		else 
		{
			wstring Exception;
			// Print (filename):(line number): (message).
			int LineNum = Message->GetLineNumber();
			Exception = CJSEngine::GetWStr(Message->GetScriptResourceName()) + L":" + int2wstring(LineNum) + L": \r\n" + ExceptionString + L"\r\n";

			// Print line of source code.
			Exception += CJSEngine::GetWStr(Message->GetSourceLine()) + L"\r\n";

			// Print wavy underline (GetUnderline is deprecated).
			wstring Line;
			wstring::size_type Start = Message->GetStartColumn();
			wstring::size_type End = Message->GetEndColumn();
			if(Start != wstring::npos)
			{
				Line.append(Start, L' ');
				ASSERT(End >= Start);
				Line.append(End - Start, L'^');
			}
			Exception += Line;

			return Exception;
		}
	}

protected:
	CJSTerminatorThread* m_pThread;
	v8::TryCatch m_TryCatch;
};
