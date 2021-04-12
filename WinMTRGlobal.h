//*****************************************************************************
// FILE:            WinMTRGlobal.h
//
//
// DESCRIPTION: Contains global definitions for the WinMTRCmd application.
//   
//
// NOTES: Based on WinMTRGlobal.h from WinMTR-v092.
//    
//
//*****************************************************************************

#ifndef GLOBAL_H_
#define GLOBAL_H_

#ifndef  _WIN64
#define  _USE_32BIT_TIME_T
#endif

#define VC_EXTRALEAN

#include <Windows.h>
#include <IPExport.h>

#include <process.h>
#include <stdio.h>
#include <io.h>
#include <conio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <tchar.h>
#include <errno.h>
#include <memory.h>
#include <math.h>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

#define WINMTR_VERSION_STR "WinMTR 0.9 (c) 2010-2011 Appnor MSP - http://WinMTR.sourceforge.net"
#define MTR_VERSION_STR "mtr 0.84 - http://www.bitwizard.nl/mtr/"

#define WINMTRCMD_VERSION	"0.1"
#define WINMTRCMD_HOMEPAGE	"http://WinMTRCmd.sourceforge.net"
#define WINMTRCMD_LICENSE	"GPL - GNU Public License"

#define DEFAULT_REPORT		FALSE
#define DEFAULT_WIDE		FALSE
#define DEFAULT_DNS			TRUE
#define DEFAULT_CYCLES		10
#define DEFAULT_INTERVAL	1.0
#define DEFAULT_PING_SIZE	64
#define DEFAULT_TIMEOUT		5.0
#define DEFAULT_FIELDS		"LS NABWV"

#define MAX_HOPS				40

#define MAXPACKET 4096
#define MINPACKET 64

#define CLS() system("cls")

#endif // ifndef GLOBAL_H_
