/******************************************************************************
* Copyright 1991 Advanced Micro Devices, Inc.
* 
* This software is the property of Advanced Micro Devices, Inc  (AMD)  which
* specifically  grants the user the right to modify, use and distribute this
* software provided this notice is not removed or altered.  All other rights
* are reserved by AMD.
*
* AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
* SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
* DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
* USE OF THIS SOFTWARE.
*
* So that all may benefit from your experience, please report  any  problems
* or  suggestions about this software to the 29K Technical Support Center at
* 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
* 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
*
* Advanced Micro Devices, Inc.
* 29K Support Products
* Mail Stop 573
* 5900 E. Ben White Blvd.
* Austin, TX 78741
* 800-292-9263 (US)
*****************************************************************************
*/
static	char soc2udip_c[]="@(#)soc2udip.c	2.14  Daniel Mann";
static char soc2udip_c_AMD[]="@(#)soc2udip.c	3.2, AMD";
/* 
*       This module converts UDI socket messages
*	into UDI Procedural calls.
*	It is used by TIP server processes
********************************************************************** HISTORY
*/
#include <stdio.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#ifdef __hpux
#else
/* #include <sys/sockio.h> */
#endif

#include <sys/fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <setjmp.h>
#include "udiproc.h"
#include "udisoc.h"

extern	int	errno;

/* local type decs. and macro defs. not in a .h  file ************* MACRO/TYPE
*/
#define	version_c 0x121		/* TIP-IPC version id */
#define PORT_NUM 	7000
#define	MAX_ERRNO	1
#define	SOC_BUF_SIZE 	4* 1024	/* size of socket comms buffer */
#define	SBUF_SIZE 	500	/* size of string buffer */

/* global dec/defs. which are not in a .h   file ************* EXPORT DEC/DEFS
*/

/* local dec/defs. which are not in a .h   file *************** LOCAL DEC/DEFS
*/
LOCAL UDIError	tip_errno;
LOCAL int	tip_sd;
LOCAL int	connect_count = 0;
LOCAL int	ns;
LOCAL int	getlength;
LOCAL struct sockaddr getname;
LOCAL UDR	udr;
LOCAL UDR*	udrs = &udr;
LOCAL char	sbuf[SBUF_SIZE];	/* String handler buffer */
LOCAL char*	tip_errmsg[/*MAX_ERRNO*/] =
{ "Trying to present non negative error number to TIP "
};

