//*****************************************************************************
// FILE:            WinMTRNet.cpp
//
//*****************************************************************************
#include "WinMTRGlobal.h"
#include "WinMTRNet.h"
#include "WinMTRParams.h"
#include <sstream>

#define TRACE_MSG(msg)										\
	{														\
	std::ostringstream dbg_msg(std::ostringstream::out);	\
	dbg_msg << msg << std::endl;							\
	OutputDebugString(dbg_msg.str().c_str());				\
	}

#define IPFLAG_DONT_FRAGMENT	0x02

struct trace_thread {
	int			address;
	WinMTRNet	*winmtr;
	int			ttl;
};

struct dns_resolver_thread {
	int			index;
	WinMTRNet	*winmtr;
};

void FinishThread(void *p);
void TraceThread(void *p);
void DnsResolverThread(void *p);

WinMTRNet::WinMTRNet(WinMTRParams *p) {
	
	ghMutex = CreateMutex(NULL, FALSE, NULL);
	tracing = false;
	initialized = false;
	wmtrparams = p;
	WSADATA wsaData;

    if( WSAStartup(MAKEWORD(2, 2), &wsaData) ) {
		fprintf(stderr, "error: failed initializing windows sockets library\n");
		return;
    }

    hICMP_DLL =  LoadLibrary(_T("ICMP.DLL"));
    if (hICMP_DLL == 0) {
		fprintf(stderr, "error: unable to locate ICMP.DLL\n");
        return;
    }

    /* 
     * Get pointers to ICMP.DLL functions
     */
    lpfnIcmpCreateFile  = (LPFNICMPCREATEFILE)GetProcAddress(hICMP_DLL,"IcmpCreateFile");
    lpfnIcmpCloseHandle = (LPFNICMPCLOSEHANDLE)GetProcAddress(hICMP_DLL,"IcmpCloseHandle");
    lpfnIcmpSendEcho    = (LPFNICMPSENDECHO)GetProcAddress(hICMP_DLL,"IcmpSendEcho");
    if ((!lpfnIcmpCreateFile) || (!lpfnIcmpCloseHandle) || (!lpfnIcmpSendEcho)) {
		fprintf(stderr, "error: wrong ICMP.DLL system library\n");
        return;
    }

    /*
     * IcmpCreateFile() - Open the ping service
     */
    hICMP = (HANDLE) lpfnIcmpCreateFile();
    if (hICMP == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "error: could not create ICMP file handle\n");
        return;
    }

	ResetHops();

	initialized = true;
	return;
}

WinMTRNet::~WinMTRNet()
{
	if(initialized) {
		/*
		 * IcmpCloseHandle - Close the ICMP handle
		 */
		lpfnIcmpCloseHandle(hICMP);

		// Shut down...
		FreeLibrary(hICMP_DLL);

		WSACleanup();
	
		CloseHandle(ghMutex);
	}
}

void WinMTRNet::ResetHops()
{
	memset(host, 0, MAX_HOPS * sizeof(s_nethost));
}

void WinMTRNet::DoTrace(int address, bool async)
{
	tracing = true;

	ResetHops();

	last_remote_addr = address;

	// one thread per TTL value
	for(int i = 0; i < MAX_HOPS; i++) {
		trace_thread *current = new trace_thread;
		current->address = address;
		current->winmtr = this;
		current->ttl = i + 1;
		hThreads[i] = (HANDLE)_beginthread(TraceThread, 0 , current);
	}

	if (async)
		_beginthread(FinishThread, 0 , this);
	else
		FinishThread(this);
}

void WinMTRNet::StopTrace()
{
	tracing = false;
}

bool WinMTRNet::IsTracing()
{
	return tracing;
}

void FinishThread(void *p)
{
	WinMTRNet *wmtrnet = (WinMTRNet*)p;

	WaitForMultipleObjects(MAX_HOPS, wmtrnet->hThreads, TRUE, INFINITE);

	wmtrnet->tracing = false;
}

