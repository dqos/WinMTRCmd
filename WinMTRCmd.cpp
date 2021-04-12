//*****************************************************************************
// FILE:			WinMTRCmd.cpp
//
//
//*****************************************************************************

#include "WinMTRCmd.h"
#include "WinMTRGlobal.h"
#include "WinMTRNet.h"
#include "WinMTRParams.h"

//*****************************************************************************
// _tmain
//
// 
//*****************************************************************************
int _tmain(int, _TCHAR** argv)
{
	WinMTRCmd cmd(argv[0]);
	cmd.Run();
	return 0;
}

//*****************************************************************************
// WinMTRCmd::WinMTRCmd
//
// 
//*****************************************************************************
WinMTRCmd::WinMTRCmd(_TCHAR *name)
	: programName(name)
{
	for (int i = 0; i < 256; i++)
		indexMapping[i] = -1;
	for (int i = 0; dataFields[i].key != 0; i++)
		indexMapping[dataFields[i].key] = i;
}

//*****************************************************************************
// WinMTRCmd::ParseCommandLineParams
//
// 
//*****************************************************************************
void ExitThread(void *p)
{
	char c;
	do {
		c = _getch();
	} while(c != 'x' && c != 'X');

	exit(0);
}

void WinMTRCmd::Run()
{
	WinMTRParams params;
	LPTSTR cmdLine			= GetCommandLineParams();
	WinMTRNet* net;
	int addr;
	FILE *file;

	// initialize default parameters
	params.SetHostName("");
	params.SetReport(DEFAULT_REPORT);
	params.SetWide(DEFAULT_WIDE);
	params.SetUseDNS(DEFAULT_DNS);
	params.SetCycles(DEFAULT_CYCLES);
	params.SetInterval(DEFAULT_INTERVAL);
	params.SetPingSize(DEFAULT_PING_SIZE);
	params.SetTimeout(DEFAULT_TIMEOUT);
	params.SetFields(DEFAULT_FIELDS);

	// parse and validate command-line params
	if (!ParseCommandLineParams(cmdLine, &params)) return;
	if (!ValidateParams(&params)) return;

	net = new WinMTRNet(&params);

	// resolve the hostname
	addr = GetAddr(params.hostname);
	if (addr == INADDR_NONE)
	{
		delete net;
		fprintf(stderr, "error: could not resolve hostname\n");
		return;
	}

	// get the name of the local host for later use
	gethostname(localHostname, 256);

	if (params.report) {
		// perform the trace sync
		net->DoTrace(addr, false);

		// print the trace report
		if (params.reportToFile)
		{
			file = fopen(params.filename, "w");
			if (file == NULL) {
				fprintf(stderr, "error: could not redirect report to '%s': %s\n",
					params.filename, strerror(errno));
				PrintReport(stdout, net, UnsafeGet, params.fields, params.wide);
			} else {
				PrintReport(file, net, UnsafeGet, params.fields, params.wide);
				fclose(file);
			}
		}
		else
			PrintReport(stdout, net, UnsafeGet, params.fields, params.wide);
	} else {
		// start the thread listening for the exit command
		_beginthread(ExitThread, 0, 0);

		// perform the trace async
		net->DoTrace(addr, true);

		// update the display in a loop while tracing
		while(net->IsTracing()) {
			Sleep((int)(params.interval * 1000));
			CLS();
			printf("(X) Exit\n");
			PrintReport(stdout, net, SafeGet, params.fields, params.wide);
		}
	}

	delete net;
}