LOCAL sig_handler(sig, code, scp, addr)
int	sig;
int	code;
struct	sigcontext *scp;
char	*addr;
{
    if(sig == SIGUSR1)
    {
	UDIStop();
	return;
    }
    if(sig == SIGIO )
    {
	return;
    }
    if(sig == SIGURG)
    {
	UDIStop();
	return;
    }
    if(sig != -1)fprintf(stderr,
			"\nTIP-ipc caught a signal %d, socket shutdown\n",sig);
#if 0
    else fprintf(stderr,"\nTIP-ipc, socket shutdown\n");
#endif
    sigblock(sigmask(SIGIO));
    if(shutdown(tip_sd, 2))
    	fprintf(stderr, "TIP-ipc WARNING: socket shutdown failed");
    if(udrs->domain == AF_UNIX)
	if(unlink(udrs->soc_name))
    	   fprintf(stderr,
		"TIP-ipc WARNING: failed to unlink %s\n",udrs->soc_name);
    exit(0);
}
/*********************************************************************** MAIN
*/
main(argc, argv)
int 	argc;
char*	argv[];
{
    char	*domain_string = argv[1];
    char	*tip_string = argv[2];
    struct sockaddr_in	dfe_sockaddr;
    struct sockaddr_in	tip_sockaddr_in;
    struct sockaddr	tip_sockaddr;
    struct hostent 	*tip_info_p;
    int		dfe_addrlen;
    int		domain; 
    int		pgrp;
#define	NAME_LEN 20
    char	tip_host[NAME_LEN];

    pgrp = getpid();
    setpgrp(0, pgrp);		/* TIP not in same pgrp as DFE */
    if(argc < 3)
    {
	TIPPrintUsage(argv[0]);
	fprintf(stderr,"TIP UDI-IPC Version %d.%d.%d\n",
		 (version_c&0xf00)>>8, (version_c&0x0f0)>>4, (version_c&0x00f));
	exit(1);
    }

    signal(SIGINT, sig_handler);
    signal(SIGKILL, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);
    signal(SIGSEGV, sig_handler);
    signal(SIGURG, sig_handler);
    signal(SIGIO, sig_handler);
    signal(SIGUSR1, sig_handler);

/*----------------------------------------------------------------- SOCKET */
    if(!strcmp(domain_string, "AF_UNIX"))
    	domain = AF_UNIX;
    else if(!strcmp(domain_string, "AF_INET"))
    	domain = AF_INET;
    else
    {	fprintf(stderr, " TIP-ipc ERROR: address family not known\n");
	exit(1);
    }
    tip_sd = socket(domain, SOCK_STREAM, 0);
    if (tip_sd == -1 )
    {	perror(" TIP-ipc ERROR, server socket() call failed");
    	sig_handler(-1);
    }
/*------------------------------------------------------------------- BIND */
    if(domain==AF_UNIX)
    {	
        memset( (char*)&tip_sockaddr, 0, sizeof(tip_sockaddr));
        tip_sockaddr.sa_family = domain;
        bcopy(tip_string, tip_sockaddr.sa_data, strlen(tip_string));
	if (bind(tip_sd, &tip_sockaddr, sizeof(tip_sockaddr)) )
	{   perror("TIP-ipc WARNING, server bind() failed");
	}
    }
    else if(domain == AF_INET)
    {
	if(gethostname(tip_host, NAME_LEN))
	{   fprintf(stderr,"TIP-ipc ERROR, unable to get TIP host name");
	    exit(1);
	}
	memset( (char*)&tip_sockaddr_in, 0, sizeof(tip_sockaddr_in));
	tip_sockaddr_in.sin_family = domain;
	tip_sockaddr_in.sin_family = AF_INET;
	tip_sockaddr_in.sin_addr.s_addr = inet_addr(tip_host);
	if( tip_sockaddr_in.sin_addr.s_addr == -1)
	{
	    tip_info_p = gethostbyname(tip_host);	/* use host name */
	    if( tip_info_p == NULL)
	    {  fprintf(stderr," TIP-ipc ERROR, %s not found in /etc/hosts\n",
			tip_host);
	       exit(1);
	    }
	    bcopy(tip_info_p->h_addr, (char *) &tip_sockaddr_in.sin_addr,
	   	tip_info_p->h_length);
	}
	tip_sockaddr_in.sin_port = htons(atoi(tip_string));
	if (bind(tip_sd, &tip_sockaddr_in, sizeof(tip_sockaddr_in)))
	{   perror(" TIP-ipc WARNING, server bind() failed");
	}
    }
/*----------------------------------------------------------------- LISTEN */
    if(listen(tip_sd,1))
    {	perror(" TIP-ipc ERROR, server listen failed");
    	sig_handler(-1);
    }
/*----------------------------------------------------------------- ACCEPT */
    while(1)
    {	while(1)
	{   dfe_addrlen = sizeof(dfe_sockaddr);
	    ns =accept(tip_sd, &dfe_sockaddr, &dfe_addrlen);
    	    if(ns  == -1)
    	    {   if(errno == EINTR) continue;
		perror(" TIP-ipc ERROR, server accept call");
    	        sig_handler(-1);
	    }
	    else break;
    	}
    	errno = 0;
	if(domain == AF_INET)
	{
#ifdef __hpux
#else
    	    if(fcntl(ns, F_SETOWN, getpid()) == -1)
    	    {   perror(" TIP-ipc, fcntl(..F_SETOWN) call: ");
    	        sig_handler(-1);
    	    }
    	    if(fcntl(ns, F_SETFL, FASYNC) == -1)
    	    {   perror(" TIP-ipc, fcntl(..FASYNC) call: ");
    	        sig_handler(-1);
    	    }
#endif
	}
    	udr_create(udrs, ns, SOC_BUF_SIZE);
    	udrs->domain = domain;
    	udrs->soc_name = tip_string;
    	service();
	udr_free(udrs);
   }
}