void TraceThread(void *p)
{
	trace_thread* current = (trace_thread*)p;
	WinMTRNet *wmtrnet = current->winmtr;
	TRACE_MSG("Thread with TTL=" << current->ttl << " started.");

	s_nethost *nethost;
    IPINFO			stIPInfo, *lpstIPInfo;
    DWORD			dwReplyCount;
	int				rtt;
	char			achReqData[8192];
	int				nDataLen									= wmtrnet->wmtrparams->pingsize;
	char			achRepData[sizeof(ICMPECHO) + 8192];
	int				cycle										= 0;
	float			oldavg, oldjavg;

	nethost = &wmtrnet->host[current->ttl - 1];

    /*
     * Init IPInfo structure
     */
    lpstIPInfo				= &stIPInfo;
    stIPInfo.Ttl			= current->ttl;
    stIPInfo.Tos			= 0;
    stIPInfo.Flags			= IPFLAG_DONT_FRAGMENT;
    stIPInfo.OptionsSize	= 0;
    stIPInfo.OptionsData	= NULL;

    for (int i=0; i<nDataLen; i++) achReqData[i] = 32; //whitespaces

	while(wmtrnet->tracing && cycle++ < wmtrnet->wmtrparams->cycles) {
	    
		// For some strange reason, ICMP API is not filling the TTL for icmp echo reply
		// Check if the current thread should be closed
		if( current->ttl > wmtrnet->GetMax() ) break;

		// NOTE: some servers does not respond back everytime, if TTL expires in transit; e.g. :
		// ping -n 20 -w 5000 -l 64 -i 7 www.chinapost.com.tw  -> less that half of the replies are coming back from 219.80.240.93
		// but if we are pinging ping -n 20 -w 5000 -l 64 219.80.240.93  we have 0% loss
		// A resolution would be:
		// - as soon as we get a hop, we start pinging directly that hop, with a greater TTL
		// - a drawback would be that, some servers are configured to reply for TTL transit expire, but not to ping requests, so,
		// for these servers we'll have 100% loss
		dwReplyCount = wmtrnet->lpfnIcmpSendEcho(wmtrnet->hICMP, current->address, achReqData, nDataLen, lpstIPInfo, achRepData, sizeof(achRepData),
			(int)(wmtrnet->wmtrparams->timeout * 1000));

		PICMPECHO icmp_echo_reply = (PICMPECHO)achRepData;
		rtt = icmp_echo_reply->RoundTripTime;

		nethost->xmit++;
		if (dwReplyCount != 0) {
			TRACE_MSG("TTL " << current->ttl << " reply TTL " << icmp_echo_reply->Options.Ttl << " Status " << icmp_echo_reply->Status << " Reply count " << dwReplyCount);

			WaitForSingleObject(wmtrnet->ghMutex, INFINITE);
			switch(icmp_echo_reply->Status) {
				case IP_SUCCESS:
				case IP_TTL_EXPIRED_TRANSIT:

					// the following code is taken directly from the mtr-0.84 net implementation

					nethost->jitter = rtt - nethost->last;
					if (nethost->jitter < 0) nethost->jitter = -nethost->jitter;
					nethost->last = rtt;

					if (nethost->returned < 1)
					{
						nethost->best = nethost->worst = rtt;
						nethost->gmean = rtt;
						nethost->avg = 0;
						nethost->var = 0;
						nethost->jitter = nethost->jworst = nethost->jinta = 0;
					}

					if (rtt < nethost->best ) nethost->best  = rtt;
					if (rtt > nethost->worst) nethost->worst = rtt;

					if (nethost->jitter > nethost->jworst)
						nethost->jworst = nethost->jitter;

					nethost->returned++;

					oldavg = nethost->avg;
					nethost->avg += (float)(rtt - oldavg) / nethost->returned;
					nethost->var += (rtt - oldavg) * (rtt - nethost->avg);

					oldjavg = nethost->javg;
					nethost->javg += (nethost->jitter - oldjavg) / nethost->returned;

					nethost->jinta += nethost->jitter - ((nethost->jinta + 8) >> 4);

					if (nethost->returned > 1)
						nethost->gmean = pow((float) nethost->gmean, (nethost->returned - 1.0f) / nethost->returned)
						* pow((float) rtt, 1.0f / nethost->returned);
					else
						nethost->gmean = rtt;

					wmtrnet->SetAddr(current->ttl - 1, icmp_echo_reply->Address);
				break;
				case IP_BUF_TOO_SMALL:
					wmtrnet->SetName(current->ttl - 1, "Reply buffer too small.");
				break;
				case IP_DEST_NET_UNREACHABLE:
					wmtrnet->SetName(current->ttl - 1, "Destination network unreachable.");
				break;
				case IP_DEST_HOST_UNREACHABLE:
					wmtrnet->SetName(current->ttl - 1, "Destination host unreachable.");
				break;
				case IP_DEST_PROT_UNREACHABLE:
					wmtrnet->SetName(current->ttl - 1, "Destination protocol unreachable.");
				break;
				case IP_DEST_PORT_UNREACHABLE:
					wmtrnet->SetName(current->ttl - 1, "Destination port unreachable.");
				break;
				case IP_NO_RESOURCES:
					wmtrnet->SetName(current->ttl - 1, "Insufficient IP resources were available.");
				break;
				case IP_BAD_OPTION:
					wmtrnet->SetName(current->ttl - 1, "Bad IP option was specified.");
				break;
				case IP_HW_ERROR:
					wmtrnet->SetName(current->ttl - 1, "Hardware error occurred.");
				break;
				case IP_PACKET_TOO_BIG:
					wmtrnet->SetName(current->ttl - 1, "Packet was too big.");
				break;
				case IP_REQ_TIMED_OUT:
					wmtrnet->SetName(current->ttl - 1, "Request timed out.");
				break;
				case IP_BAD_REQ:
					wmtrnet->SetName(current->ttl - 1, "Bad request.");
				break;
				case IP_BAD_ROUTE:
					wmtrnet->SetName(current->ttl - 1, "Bad route.");
				break;
				case IP_TTL_EXPIRED_REASSEM:
					wmtrnet->SetName(current->ttl - 1, "The time to live expired during fragment reassembly.");
				break;
				case IP_PARAM_PROBLEM:
					wmtrnet->SetName(current->ttl - 1, "Parameter problem.");
				break;
				case IP_SOURCE_QUENCH:
					wmtrnet->SetName(current->ttl - 1, "Datagrams are arriving too fast to be processed and datagrams may have been discarded.");
				break;
				case IP_OPTION_TOO_BIG:
					wmtrnet->SetName(current->ttl - 1, "An IP option was too big.");
				break;
				case IP_BAD_DESTINATION:
					wmtrnet->SetName(current->ttl - 1, "Bad destination.");
				break;
				case IP_GENERAL_FAILURE:
					wmtrnet->SetName(current->ttl - 1, "General failure.");
				break;
				default:
					wmtrnet->SetName(current->ttl - 1, "General failure.");
			}
			ReleaseMutex(wmtrnet->ghMutex);

			if(wmtrnet->wmtrparams->interval * 1000 > icmp_echo_reply->RoundTripTime)
				Sleep(wmtrnet->wmtrparams->interval * 1000 - icmp_echo_reply->RoundTripTime);
		}

    } /* end ping loop */

	TRACE_MSG("Thread with TTL=" << current->ttl << " stopped.");

	delete p;
	_endthread();
}

