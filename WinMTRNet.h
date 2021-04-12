//*****************************************************************************
// FILE:            WinMTRNet.h
//
//
// DESCRIPTION: The WinMTRNet class implements the core ping and traceroute
//              functionality.
//   
//
// NOTES: This class is heavily based on the WinMTRNet class from WinMTR-v092.
//        Additional code from mtr-0.84 has been merged into this to provide
//        a broader spectrum of statistics.
//    
//
//*****************************************************************************

#ifndef WINMTRNET_H_
#define WINMTRNET_H_


class WinMTRParams;

typedef ip_option_information IPINFO, *PIPINFO, FAR *LPIPINFO;

#ifdef _WIN64
typedef icmp_echo_reply32 ICMPECHO, *PICMPECHO, FAR *LPICMPECHO;
#else
typedef icmp_echo_reply ICMPECHO, *PICMPECHO, FAR *LPICMPECHO;
#endif

struct s_nethost {
  __int32 addr;		// IP as a decimal, big endian
  int xmit;			// number of PING packets sent
  int returned;		// number of ICMP echo replies received
  int last;				// last time
  int best;				// best time
  int worst;			// worst time
  float avg;			// average
  float var;		    // variance
  float gmean;			// geometric mean
  int jitter;			// current jitter, defined as t1-t0
  float javg;			// avg jitter
  int jworst;			// max jitter
  int jinta;			// estimated variance,? rfc1889's "Interarrival Jitter"
  char name[255];
};

//*****************************************************************************
// CLASS:  WinMTRNet
//
//
//*****************************************************************************

class WinMTRNet {
	typedef HANDLE (WINAPI *LPFNICMPCREATEFILE)(VOID);
	typedef BOOL  (WINAPI *LPFNICMPCLOSEHANDLE)(HANDLE);
	typedef DWORD (WINAPI *LPFNICMPSENDECHO)(HANDLE, u_long, LPVOID, WORD, LPVOID, LPVOID, DWORD, DWORD);

	friend void FinishThread(void *p);
	friend void TraceThread(void *p);
	friend void DnsResolverThread(void *p);

public:

	WinMTRNet(WinMTRParams *p);
	~WinMTRNet();
	void	DoTrace(int address, bool async);
	void	ResetHops();
	void	StopTrace();
	bool	IsTracing();

	int		GetAddr(int at);
	int		GetName(int at, char *n);
	int		GetBest(int at);
	int		GetWorst(int at);
	float	GetAvg(int at);
	float	GetPercent(int at);
	int		GetLast(int at);
	int		GetReturned(int at);
	int		GetXmit(int at);
	int		GetDropped(int at);
	float	GetStDev(int at);
	float	GetGMean(int at);
	int		GetJitter(int at);
	float	GetJAvg(int at);
	int		GetJWorst(int at);
	int		GetJInta(int at);
	int		GetMax();

	// these getter versions are not thread safe, but significantly faster
	int		GetAddrUnsafe(int at);
	int		GetNameUnsafe(int at, char *n);
	int		GetBestUnsafe(int at);
	int		GetWorstUnsafe(int at);
	float	GetAvgUnsafe(int at);
	float	GetPercentUnsafe(int at);
	int		GetLastUnsafe(int at);
	int		GetReturnedUnsafe(int at);
	int		GetXmitUnsafe(int at);
	int		GetDroppedUnsafe(int at);
	float	GetStDevUnsafe(int at);
	float	GetGMeanUnsafe(int at);
	int		GetJitterUnsafe(int at);
	float	GetJAvgUnsafe(int at);
	int		GetJWorstUnsafe(int at);
	int		GetJIntaUnsafe(int at);
	int		GetMaxUnsafe();

private:
	void	SetAddr(int at, __int32 addr);
	void	SetName(int at, char *n);

private:
	HANDLE				hThreads[MAX_HOPS];

	WinMTRParams		*wmtrparams;
	__int32				last_remote_addr;
	bool				tracing;
	bool				initialized;
    HANDLE				hICMP;
	LPFNICMPCREATEFILE	lpfnIcmpCreateFile;
	LPFNICMPCLOSEHANDLE lpfnIcmpCloseHandle;
	LPFNICMPSENDECHO	lpfnIcmpSendEcho;

	HINSTANCE			hICMP_DLL;

	struct s_nethost	host[MAX_HOPS];
	HANDLE				ghMutex;
};

#endif	// ifndef WINMTRNET_H_