/******************************************************************** SERVICE
* Service each DFE request as they arrive.
*/
service()
{
 UDIInt32	service_id;	/* requested DFE service */

 for(;;)
 {
  tip_errno = 0;
  udrs->udr_op = UDR_DECODE;
  if(udr_UDIInt32(udrs, &service_id))
  { connect_count--;
    return -1;
  }
  switch(service_id)
  {
    default:
    {
	break;
    }
/*----------------------------------------------------------------- UDIConnect
*/
    case UDIConnect_c:
    {
        char*		Config;		/* in  -- identification string */
        UDISessionId	Session;	/* out -- session ID */
	int		tip_pid;	/* pid of TIP process */
	UDIUInt32	TIPIPCId;
	UDIUInt32	DFEIPCId;

    	udr_UDIUInt32(udrs, &DFEIPCId); /* recv all "in" params */
	if ((DFEIPCId & 0xfff) < version_c) {
	    fprintf(stderr, "TIP-ipc: Connect from obsolete DFE\n");
	    sig_handler(-1);
	}
        udr_string(udrs, sbuf);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	tip_errno = UDIConnect(sbuf, &Session);
	if( tip_errno <= UDINoError) connect_count++;
	TIPIPCId = (company_c << 16) + (product_c << 12) + version_c;
    	udr_UDIUInt32(udrs, &TIPIPCId);
	tip_pid = getpid();
        udr_UDIInt32(udrs, &tip_pid);
        udr_UDISessionId(udrs, &Session);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*-------------------------------------------------------------- UDIDisconnect
*/
    case UDIDisconnect_c:
    {
        UDISessionId	Session;	/* In */
        UDIBool		Terminate;	/* In */

        udr_UDISessionId(udrs, &Session);
    	udr_UDIBool(udrs, &Terminate);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	tip_errno = UDIDisconnect(Session, Terminate);
	if( tip_errno == UDINoError) connect_count--;
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
        if(Terminate != UDIContinueSession && connect_count == 0)
	{
#if 0
	    fprintf(stderr, "\nTIP-ipc: DFE has disconnected all sessions\n");
#endif
	    sig_handler(-1);
	}
 	if(connect_count == 0) return;
	break;
    }
/*-------------------------------------------------------------------- UDIKill
*/
    case UDIKill_c:
    {
        UDISessionId	Session;	/* In */
        UDIInt32	signal;	/* In */

        udr_UDISessionId(udrs, &Session);
    	udr_UDIInt32(udrs, signal);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	tip_errno = 0;
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
        if(connect_count == 0)
	{
#if 0
	    fprintf(stderr, "\nTIP-ipc: DFE has disconnected all sessions\n");
#endif
	    sig_handler(-1);
	}
	break;
    }
/*---------------------------------------------------- UDISetCurrentConnection
*/
    case UDISetCurrentConnection_c:
    {
	UDISessionId	Session;	/* In */

        udr_UDISessionId(udrs, &Session);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDISetCurrentConnection(Session);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*------------------------------------------------------------ UDICapabilities
*/
    case UDICapabilities_c:
    {
	UDIUInt32	TIPId;		/* out */
	UDIUInt32	TargetId;	/* out */
	UDIUInt32	DFEId;		/* in */
	UDIUInt32	DFE;		/* in */
	UDIUInt32	TIP;		/* out */
	UDIUInt32	DFEIPCId;	/* out */
	UDIUInt32	TIPIPCId;	/* out */
	char	*TIPString = sbuf;	/* out */

	strcpy(TIPString, "No TIP Capability string");
	udr_UDIUInt32(udrs, &DFEId);
	udr_UDIUInt32(udrs, &DFE);

    	udrs->udr_op = UDR_ENCODE;		/* send all "out" paramters */
        tip_errno = UDICapabilities( &TIPId, &TargetId, DFEId, DFE,
		&TIP, &DFEIPCId, &TIPIPCId, TIPString);
	udr_UDIUInt32(udrs, &TIPId);
    	udr_UDIUInt32(udrs, &TargetId);
    	udr_UDIUInt32(udrs, &TIP);
    	udr_UDIUInt32(udrs, &DFEIPCId);
	TIPIPCId = (company_c << 16) + (product_c << 12) + version_c;
    	udr_UDIUInt32(udrs, &TIPIPCId);
    	udr_string(udrs, TIPString);
        udr_UDIError(udrs, &tip_errno);		/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*------------------------------------------------------------- UDIGetErrorMsg
*/
    case UDIGetErrorMsg_c:
    {
	UDIError	error_code;	/* In */
	UDISizeT	msg_len;	/* In  -- allowed message space */
	char*		msg = sbuf;	/* Out -- length of message */
	UDISizeT	CountDone;	/* Out -- number of chars */

	strcpy(msg, "No TIP message");
        udr_UDIError(udrs, &error_code); /* recv all "in" parameters */
        udr_UDISizeT(udrs, &msg_len);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	if(error_code < 0)
	{
	    if(SBUF_SIZE < msg_len) msg_len = SBUF_SIZE;
	    tip_errno = UDIGetErrorMsg(error_code, msg_len, msg, &CountDone);
            udr_string(udrs, msg);
	}
	else
	{
            udr_string(udrs, tip_errmsg[0]);
	    tip_errno = UDIErrorCantHappen;
	    CountDone = strlen(tip_errmsg[0]);
	}
	udr_UDISizeT(udrs, &CountDone);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------- UDIGetTargetConfig
*/
    case UDIGetTargetConfig_c:
    {
	char*		KnownMemory_p;		/* Out */
	UDIInt		NumberOfRanges;		/* In and Out */
	char*		ChipVersions_p;		/* Out */
	UDIInt		NumberOfChips;		/* In and Out */
	int		cnt;
	int		MaxOfRanges;
	char*		NumberOfRanges_p;

	udr_UDIInt(udrs, &NumberOfRanges);	/* recieve all "in" params */
	MaxOfRanges = NumberOfRanges;
	udr_UDIInt(udrs, &NumberOfChips);

	udrs->udr_op = UDR_ENCODE;		/* send all "out" parameters */
	udr_work(udrs, &NumberOfRanges, 0);	/* nothing yet */
	KnownMemory_p = udr_getpos(udrs);
	udr_inline(udrs, MaxOfRanges * sizeof(UDIMemoryRange) );
	NumberOfRanges_p = udr_getpos(udrs);
	udr_inline(udrs, sizeof(UDIInt) );	/* space for NumberOfRanges */
	udr_inline(udrs, sizeof(UDIInt) );	/* space for NumberOfChips */
	ChipVersions_p = udr_getpos(udrs);
	tip_errno = UDIGetTargetConfig(KnownMemory_p, &NumberOfRanges,
		ChipVersions_p, &NumberOfChips);
	udr_setpos(udrs, NumberOfRanges_p);
	udr_UDIInt(udrs, &NumberOfRanges);
	udr_UDIInt(udrs, &NumberOfChips);
	udr_inline(udrs, NumberOfChips * sizeof(UDIInt32) );
	udr_UDIError(udrs, &tip_errno);
	udr_sendnow(udrs);
	break;
    }
/*----------------------------------------------------------- UDICreateProcess
*/
    case UDICreateProcess_c:
    {
        UDIPId	pid;		/* out */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDICreateProcess(&pid);
        udr_UDIPId(udrs, &pid);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*------------------------------------------------------- UDIInitializeProcess
*/
    case UDIInitializeProcess_c:
    {
	char*		ProcessMemory_p;	/* In */
	UDIInt		NumberOfRanges;		/* In */
	UDIResource	EntryPoint;		/* In */
	char*		StackSizes_p;		/* In */
	UDIInt		NumberOfStacks;		/* In */
	char		*ArgString = sbuf;	/* In */
	int		cnt;

	udr_UDIInt(udrs, &NumberOfRanges); /* recv all "in" parameters */
	ProcessMemory_p = udr_getpos(udrs);
	udr_inline(udrs, NumberOfRanges * sizeof(UDIMemoryRange) );
    	udr_UDIResource(udrs, &EntryPoint);
    	udr_UDIInt(udrs, &NumberOfStacks);
	StackSizes_p = udr_getpos(udrs);
	udr_inline(udrs, NumberOfStacks * sizeof(CPUSizeT) );
    	udr_string(udrs, ArgString);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	tip_errno = UDIInitializeProcess( ProcessMemory_p, NumberOfRanges,
		EntryPoint, StackSizes_p, NumberOfStacks, ArgString);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*------------------------------------------------------- UDISetCurrentProcess
*/
    case UDISetCurrentProcess_c:
    {
        UDIPId   pid;			/* in */

        udr_UDIPId(udrs, &pid);		/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDISetCurrentProcess(pid);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*---------------------------------------------------------- UDIDestroyProcess
*/
    case UDIDestroyProcess_c:
    {
        UDIPId   pid;			/* in */

        udr_UDIPId(udrs, &pid);		/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIDestroyProcess(pid);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*-------------------------------------------------------------------- UDIRead
*/
    case UDIRead_c:
    {
        UDIResource	from;		/* in - source address on target */
        UDIHostMemPtr	to_dfe;		/* out - destination adds on dfe host */
        UDICount	count;		/* in -- count of objects */
        UDISizeT	size;		/* in -- size of each object */
        UDICount	count_done;	/* out - count actually transferred */
        UDIBool		host_endian;	/* in -- flag for endian information */
	char*		count_done_p;
	char*		to_dfe_p;

        udr_UDIResource(udrs, &from);	/* recv all "in" parameters */
        udr_UDICount(udrs, &count);
        udr_UDISizeT(udrs, &size);
        udr_UDIBool(udrs, &host_endian);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	udr_work(udrs, count_done, 0);	/* nothing to send yet */
	count_done_p = udr_getpos(udrs);
	udr_inline(udrs, sizeof(UDICount));/* make space for count_done */
	to_dfe_p = udr_getpos(udrs);
        tip_errno = UDIRead (from, to_dfe_p, count, size, 
		&count_done, host_endian);
	udr_inline(udrs,count_done*size);/* make space in socket buffer */
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
	udr_setpos(udrs, count_done_p);	/* restore count_done stream position */
        udr_UDICount(udrs, &count_done);
        udr_sendnow(udrs);
	break;
    }
/*------------------------------------------------------------------- UDIWrite
*/
    case UDIWrite_c:
    {
        UDIHostMemPtr	from_dfe;	/* in -- source address on DFE host */
        UDIResource	to;		/* in -- destination adds on target */
        UDICount	count;		/* in -- count of objects */
        UDISizeT	size;		/* in -- size of each object */
        UDICount	count_done;	/* out - count actually transferred */
        UDIBool		host_endian;	/* in -- flag for endian information */
	int		byte_count;
	char*		from_dfe_p;

        udr_UDIResource(udrs, &to);	/* recv all "in" parameters */
        udr_UDICount(udrs, &count);
        udr_UDISizeT(udrs, &size);
        udr_UDIBool(udrs, &host_endian);
	from_dfe_p = udr_getpos(udrs);	/* fake DFE host address */
	byte_count = size * count;
	udr_readnow(udrs, byte_count);	/* read all data available from the 
					   socket into the stream buffer */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIWrite( from_dfe_p, to, count, size, 
		&count_done, host_endian );
        udr_UDICount(udrs, &count_done);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*-------------------------------------------------------------------- UDICopy
*/
    case UDICopy_c:
    {
        UDIResource	from;		/* in -- dest. address on target */
        UDIResource	to;		/* in -- source address on target */
        UDICount	count;		/* in -- count of objects */
        UDISizeT	size;		/* in -- size of each object */
        UDICount	count_done;	/* out - count actually transferred */
        UDIBool		direction;	/* in -- high-to-low or reverse */

        udr_UDIResource(udrs, &from);	/* recv all "in" parameters */
        udr_UDIResource(udrs, &to);
        udr_UDICount(udrs, &count);
        udr_UDISizeT(udrs, &size);
        udr_UDIBool(udrs, &direction);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDICopy(from, to, count, size, &count_done, direction );
        udr_UDICount(udrs, &count_done);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*----------------------------------------------------------------- UDIExecute
*/
    case UDIExecute_c:
    {
        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIExecute();
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*-------------------------------------------------------------------- UDIStep
*/
    case UDIStep_c:
    {
        UDIUInt32	steps;		/* in -- number of steps */
        UDIStepType	steptype;       /* in -- type of stepping to be done */
        UDIRange	range;          /* in -- range if StepInRange is TRUE */

        udr_UDIInt32(udrs, &steps);	/* recv all "in" parameters */
        udr_UDIStepType(udrs, &steptype);
        udr_UDIRange(udrs, &range);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIStep(steps, steptype, range);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*-------------------------------------------------------------------- UDIWait
*/
    case UDIWait_c:
    {
        UDIInt32   maxtime;     /* in -- maximum time to wait for completion */
        UDIPId     pid;        /* out -- pid of process which stopped if any */
        UDIUInt32  stop_reason;/* out -- PC where process stopped */
    
        udr_UDIInt32(udrs, &maxtime);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIWait(maxtime, &pid, &stop_reason);
        udr_UDIPId(udrs, &pid);
        udr_UDIUInt32(udrs, &stop_reason);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*----------------------------------------------------------- UDISetBreakpoint
*/
    case UDISetBreakpoint_c:
    {
        UDIResource	addr;		/* in -- where breakpoint gets set */
        UDIInt32	passcount;	/* in -- passcount for breakpoint  */
        UDIBreakType	type;		/* in -- breakpoint type */
        UDIBreakId	break_id;	/* out - assigned break id */

        udr_UDIResource(udrs, &addr); /* recv all "in" parameters */
        udr_UDIInt32(udrs, &passcount);
        udr_UDIBreakType(udrs, &type);
    
        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDISetBreakpoint (addr, passcount, type, &break_id);
        udr_UDIBreakId(udrs, &break_id);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------- UDIQueryBreakpoint
*/
    case UDIQueryBreakpoint_c:
    {
        UDIBreakId	break_id;	/* in -- assigned break id */
        UDIResource	addr;		/* out - where breakpoint was set */
        UDIInt32	passcount;	/* out - trigger passcount */
        UDIBreakType	type;		/* out - breakpoint type */
        UDIInt32	current_count;	/* out - current breakpoint count */

        udr_UDIBreakId(udrs, &break_id);/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIQueryBreakpoint (break_id, &addr, &passcount,
		&type, &current_count);
        udr_UDIResource(udrs, &addr);
        udr_UDIInt32(udrs, &passcount);
        udr_UDIBreakType(udrs, &type);
        udr_UDIInt32(udrs, &current_count);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------- UDIClearBreakpoint
*/
    case UDIClearBreakpoint_c:
    {
        UDIBreakId	break_id;	/* in -- assigned break id */

        udr_UDIBreakId(udrs, &break_id);/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIClearBreakpoint (break_id);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------------- UDIGetStdout
*/
    case UDIGetStdout_c:
    {
        UDIHostMemPtr	buf_dfe;	/* out -- dfe buffer to be filled */
        UDISizeT	bufsize;	/* in  -- buffer size in bytes */
        UDISizeT	count_done;	/* out -- number of bytes written */
	char*		buf_dfe_p;
        char*		count_done_p;

        udr_UDISizeT(udrs, &bufsize);	/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	udr_work(udrs, count_done, 0);	/* nothing to send yet */
	count_done_p = udr_getpos(udrs);
	udr_inline(udrs,sizeof(UDISizeT));/* make space for count_done */
	buf_dfe_p = udr_getpos(udrs);	/* get start of string buffer */
        tip_errno = UDIGetStdout(buf_dfe_p, bufsize, &count_done);
	udr_inline(udrs, count_done);	/* leave space for string */
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
	udr_setpos(udrs, count_done_p);	/* restore count_done stram position */
        udr_UDISizeT(udrs, &count_done);
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------------- UDIGetStderr
*/
    case UDIGetStderr_c:
    {
        UDIHostMemPtr	buf_dfe;	/* out -- dfe buffer to be filled */
        UDISizeT	bufsize;	/* in  -- buffer size in bytes */
        UDISizeT	count_done;	/* out -- number of bytes written */
        char*		buf_dfe_p;
        char*		count_done_p;

        udr_UDISizeT(udrs, &bufsize);	/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	udr_work(udrs, count_done, 0);	/* nothing to send yet */
	count_done_p = udr_getpos(udrs);
	udr_inline(udrs,sizeof(UDISizeT));/* make space for count_done */
	buf_dfe_p = udr_getpos(udrs);	/* get start of string buffer */
        tip_errno = UDIGetStderr(buf_dfe_p, bufsize, &count_done);
	udr_inline(udrs, count_done);	/* leave space for string */
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
	udr_setpos(udrs, count_done_p);	/* restore count_done stram position */
        udr_UDISizeT(udrs, &count_done);
        udr_sendnow(udrs);
	break;
    }
/*---------------------------------------------------------------- UDIPutStdin
*/
    case UDIPutStdin_c:
    {
        UDIHostMemPtr buf_dfe;		/* in -- buffer to be filled */
        UDISizeT      count;		/* in -- buffer size in bytes */
        UDISizeT      count_done;	/* out - number bytes written to buf */
	char*	buf_dfe_p;		/* pointer to incoming stream data */

	udr_UDISizeT(udrs, &count);	/* recv all "in" parameters */
	buf_dfe_p = udr_getpos(udrs);	/* get start of stream buffer */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIPutStdin (buf_dfe_p, count, &count_done);
        udr_UDISizeT(udrs, &count_done);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------------- UDIStdinMode
*/
    case UDIStdinMode_c:
    {
        UDIMode		mode;		/* out - */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIStdinMode (&mode);
        udr_UDIMode(udrs, &mode);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*---------------------------------------------------------------- UDIPutTrans
*/
    case UDIPutTrans_c:
    {
        UDIHostMemPtr buf;		/* in -- buffer to be filled */
        UDISizeT      count;		/* in -- buffer size in bytes */
        UDISizeT      count_done;	/* out - number bytes written to buf */

	udr_UDISizeT(udrs, &count);	/* recv all "in" parameters */
 	buf = udr_getpos(udrs);	/* get start of stream buffer */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDIPutTrans (buf, count, &count_done);
        udr_UDISizeT(udrs, &count_done);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	break;
    }
/*---------------------------------------------------------------- UDIGetTrans
*/
    case UDIGetTrans_c:
    {
        UDIHostMemPtr	buf_dfe;	/* out -- dfe buffer to be filled */
        UDISizeT	bufsize;	/* in  -- buffer size in bytes */
        UDISizeT	count_done;	/* out -- number of bytes written */
        char*		buf_dfe_p;
        char*		count_done_p;

        udr_UDISizeT(udrs, &bufsize);	/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	udr_work(udrs, count_done, 0);	/* nothing to send yet */
	count_done_p = udr_getpos(udrs);
	udr_inline(udrs,sizeof(UDISizeT));/* make space for count_done */
	buf_dfe_p = udr_getpos(udrs);	/* get start of string buffer */
        tip_errno = UDIGetTrans (buf_dfe_p, bufsize, &count_done);
	udr_inline(udrs, count_done);	/* leave space for string */
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
	udr_setpos(udrs, count_done_p);	/* restore count_done stram position */
        udr_UDISizeT(udrs, &count_done);
        udr_sendnow(udrs);
	break;
    }
/*--------------------------------------------------------------- UDITransMode
*/
    case UDITransMode_c:
    {
        UDIMode		mode;		/* in  -- selected mode */

        udr_UDIMode(udrs, &mode);	/* recv all "in" parameters */

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
        tip_errno = UDITransMode(&mode);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
    }
/*-------------------------------------------------------------------- UDITest
*/
    case UDITest_c:
    {
        UDISizeT	cnt;
        UDIHostMemPtr	str_p;
        UDIHostMemPtr	iarray_dfe;
	UDISizeT	r_cnt;
    	UDIInt16	scnt;
	UDIInt32	array[4];

        udr_UDISizeT(udrs, &cnt);
        udr_UDIInt16(udrs, &scnt);
        udr_bytes(udrs, (char*)array, 4*sizeof(UDIInt32));
        udr_string(udrs, sbuf);

        udrs->udr_op = UDR_ENCODE;	/* send all "out" parameters */
	r_cnt = cnt;
	cnt = array[3]; array[3] = array[0]; array[0] = cnt;
	cnt = array[2]; array[2] = array[1]; array[1] = cnt;
	tip_errno = 0;
        udr_UDISizeT(udrs, &r_cnt);
	scnt = -1* scnt;
        udr_UDIInt16(udrs, &scnt);
        udr_bytes(udrs, (char*)array, 4*sizeof(UDIInt32));
	for(cnt=0; cnt< strlen(sbuf);cnt++)sbuf[cnt] -= 32;
        udr_string(udrs, sbuf);
        udr_UDIError(udrs, &tip_errno);	/* send any TIP error */
        udr_sendnow(udrs);
	if(scnt == 3)
	{   fprintf(stderr,"TIP-ipc test sleep(30) seconds\n");
	    sleep(30);
	}
	break;
    }
  }
 }
}
