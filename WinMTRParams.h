//*****************************************************************************
// FILE:            WinMTRParams.h
//
//
// DESCRIPTION: The WinMTRParams class collects the parameters for running 
//              an instance of WinMTRNet.
//   
//
// NOTES: 
//    
//
//*****************************************************************************

#ifndef WINMTROPTIONS_H_
#define WINMTROPTIONS_H_

#include "WinMTRGlobal.h"

//*****************************************************************************
// CLASS:  WinMTRParams
//
//
//*****************************************************************************

#define SIZE_HOSTNAME 1024
#define SIZE_FIELDS 64
#define SIZE_FILENAME 1024

class WinMTRParams {

public:

	char				hostname[SIZE_HOSTNAME];
	int					cycles;
	float				interval;
	int					pingsize;
	float				timeout;
	bool				report;
	bool				useDNS;
	bool				wide;
	char				fields[SIZE_FIELDS];
	bool				reportToFile;
	char				filename[SIZE_FILENAME];

	WinMTRParams();

	void SetHostName(const char *host);
	void SetCycles(int c);
	void SetInterval(float i);
	void SetPingSize(int ps);
	void SetTimeout(float t);
	void SetReport(bool b);
	void SetUseDNS(bool udns);
	void SetWide(bool w);
	void SetFields(const char *f);
	void SetFilename(const char *f);
};

#endif	// ifndef WINMTRPARAMS_H_