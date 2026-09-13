// =================================================================================================================================
// The MIT Lisence:
//
// Copyright (C) 2012 - 2016 Jarmo Nikkanen
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy,
// modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
// OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// =================================================================================================================================

#include <Windows.h>
#include "Log.h"
#include "D3D9Util.h"
#include "D3D9Config.h"
#include "D3D9Client.h"

FILE *d3d9client_log = NULL;

#define LOG_MAX_LINES 100000
#define ERRBUF 8000
#define OPRBUF 512
#define TIMEBUF 63

extern class D3D9Client* g_client;

char ErrBuf[ERRBUF+1];
char OprBuf[OPRBUF+1];
char TimeBuf[TIMEBUF+1];

time_t ltime;
int uEnableLog = 1;     // This value is controlling log opeation ( Config->DebugLvl )
int iEnableLog = 0;     // Index into EnableLogStack
int EnableLogStack[16];
int iLine = 0;          // Line number counter (iLine <= LOG_MAX_LINES)

__int64 qpcFrq = 0;     // Performance counter frequency
__int64 qpcRef = 0;     // Performance counter reference value (for "delta t")
__int64 qpcStart = 0;   // Performance counter start value ("zero")

std::queue<std::string> D3D9DebugQueue;

CRITICAL_SECTION LogCrit;


//-------------------------------------------------------------------------------------------
//
void MissingRuntimeError()
{
	MessageBoxA(NULL,
		"DirectX Runtimes may be missing. See /Doc/D3D9Client.pdf for more information",
		"D3D9Client Initialization Failed", MB_OK);
}

//-------------------------------------------------------------------------------------------
//
void FailedDeviceError()
{
	MessageBoxA(NULL,
		"DirectX9 Device Failed. Try to enable EnableDX12Wrapper from D3D9Client.cfg",
		"D3D9Client Initialization Failed", MB_OK);
}

//-------------------------------------------------------------------------------------------
//
void RuntimeError(const char* File, const char* Fnc, UINT Line)
{
	if (Config->DebugLvl == 0) return;
	char buf[256];
	sprintf_s(buf, 256, "[%s] [%s] Line: %u See Orbiter.log for details.", File, Fnc, Line);
	MessageBoxA(g_client->GetRenderWindow(), buf, "Critical Error:", MB_OK);
	DebugBreak();
}

//-------------------------------------------------------------------------------------------
//
/*
int PrintModules(DWORD pAdr)
{
	HMODULE hMods[1024];
	HANDLE hProcess;
	DWORD cbNeeded;
	unsigned int i;

	// Get a handle to the process.

	hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetProcessId(GetCurrentProcess()));

	if (NULL == hProcess) return 1;

	if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
		for (i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
			char szModName[MAX_PATH];
			if (GetModuleFileNameExA(hProcess, hMods[i], szModName, sizeof(szModName))) {
				MODULEINFO mi;
				GetModuleInformation(hProcess, hMods[i], &mi, sizeof(MODULEINFO));
				DWORD Base = (DWORD)mi.lpBaseOfDll;
				if (pAdr > Base && pAdr < (Base + mi.SizeOfImage)) LogErr("%s EntryPoint=0x%8.8X, Base=0x%8.8X, Size=%u", szModName, mi.EntryPoint, mi.lpBaseOfDll, mi.SizeOfImage);
				else										 LogOk("%s EntryPoint=0x%8.8X, Base=0x%8.8X, Size=%u", szModName, mi.EntryPoint, mi.lpBaseOfDll, mi.SizeOfImage);
			}
		}
	}
	CloseHandle(hProcess);
	return 0;
}
*/