//*****************************************************************************
// WinMTRCmd::ParseCommandLineParams
//
// 
//*****************************************************************************
bool WinMTRCmd::ParseCommandLineParams(LPTSTR cmd, WinMTRParams *wmtrparams)
{
	char value[128];
	std::string host_name = "";

	if(GetParamValue(cmd, "version",'v', value, true)) {
		printf("WinMTRCmd %s - %s\n"
			   "based on %s\n"
			   "and %s\n"
			   "%s\n", WINMTRCMD_VERSION, WINMTRCMD_HOMEPAGE, MTR_VERSION_STR,
			   WINMTR_VERSION_STR, WINMTRCMD_LICENSE);
		return false;
	}

	if(GetParamValue(cmd, "help",'h', value, true)) {
		printf("usage: %s [--help|-h] [--help-format|-p] [--version|-v]\n"
			   "\t\t [--report|-r] [--wide|-w] [--numeric|-n]\n"
			   "\t\t [--cycles=COUNT|-c=COUNT] [--interval=SECONDS|-i=SECONDS]\n"
			   "\t\t [--size=BYTES|-s=BYTES] [--timeout=SECONDS|-t=SECONDS]\n"
			   "\t\t [--file=PATH|-f=PATH] [--order=FIELDS ORDER|-o=FIELDS ORDER]\n"
			   "\t\t HOSTNAME\n", programName);
		return false;
	}

	if(GetParamValue(cmd, "help-format",'p', value, true)) {
		printf("order format usage: [key+], for example '%s'\n"
			"\tkey:  field description\n", DEFAULT_FIELDS);
		for (int i = 0; dataFields[i].key != 0; ++i) {
			printf("\t%s\n", dataFields[i].descr);
		}
		return false;
	}

	if (GetParamValue(cmd, "report", 'r', value, true)) {
		wmtrparams->SetReport(true);
	}
	if(GetParamValue(cmd, "wide",'w', value, true)) {
		wmtrparams->SetWide(true);
	}
	if(GetParamValue(cmd, "numeric",'n', value, true)) {
		wmtrparams->SetUseDNS(false);
	}
	if(GetParamValue(cmd, "cycles",'c', value, false)) {
		wmtrparams->SetCycles(atoi(value));
	}
	if(GetParamValue(cmd, "interval",'i', value, false)) {
		wmtrparams->SetInterval((float)atof(value));
	}
	if(GetParamValue(cmd, "size",'s', value, false)) {
		wmtrparams->SetPingSize(atoi(value));
	}
	if(GetParamValue(cmd, "timeout",'t', value, false)) {
		wmtrparams->SetTimeout((float)atof(value));
	}
	if(GetParamValue(cmd, "file",'f', value, false)) {
		wmtrparams->SetFilename(value);
	}
	if(GetParamValue(cmd, "order",'o', value, false)) {
		wmtrparams->SetFields(value);
	}
	if(GetHostNameParamValue(cmd, host_name)) {
		wmtrparams->SetHostName(host_name.c_str());
	}
	return true;
}

//*****************************************************************************
// WinMTRCmd::ValidateParams
//
// 
//*****************************************************************************
bool WinMTRCmd::ValidateParams(WinMTRParams *wmtrparams)
{
	if (strlen(wmtrparams->hostname) == 0) {
		printf("error: no hostname specified\n");
		return false;
	}

	if (wmtrparams->pingsize < MINPACKET || wmtrparams->pingsize > MAXPACKET) {
		printf("error: size has to be in the range [%d, %d]\n", MINPACKET, MAXPACKET);
		return false;
	}

	return true;
}

//*****************************************************************************
// WinMTRCmd::GetCommandLineParams
//
// 
//*****************************************************************************
LPTSTR WinMTRCmd::GetCommandLineParams()
{
	LPTSTR cmdLine = GetCommandLine();
	LPTSTR offset;

	if (*cmdLine == 0) {
		return cmdLine;
	}
	if (*cmdLine == '"') {
		offset = strchr(cmdLine + 1, '"') + 1;
	} else {
		offset = strchr(cmdLine, ' ');
		if (!offset) {
			offset = cmdLine + strlen(cmdLine);
		}
	}
	while (*offset == ' ')
		++offset;
	if (*offset) {
		strcpy(cmdLineParams, offset);
		strcat(cmdLineParams, " ");
	} else {
		*cmdLineParams = ' ';
		*(cmdLineParams + 1) = 0;
	}
	return cmdLineParams;
}

