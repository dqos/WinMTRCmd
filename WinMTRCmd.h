//*****************************************************************************
// FILE:            WinMTRCmd.h
//
//
// DESCRIPTION: The entry class for WinMTRCmd, fetches program parameters,
//              utilizes WinMTRNet to perform the mtr service and prints
//              reports.
//   
//
// NOTES: This class is based on WinMTRDialog and WinMTRMain from WinMTR-v092.
//        The report print code has been directly taken from mtr-0.84 and
//        adapted to C++ and the WinMTRNet class.
//    
//
//*****************************************************************************

#ifndef WINMTRCMD_H_
#define WINMTRCMD_H_

#include "WinMTRGlobal.h"
#include "WinMTRNet.h"
#include "WinMTRParams.h"

//*****************************************************************************
// CLASS:  WinMTRCmd
//
//
//*****************************************************************************

class WinMTRCmd {

public:
	WinMTRCmd(_TCHAR *name);

	void	Run();

private:
	typedef void* (WinMTRNet::*VoidGetMethod)(int at);
	typedef int (WinMTRNet::*IntGetMethod)(int at);
	typedef float (WinMTRNet::*FloatGetMethod)(int at);

	static const int SafeGet = 0;
	static const int UnsafeGet = 1;

	static struct fields {
		unsigned char key;
		char *descr;
		char *title;
		char *format;
		int length;
		bool floatReturn;
		VoidGetMethod get[2];
	};

	bool	ParseCommandLineParams(LPTSTR cmd, WinMTRParams *wmtrparams);
	bool	ValidateParams(WinMTRParams *wmtrdlg);
	LPTSTR	GetCommandLineParams();
	bool	GetParamValue(LPTSTR cmd, char * param, char sparam, char *value, bool flag);
	bool	GetHostNameParamValue(LPTSTR cmd, std::string& value);

	void	PrintReport(FILE* file, WinMTRNet* net, int getType, 
		const char* fields, bool reportWide);
	int		GetAddr(char* s);

private:
	_TCHAR *programName;
	_TCHAR localHostname[256];
	_TCHAR cmdLineParams[256];

	static fields	dataFields[];
	int				indexMapping[256];
};

#endif	// ifndef WINMTRCMD_H_