//-------------------------------------------------------------------------------------------
// Log OAPISURFACE_xxx attributes
void LogAttribs(DWORD attrib, DWORD w, DWORD h, LPCSTR origin)
{
	char buf[512];
	sprintf_s(buf, 512, "%s (%d,%d)[0x%X]: ", origin, w, h, attrib);
	if (attrib&OAPISURFACE_TEXTURE)		 strcat_s(buf, 512, "OAPISURFACE_TEXTURE ");
	if (attrib&OAPISURFACE_RENDERTARGET) strcat_s(buf, 512, "OAPISURFACE_RENDERTARGET ");
	if (attrib&OAPISURFACE_GDI)			 strcat_s(buf, 512, "OAPISURFACE_GDI ");
	if (attrib&OAPISURFACE_SKETCHPAD)	 strcat_s(buf, 512, "OAPISURFACE_SKETCHPAD ");
	if (attrib&OAPISURFACE_MIPMAPS)		 strcat_s(buf, 512, "OAPISURFACE_MIPMAPS ");
	if (attrib&OAPISURFACE_NOMIPMAPS)	 strcat_s(buf, 512, "OAPISURFACE_NOMIPMAPS ");
	if (attrib&OAPISURFACE_ALPHA)		 strcat_s(buf, 512, "OAPISURFACE_ALPHA ");
	if (attrib&OAPISURFACE_NOALPHA)		 strcat_s(buf, 512, "OAPISURFACE_NOALPHA ");
	if (attrib&OAPISURFACE_UNCOMPRESS)	 strcat_s(buf, 512, "OAPISURFACE_UNCOMPRESS ");
	if (attrib&OAPISURFACE_SYSMEM)		 strcat_s(buf, 512, "OAPISURFACE_SYSMEM ");
	LogDbg("BlueViolet", buf);
}

//-------------------------------------------------------------------------------------------
//
void D3D9DebugLog(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
	va_end(args);

	D3D9DebugQueue.push(std::string(ErrBuf));
}

//-------------------------------------------------------------------------------------------
//
void D3D9DebugLogVec(const char* lbl, oapi::FVECTOR4 &v)
{
	sprintf_s(ErrBuf, ERRBUF, "%s = [%f, %f, %f, %f]", lbl, v.x, v.y, v.z, v.w);
	D3D9DebugQueue.push(std::string(ErrBuf));
}

//-------------------------------------------------------------------------------------------
//
void D3D9InitLog(const char *file)
{
	QueryPerformanceFrequency((LARGE_INTEGER*)&qpcFrq);
	QueryPerformanceCounter((LARGE_INTEGER*)&qpcStart);

	if (fopen_s(&d3d9client_log,file,"w+")) { d3d9client_log=NULL; } // Failed
	else {
		QueryPerformanceCounter((LARGE_INTEGER*)&qpcRef);
		InitializeCriticalSectionAndSpinCount(&LogCrit, 256);
		fprintf_s(d3d9client_log,"<!DOCTYPE html><html><head><title>D3D9Client Log</title></head><body bgcolor=black text=white>");
		fprintf_s(d3d9client_log,"<center><h2>D3D9Client Log</h2><br>");
		fprintf_s(d3d9client_log,"</center><hr><br><br>");
	}
}

//-------------------------------------------------------------------------------------------
//
void D3D9CloseLog()
{
	if (d3d9client_log) {
		fprintf(d3d9client_log,"</body></html>");
		fclose(d3d9client_log);
		d3d9client_log = NULL;
		DeleteCriticalSection(&LogCrit);
	}
}

//-------------------------------------------------------------------------------------------
//
double D3D9GetTime()
{
	__int64 qpcCurrent;
	QueryPerformanceCounter((LARGE_INTEGER*)&qpcCurrent);
	return double(qpcCurrent) * 1e6 / double(qpcFrq);
}

//-------------------------------------------------------------------------------------------
//
void D3D9SetTime(D3D9Time &inout, double ref)
{
	__int64 qpcCurrent;
	QueryPerformanceCounter((LARGE_INTEGER*)&qpcCurrent);
	double time = double(qpcCurrent) * 1e6 / double(qpcFrq);
	inout.time += (time - ref);
	inout.count += 1.0;
	inout.peak = max((time - ref), inout.peak);
}

//-------------------------------------------------------------------------------------------
//
char *my_ctime()
{
	__int64 qpcCurrent;
	QueryPerformanceCounter((LARGE_INTEGER*)&qpcCurrent);
	double time = double(qpcCurrent-qpcRef) * 1e3 / double(qpcFrq);
	double start = double(qpcCurrent-qpcStart) / double(qpcFrq);
	sprintf_s(OprBuf,OPRBUF,"%d: %.1fs %05.2fms", iLine++, start, time);
	qpcRef = qpcCurrent;
	return OprBuf;
}