int WinMTRNet::GetAddr(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int addr = ntohl(host[at].addr);
	ReleaseMutex(ghMutex);
	return addr;
}

int WinMTRNet::GetName(int at, char *n)
{
	WaitForSingleObject(ghMutex, INFINITE);
	GetNameUnsafe(at, n);
	ReleaseMutex(ghMutex);
	return 0;
}

int WinMTRNet::GetBest(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].best;
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetWorst(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].worst;
	ReleaseMutex(ghMutex);
	return ret;
}

float WinMTRNet::GetAvg(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	float ret = host[at].avg;
	ReleaseMutex(ghMutex);
	return ret;
}

float WinMTRNet::GetPercent(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	float ret = (host[at].xmit == 0) ? 0 : (100.0f - (100.0f * host[at].returned / host[at].xmit));
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetLast(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].last;
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetReturned(int at)
{ 
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].returned;
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetXmit(int at)
{ 
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].xmit;
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetDropped(int at)
{ 
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].xmit - host[at].returned;
	ReleaseMutex(ghMutex);
	return ret;
}

float WinMTRNet::GetStDev(int at)
{ 
	WaitForSingleObject(ghMutex, INFINITE);
    float ret = GetStDevUnsafe(at);
	ReleaseMutex(ghMutex);
	return ret;
}

float WinMTRNet::GetGMean(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	float ret = host[at].gmean;
	ReleaseMutex(ghMutex);
	return ret;
}

int	WinMTRNet::GetJitter(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].jitter;
	ReleaseMutex(ghMutex);
	return ret;
}

float WinMTRNet::GetJAvg(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	float ret = host[at].javg;
	ReleaseMutex(ghMutex);
	return ret;
}

int	WinMTRNet::GetJWorst(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].jworst;
	ReleaseMutex(ghMutex);
	return ret;
}