//*****************************************************************************
// WinMTRCmd::GetParamValue
//
// 
//*****************************************************************************
bool WinMTRCmd::GetParamValue(LPTSTR cmd, char * param, char sparam, char *value, bool flag)
{
	char *p;
	
	char p_long[128];
	char p_short[128];
	bool inString = false;
	
	sprintf(p_long,"--%s ", param);
	sprintf(p_short,"-%c ", sparam);
	
	if( (p=strstr(cmd, p_long)) ) ;
	else 
		p=strstr(cmd, p_short);

	if(p == NULL)
		return false;

	if(flag) 
		return true;

	while(*p && *p!=' ')
		p++;
	while(*p==' ') p++;
	
	int i = 0;
	while(*p && (*p!=' ' || inString))
	{
		if (*p == '"')
		{
			if (inString) break;
			else inString = true;
		} else
			value[i++] = *p;

		p++;
	}
	value[i]='\0';

	return true;
}

//*****************************************************************************
// WinMTRCmd::GetHostNameParamValue
//
// 
//*****************************************************************************
bool WinMTRCmd::GetHostNameParamValue(LPTSTR cmd, std::string& host_name)
{
	int size = strlen(cmd);
	std::string name = "";

	while(cmd[--size] == ' ');
	size++;
	
	while(size-- && cmd[size] != ' ' && (cmd[size] != '-' || cmd[size - 1] != ' ')) {
		name = cmd[size ] + name;
	}

	if(size == -1) {
		if(name.length() == 0) {
			return false;
		} else {
			host_name = name;
			return true;
		}
	}
	if(cmd[size] == '-' && cmd[size - 1] == ' ') {
		// no target specified
		return false;
	}

	while(cmd[--size] == ' ');
	size++;

	std::string possible_argument = "";

	while(size-- && cmd[size] != ' ') {
		possible_argument = cmd[size] + possible_argument;
	}

	if(possible_argument.length() && (possible_argument[0] != '-' || possible_argument == "-n"
		|| possible_argument == "-w" || possible_argument == "-r" || possible_argument == "--numeric"
		|| possible_argument == "--wide" ||  possible_argument == "--report")) {
		host_name = name;
		return true;
	}

	return false;
}

//*****************************************************************************
// WinMTRCmd::PrintReport
//
// 
//*****************************************************************************

void WinMTRCmd::PrintReport(FILE* file, WinMTRNet* net, int getType,
	const char* fields, bool reportwide)
{
	int max;
	char name[81];
	char buf[1024];
	char fmt[16];
	int len = 0, len_hosts = 33;
	VoidGetMethod voidNetGet;

	max = net->GetMax();

	if (reportwide) {
		// get the longest hostname
		len_hosts = strlen(localHostname);
		for (int at = 0; at < max; at++) {
			int nlen;
			net->GetName(at, name);
			if ((nlen = strlen(name)))
				if (len_hosts < nlen)
					len_hosts = nlen;
		}
	}

	_snprintf(fmt, sizeof(fmt), "HOST: %%-%ds", len_hosts + 2);
	_snprintf(buf, sizeof(buf), fmt, localHostname);
	len = reportwide ? strlen(buf) : len_hosts;
	for (int i = 0; fields[i] != 0; i++) {
		int j = indexMapping[fields[i]];
		if (j < 0) continue;

		_snprintf(fmt, sizeof(fmt), "%%%ds", dataFields[j].length);
		_snprintf(buf + len, sizeof(buf) - len, fmt, dataFields[j].title);
		len += dataFields[j].length;
	}
	fprintf(file, "%s\n", buf);

	for (int at = 0; at < max; at++) {
		net->GetName(at, name);

		_snprintf(fmt, sizeof(fmt), " %%2d.|-- %%-%ds", len_hosts);
		_snprintf(buf, sizeof(buf), fmt, at + 1, name);
		len = reportwide ? strlen(buf) : len_hosts;
		for (int i = 0; i < fields[i] != 0; i++) {
			int j = indexMapping[fields[i]];
			if (j < 0) continue;
			
			voidNetGet = dataFields[j].get[getType];
			if (voidNetGet != NULL)
			{
				if (dataFields[j].floatReturn)
					_snprintf(buf + len, sizeof(buf) - len, dataFields[j].format,
						(net->*((FloatGetMethod)voidNetGet))(at));
				else
					_snprintf(buf + len, sizeof(buf) - len, dataFields[j].format,
						(net->*((IntGetMethod)voidNetGet))(at));
			}
			else
				_snprintf(buf + len, sizeof(buf) - len, dataFields[j].format);
			len += dataFields[j].length;
		}
		fprintf(file, "%s\n", buf);
	}
}