//-------------------------------------------------------------------------------------------
//
void escape_ErrBuf () {
	std::string buf(ErrBuf);
	size_t n = 0;
	n += replace_all(buf, "&", "&amp;");
	n += replace_all(buf, "<", "&lt;");
	n += replace_all(buf, ">", "&gt;");
	if (n) {
		strcpy_s(ErrBuf, ARRAYSIZE(ErrBuf), buf.c_str());
	}
}

//-------------------------------------------------------------------------------------------
//
void LogTrace(const char *format, ...)
{
	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>3) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=DarkGrey> ", my_ctime(), th);

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		LeaveCriticalSection(&LogCrit);
	}
}

//-------------------------------------------------------------------------------------------
//
void LogAlw(const char *format, ...)
{
	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>0) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=Olive> ", my_ctime(), th);

		va_list args;
		va_start(args, format);

		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);

		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		LeaveCriticalSection(&LogCrit);
	}
}

//-------------------------------------------------------------------------------------------
//
void LogDbg(const char *color, const char *format, ...)
{
	if (d3d9client_log == NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>2) {
		EnterCriticalSection(&LogCrit);

		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=%s> ", my_ctime(), th, color);

		va_list args;
		va_start(args, format);

		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);

		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf, d3d9client_log);
		fputs("</font><br>\n", d3d9client_log);
		fflush(d3d9client_log);

		LeaveCriticalSection(&LogCrit);
	}
}

//-------------------------------------------------------------------------------------------
//
void LogClr(const char *color, const char *format, ...)
{
	if (d3d9client_log == NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>1) {
		EnterCriticalSection(&LogCrit);

		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=%s> ", my_ctime(), th, color);

		va_list args;
		va_start(args, format);

		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);

		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf, d3d9client_log);
		fputs("</font><br>\n", d3d9client_log);
		fflush(d3d9client_log);

		LeaveCriticalSection(&LogCrit);
	}
}

//-------------------------------------------------------------------------------------------
//
void LogOapi(const char *format, ...)
{

	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>0) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=Olive> ", my_ctime(), th);

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		oapiWriteLogV("D3D9: %s", ErrBuf);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		LeaveCriticalSection(&LogCrit);
	}
}

// ---------------------------------------------------
//
// ===========================================================================
// ORO PATCH (al) - A REPEATING ERROR IS COLLAPSED
// ---------------------------------------------------------------------------
// An error that recurs every frame writes a line every frame, in Orbiter.log as
// well as here, and DebugLvl defaults to 1 - so it reaches EVERY user, not just
// someone debugging. One stale addon SetFloat produced 16,252 identical lines in
// a twenty-minute flight, out of a 17,134-line Orbiter.log: the log a tester is
// then asked to send in becomes useless at exactly the moment it matters.
//
// LOG_MAX_LINES does bound it, at 100,000 - six times that flood, so the safety
// net is set far above the damage.
//
// So a message identical to the last one is COUNTED rather than written, and the
// count is flushed at most once per REAL second, carrying the tally. Nothing is
// lost: "[repeated 79 times in the last second]" says more than 79 identical
// lines do, and says it where it can be seen. A different message closes the
// previous one's tally first, so no count is ever dropped.
//
// Errors only. LogWrn is gated on uEnableLog > 1, above the default, so it never
// reaches an ordinary user's log; the informational writers are left alone
// because collapsing those could hide a sequence that matters.
//
// Same family as patch (q): a stock defect, reproducible with no addon loaded,
// that exists because nothing else keeps Orbiter.log worth reading.
// ===========================================================================
static char  oroLastErr[ERRBUF + 1] = "";
static DWORD oroLastErrTick = 0;
static DWORD oroErrRepeat   = 0;

