Note: This is a re-upload from the abandoned project from Sourceforge.

# WinMTRCmd
MTR for the Windows command line, hosted on GitHub because nobody likes Sourceforge.

WinMTRCmd Version 0.1 05/29/2013

### About

WinMTRCmd combines traceroute and ping functionality in a single network diagnostic tool. It is a command-line application based on WinMTR, which in turn is based on the original MTR. 

### Usage

WinMTRCmd is a command-line tool without graphical user interface. Using the tool from the command-line is very similar to using mtr. The zip-archive contains two folders Release_x32 and Release_x64 containing the 32-bit and 64-bit version of the application.

Examples:
```
'WinMTRCmd google.com'
'WinMTRCmd --version'
'WinMTRCmd -c 5 -t 2 --report google.com'
'WinMTRCmd -i 0.1 -n -w google.com'
'WinMTRCmd -c 100 -i 0.2 -t 1 -o "LS ABW G" -f "report.txt" -r google.com'

For a complete list of command-line arguments see 'WinMTRCmd --help' and 'WinMTRCmd --help-format'.
```
 
### Build
To manually build the project Visual Studio 2010 is required. For the 32-bit version Visual Studio Express 2010 is sufficient. For the 64-bit version the Windows SDK 7.1 has to be installed in addition to Visual Studio Express 2010.

### Contact
Author: Martin Riess (volrathmr+winmtrcmd@gmail.com)
Project Page: http://sourceforge.net/p/winmtrcmd

### License
Like WinMTR and MTR, WinMTRCmd is offered as Open Source Software under GPL v2 (see LICENSE.md).