int	WinMTRNet::GetJInta(int at)
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = host[at].jinta;
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetMax()
{
	WaitForSingleObject(ghMutex, INFINITE);
	int ret = GetMaxUnsafe();
	ReleaseMutex(ghMutex);
	return ret;
}

int WinMTRNet::GetAddrUnsafe(int at)
{
	return ntohl(host[at].addr);
}

int WinMTRNet::GetNameUnsafe(int at, char *n)
{
	if(!strcmp(host[at].name, "")) {
		int addr = GetAddr(at);
		sprintf (n, "%d.%d.%d.%d", 
			(addr >> 24) & 0xff, 
			(addr >> 16) & 0xff, 
			(addr >> 8) & 0xff, 
			addr & 0xff
		);
		if(addr==0)
			strcpy(n,"???");
	} else {
		strcpy(n, host[at].name);
	}
	return 0;
}

int WinMTRNet::GetBestUnsafe(int at)
{
	return host[at].best;
}

int WinMTRNet::GetWorstUnsafe(int at)
{
	return host[at].worst;
}

float WinMTRNet::GetAvgUnsafe(int at)
{
	return host[at].avg;
}

float WinMTRNet::GetPercentUnsafe(int at)
{
	return (host[at].xmit == 0) ? 0.0f :
		(100.0f - (100.0f * host[at].returned / host[at].xmit));
}

int WinMTRNet::GetLastUnsafe(int at)
{
	return host[at].last;
}

int WinMTRNet::GetReturnedUnsafe(int at)
{ 
	return host[at].returned;
}

int WinMTRNet::GetXmitUnsafe(int at)
{
	return host[at].xmit;
}

int WinMTRNet::GetDroppedUnsafe(int at)
{
	return host[at].xmit - host[at].returned;
}

float WinMTRNet::GetStDevUnsafe(int at)
{
	if (host[at].returned > 1) {
		return sqrt(host[at].var / (host[at].returned - 1.0f));
	} else
		return 0;
}

float WinMTRNet::GetGMeanUnsafe(int at)
{
	return host[at].gmean;
}

int	WinMTRNet::GetJitterUnsafe(int at)
{
	return host[at].jitter;
}

float WinMTRNet::GetJAvgUnsafe(int at)
{
	return host[at].javg;
}

int	WinMTRNet::GetJWorstUnsafe(int at)
{
	return host[at].jworst;
}

int	WinMTRNet::GetJIntaUnsafe(int at)
{
	return host[at].jinta;
}

int WinMTRNet::GetMaxUnsafe()
{
	int max = MAX_HOPS;

	// first match: traced address responds on ping requests, and the address is in the hosts list
	for(int i = 0; i < MAX_HOPS; i++) {
		if(host[i].addr == last_remote_addr) {
			max = i + 1;
			break;
		}
	}

	// second match:  traced address doesn't responds on ping requests
	if(max == MAX_HOPS) {
		while((max > 1) && (host[max - 1].addr == host[max - 2].addr) && (host[max - 1].addr != 0) ) max--;
	}
	
	return max;
}

void WinMTRNet::SetAddr(int at, __int32 addr)
{
	if(host[at].addr == 0 && addr != 0) {
		TRACE_MSG("Start DnsResolverThread for new address " << addr << ". Old addr value was " << host[at].addr);
		host[at].addr = addr;
		if(wmtrparams->useDNS)
		{
			dns_resolver_thread *dnt = new dns_resolver_thread;
			dnt->index = at;
			dnt->winmtr = this;
			_beginthread(DnsResolverThread, 0, dnt);
		}
	}
}

void WinMTRNet::SetName(int at, char *n)
{
	strcpy(host[at].name, n);
}

void DnsResolverThread(void *p)
{
	TRACE_MSG("DNS resolver thread started.");
	dns_resolver_thread *dnt = (dns_resolver_thread*)p;
	WinMTRNet* wn = dnt->winmtr;

	struct hostent *phent ;

	char buf[100];
	int addr = wn->GetAddr(dnt->index);
	sprintf (buf, "%d.%d.%d.%d", (addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);

	int haddr = htonl(addr);
	phent = gethostbyaddr( (const char*)&haddr, sizeof(int), AF_INET);

	WaitForSingleObject(wn->ghMutex, INFINITE);
	if(phent) {
		wn->SetName(dnt->index, phent->h_name);
	} else {
		wn->SetName(dnt->index, buf);
	}
	ReleaseMutex(wn->ghMutex);
	
	delete p;
	TRACE_MSG("DNS resolver thread stopped.");
	_endthread();
}