// Writes whatever is in ErrBuf to both logs. ErrBuf is ESCAPED IN PLACE here, so
// any copy of the raw text must be taken before calling this.
static void oroWriteErrLine(DWORD th)
{
	fprintf(d3d9client_log,"<font color=Gray>(%s)(0x%lX)</font><font color=Red> [ERROR] ", my_ctime(), th);
	oapiWriteLogV("D3D9ERROR: %s", ErrBuf);
	escape_ErrBuf();
	fputs(ErrBuf,d3d9client_log);
	fputs("</font><br>\n",d3d9client_log);
	fflush(d3d9client_log);
}

void LogErr(const char *format, ...)
{
	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>0) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		// --- ORO patch (al): collapse a repeat -------------------------------
		const DWORD now = GetTickCount();
		if (strcmp(ErrBuf, oroLastErr) == 0) {
			oroErrRepeat++;
			// unsigned arithmetic, so the 49-day tick wrap is a non-event
			if ((DWORD)(now - oroLastErrTick) < 1000) {
				LeaveCriticalSection(&LogCrit);
				return;
			}
			_snprintf_s(ErrBuf, ERRBUF, ERRBUF, "%s   [repeated %u times in the last second]",
			            oroLastErr, oroErrRepeat);
			oroErrRepeat   = 0;
			oroLastErrTick = now;
			oroWriteErrLine(th);
			LeaveCriticalSection(&LogCrit);
			return;
		}
		if (oroErrRepeat) {          // a different error - close the old tally first
			char pending[ERRBUF + 1];
			strcpy_s(pending, sizeof(pending), ErrBuf);
			_snprintf_s(ErrBuf, ERRBUF, ERRBUF, "%s   [repeated %u more times]",
			            oroLastErr, oroErrRepeat);
			oroWriteErrLine(th);
			strcpy_s(ErrBuf, sizeof(ErrBuf), pending);
			oroErrRepeat = 0;
		}
		strcpy_s(oroLastErr, sizeof(oroLastErr), ErrBuf);   // the RAW text, before escaping
		oroLastErrTick = now;
		// --- end ORO patch (al) ----------------------------------------------

		oroWriteErrLine(th);
		LeaveCriticalSection(&LogCrit);
	}
}

// ---------------------------------------------------
//
void LogBlu(const char *format, ...)
{
	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>1) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log,"<font color=Gray>(%s)(0x%lX)</font><font color=#1E90FF> ", my_ctime(), th);

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		LeaveCriticalSection(&LogCrit);
	}
}

// ---------------------------------------------------
//
void LogWrn(const char *format, ...)
{
	if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>1) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log,"<font color=Gray>(%s)(0x%lX)</font><font color=Yellow> [WARNING] ", my_ctime(), th);

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		oapiWriteLogV("D3D9Info: %s", ErrBuf);
		LeaveCriticalSection(&LogCrit);
	}
}

// ---------------------------------------------------
//
void LogBreak(const char* format, ...)
{
	if (d3d9client_log == NULL) return;
	if (iLine > LOG_MAX_LINES) return;
	if (uEnableLog > 1) {

		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log, "<font color=Gray>(%s)(0x%lX)</font><font color=Yellow> [WARNING] ", my_ctime(), th);

		va_list args;
		va_start(args, format);
		_vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
		va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf, d3d9client_log);
		fputs("</font><br>\n", d3d9client_log);
		fflush(d3d9client_log);
		oapiWriteLogV("D3D9Debug: %s", ErrBuf);
		LeaveCriticalSection(&LogCrit);

		if (Config->DebugBreak) DebugBreak();
	}
}

// ---------------------------------------------------
//
void LogOk(const char *format, ...)
{
	/*if (d3d9client_log==NULL) return;
	if (iLine>LOG_MAX_LINES) return;
	if (uEnableLog>2) {
		EnterCriticalSection(&LogCrit);
		DWORD th = GetCurrentThreadId();
		fprintf(d3d9client_log,"<font color=Gray>(%s)(0x%lX)</font><font color=#00FF00> ", my_ctime(), th);

		va_list args;
		va_start(args, format);
        _vsnprintf_s(ErrBuf, ERRBUF, ERRBUF, format, args);
        va_end(args);

		escape_ErrBuf();
		fputs(ErrBuf,d3d9client_log);
		fputs("</font><br>\n",d3d9client_log);
		fflush(d3d9client_log);
		LeaveCriticalSection(&LogCrit);
	}*/
}