//*****************************************************************************
// WinMTRCmd::GetAddr
//
// 
//*****************************************************************************
int WinMTRCmd::GetAddr(char* s)
{
	struct hostent *host;

	int isIP=1;
	char *t = s;
	while(*t) {
		if(!isdigit(*t) && *t!='.') {
			isIP=0;
			break;
		}
		t++;
	}

	if(!isIP) {
		host = gethostbyname(s);
		if (host)
			return *(int *)host->h_addr;
		else
			return INADDR_NONE;
	} else
		return inet_addr(s);
}

//*****************************************************************************
// WinMTRCmd::dataFields
//
// 
//*****************************************************************************
#define NETM(a) (VoidGetMethod)&WinMTRNet::a

WinMTRCmd::fields WinMTRCmd::dataFields[] = {
		// key, Remark, Header, Format, Width, Float Return, Getter
		{' ', "<sp>: Space between fields", " ",  " ",           1, false, NULL, NULL },
		{'L', "L:    Loss Ratio",          "Loss%",  " %5.1f%%", 7, true,
			NETM(GetPercent), NETM(GetPercentUnsafe) },
		{'D', "D:    Dropped Packets",     "Drop",   " %4d",     5, false,
			NETM(GetDropped), NETM(GetDroppedUnsafe) },
		{'R', "R:    Received Packets",    "Rcv",    " %4d",     5, false,
			NETM(GetReturned), NETM(GetReturnedUnsafe) },
		{'S', "S:    Sent Packets",        "Snt",    " %4d",     5, false,
			NETM(GetXmit), NETM(GetXmitUnsafe) },
		{'N', "N:    Newest RTT(ms)",      "Last",   " %4d",	 5, false,
			NETM(GetLast), NETM(GetLastUnsafe) },
		{'B', "B:    Min/Best RTT(ms)",    "Best",   " %4d",     5, false,
			NETM(GetBest), NETM(GetBestUnsafe) },
		{'A', "A:    Average RTT(ms)",     "Avg",    " %6.1f",   7, true,
			NETM(GetAvg), NETM(GetAvgUnsafe) },
		{'W', "W:    Max/Worst RTT(ms)",   "Wrst",   " %4d",     5, false,
			NETM(GetWorst), NETM(GetWorstUnsafe) },
		{'V', "V:    Standard Deviation",  "StDev",  " %6.1f",   7, true,
			NETM(GetStDev), NETM(GetStDevUnsafe) },
		{'G', "G:    Geometric Mean",      "Gmean",  " %6.1f",   7, true,
			NETM(GetGMean), NETM(GetGMeanUnsafe) },
		{'J', "J:    Current Jitter",      "Jttr",   " %4d",     5, false,
			NETM(GetJitter), NETM(GetJitterUnsafe) },
		{'M', "M:    Jitter Mean/Avg.",    "Javg",   " %6.1f",   7, true,
			NETM(GetJAvg), NETM(GetJAvgUnsafe) },
		{'X', "X:    Worst Jitter",        "Jmax",   " %4d",     5, false,
			NETM(GetJWorst), NETM(GetJWorstUnsafe) },
		{'I', "I:    Interarrival Jitter", "Jint",   " %4d",     5, false,
			NETM(GetJInta), NETM(GetJIntaUnsafe) },
		{'\0', NULL, NULL, NULL, 0, false, NULL, NULL}
	};
