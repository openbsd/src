/*
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
* 800-292-9263
*****************************************************************************
*/
static 	char udip2soc_c[]="@(#)udip2soc.c	2.13  Daniel Mann";
static  char udip2soc_c_AMD[]="@(#)udip2soc.c	2.11, AMD";
/* 
*       This module converts UDI Procedural calls into
*	UDI socket messages for UNIX. 
*	It is used by DFE client processes
********************************************************************** HISTORY
*/
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <sys/errno.h>
#include "udiproc.h"
#include "udisoc.h"

extern	int		errno;
extern	int		sys_nerr;
extern	char*		sys_errlist[];
extern	int		udr_errno;
extern	char*		getenv();

/* local type decs. and macro defs. not in a .h  file ************* MACRO/TYPE
*/
#define		version_c 0x121		/* DFE-IPC version id */
#define		TRUE -1
#define		FALSE 0
#define		PORT_NUM 7000
#define		MAX_SESSIONS 5		/* maximum DFE-TIP connections */
#define		SOC_BUF_SIZE 4* 1024	/* size of socket comms buffer */
#define		SBUF_SIZE 500		/* size of string buffer */
#define		ERRMSG_SIZE 500		/* size of error message buffer */

#define	errmsg_m {int ii; for(ii=0; ii<ERRMSG_SIZE; ii++) dfe_errmsg[ii]=0;}

typedef struct connection_str		/* record of connect session */
{
    int		in_use;
    char	connect_id[20];		/* connection identifier */
    char	domain_string[20];	/* dommaing for conection */
    char	tip_string[30];		/* TIP host name for AF_INET */
    char	tip_exe[80];		/* TIP exe name */
    int		dfe_sd;			/* associated DFE socket */
    int		tip_pid;		/* pid of TIP process */
    struct sockaddr_in dfe_sockaddr;
    struct sockaddr_in tip_sockaddr_in;
    struct sockaddr    tip_sockaddr;
} connection_t;

typedef struct session_str
{
    int		  in_use;
    connection_t* soc_con_p;		/* associated connection */
    UDISessionId  tip_id;		/* associated TIP session ID */
} session_t;

/* global dec/defs. which are not in a .h   file ************* EXPORT DEC/DEFS
*/
UDIError	dfe_errno;
char	dfe_errmsg[ERRMSG_SIZE];/* error string */

/* local dec/defs. which are not in a .h   file *************** LOCAL DEC/DEFS
*/
LOCAL connection_t	soc_con[MAX_SESSIONS];	
LOCAL session_t	session[MAX_SESSIONS];	
LOCAL UDR	udr;
LOCAL UDR*	udrs = &udr;		/* UDR for current session */
LOCAL int	current;		/* int-id for current session */
LOCAL char	sbuf[SBUF_SIZE];	/* String handler buffer */
LOCAL char	config_file[80];	/* path/name for config file */

/***************************************************************** UDI_CONNECT
* Establish a new FDE to TIP conection. The file "./udi_soc" or
* "/etc/udi_soc" may be examined to obtain the conection information
* if the "Config" parameter is not a completd "line entry".
*
* NOTE: the Session string must not start whith white-space characters.
* Format of string is:
* <session>   <domain> <soc_name|host_name> <tip_exe|port> <pass to UDIconnect>
* soc2cayman  AF_INET            cayman      7000           <not supported>
* soc2tip     AF_UNIX   astring              tip.exe        ...
*/
UDIConnect(Config, Session)
char*	Config;			/* in  -- identification string */
UDISessionId *Session;		/* out -- session ID */
{
    UDIInt32	service_id = UDIConnect_c;
    int		domain;
    int		cnt=0;
    int		rcnt, pos, fd, params_pos=0;
    char	*tip_main_string;
    char	*env_p;
    struct hostent	*tip_info_p;
    FILE	*f_p;
    UDIUInt32	TIPIPCId;
    UDIUInt32	DFEIPCId;

    sprintf(sbuf, "which udi_soc");
    f_p = popen(sbuf, "r");
    if(f_p)
    {   while( (sbuf[cnt++]=getc(f_p)) != EOF);
	sbuf[cnt-2]=0;
    }
    pclose(f_p);
    errmsg_m;
    for (rcnt=0; rcnt < MAX_SESSIONS; rcnt++)
        if(!session[rcnt].in_use) break;
    if(rcnt >= MAX_SESSIONS)
    {
        sprintf(dfe_errmsg, "DFE-ipc ERROR: Too many sessions already open");
        return UDIErrorIPCLimitation;
    }
    /* One connection can be multiplexed between several sessions. */
    for (cnt=0; cnt < MAX_SESSIONS; cnt++)	
        if(!soc_con[cnt].in_use) break;
    if(cnt >= MAX_SESSIONS)
    {
        sprintf(dfe_errmsg, "DFE-ipc ERROR: Too many connections already open");
        return UDIErrorIPCLimitation;
    }
    *Session = rcnt;
    session[rcnt].soc_con_p = &soc_con[cnt];

    if(strchr(Config, ' '))		/* test if file entry given */
    {
        soc_con[cnt].in_use = TRUE;
        sscanf(Config, "%s %s %s %s %n",
	    soc_con[cnt].connect_id,
	    soc_con[cnt].domain_string,
	    soc_con[cnt].tip_string,
	    soc_con[cnt].tip_exe,
	    &params_pos);
	tip_main_string = Config + params_pos;
    }
    else				/* here if need to read udi_soc file */
    {
	fd = -1;
	env_p = getenv("UDICONF");
	if(env_p)
	{   sprintf(config_file, "%s", env_p);	/* path includes file name */
	    fd = open(config_file, O_RDONLY);
	}
	if(fd == -1)
	{   fd = open("udi_soc", O_RDONLY);
	    strcpy(config_file, "udi_soc");
	}
	if(fd == -1)
	{   fd = open(sbuf, O_RDONLY);
	    strcpy(config_file, sbuf);
	}
	if(fd == -1)
    	{   sprintf(dfe_errmsg, "UDIConnect, can't open udi_soc file:\n%s ",
	    	sys_errlist[errno]);
	    return UDIErrorCantOpenConfigFile;
	}
	while(1)
	{   pos = 0;
	    while((rcnt = read(fd, &sbuf[pos], 1)) != -1)/* read a line */
	    {   if (sbuf[pos] == '\n' || rcnt == 0 )
		    break;
		pos += 1;
	    }
	    sbuf[pos] = 0;			/* terminate string */
            sscanf(sbuf, "%s %s %s %s %n",
	        soc_con[cnt].connect_id,
	        soc_con[cnt].domain_string,
	        soc_con[cnt].tip_string,
	        soc_con[cnt].tip_exe,
		&params_pos);
	    if( strcmp(Config, soc_con[cnt].connect_id)
		|| rcnt == -1 || rcnt == 0)
	        if(rcnt == -1 || rcnt == 0)
    	        {   sprintf(dfe_errmsg,
			"UDIConnect, can't find entry in udi_soc file");
	            return UDIErrorNoSuchConfiguration;
	        }
		else
		    continue;
            soc_con[cnt].in_use = TRUE;		/* here if entry found */
	    tip_main_string = sbuf + params_pos;
	    break;
	}
	close(fd);
    }
/*-------------------------------------------------------------- '*' SOC_ID */
    if( *soc_con[cnt].tip_string == '*'
     && *soc_con[cnt+1].tip_string == 0)
    {
	rcnt = 0;
	pos = getpid();
	do
	{   sprintf(soc_con[cnt].tip_string,"/tmp/udi%.5d", pos++);
	    fd = open(soc_con[cnt].tip_string, O_CREAT);
	    if(rcnt++ > 20)
	    {   sprintf(dfe_errmsg,
			"DFE-ipc ERROR, can't create random socket name\n");
	        return UDIErrorCantConnect;
	    }
	} while(fd == -1);
	close(fd);
	unlink(soc_con[cnt].tip_string);
    }
/*----------------------------------------------------------- SELECT DOMAIN */
    if(!strcmp(soc_con[cnt].domain_string, "AF_UNIX"))
	domain = AF_UNIX;
    else if(!strcmp(soc_con[cnt].domain_string, "AF_INET"))
	domain = AF_INET;
    else
    {   errmsg_m;
    	sprintf(dfe_errmsg, "DFE-ipc ERROR: socket address family not known");
	return UDIErrorBadConfigFileEntry;
    }

/*---------------------------------------------------- MULTIPLEXED SOCKET ? */
/* If the requested session requires communication with
   a TIP which already has a socket connection established,
   then we do not create a new socket but multiplex the
   existing one. A TIP is said to use the same socket if
   socket-name/host-name and the domain are the same.
 */
    for (rcnt=0; rcnt < MAX_SESSIONS; rcnt++)
    {   if( soc_con[rcnt].in_use
	&& !strcmp(soc_con[cnt].domain_string, soc_con[rcnt].domain_string)
	&& !strcmp(soc_con[cnt].tip_string, soc_con[rcnt].tip_string) 
	&& rcnt != cnt	)
	{
	    session[*Session].soc_con_p = &soc_con[rcnt];
	    soc_con[cnt].in_use = FALSE;	/* don't need new connect */
	    goto tip_connect; 
	}
    }
/*------------------------------------------------------------------ SOCKET */
    soc_con[cnt].dfe_sd = socket(domain, SOCK_STREAM, 0);
    if (soc_con[cnt].dfe_sd == -1 )
    {   errmsg_m;
    	sprintf(dfe_errmsg, "DFE-ipc ERROR, socket() call failed %s ",
	    sys_errlist[errno]);
    	UDIKill(cnt);
    }

/*--------------------------------------------------------- AF_UNIX CONNECT */
    if(domain == AF_UNIX)
    {
        memset( (char*)&soc_con[cnt].tip_sockaddr, 0,
            sizeof(soc_con[cnt].tip_sockaddr));
        soc_con[cnt].tip_sockaddr.sa_family = domain;
        bcopy(soc_con[cnt].tip_string,
		soc_con[cnt].tip_sockaddr.sa_data,
		sizeof(soc_con[cnt].tip_sockaddr.sa_data) );
    	if(connect(soc_con[cnt].dfe_sd, 
		&soc_con[cnt].tip_sockaddr,
		sizeof(soc_con[cnt].tip_sockaddr)) == -1)
  	{	/* if connect() fails assume TIP not yet started */
/*------------------------------------------------------------ AF_UNIX EXEC */
	    int	pid;
#ifdef __hpux
	    int	statusp;
#else
	    union	wait statusp;
#endif
	    char*	arg0 = strrchr(soc_con[cnt].tip_exe,'/');

	    if(!arg0) arg0 = soc_con[cnt].tip_exe;
	    else	arg0++;
    
	    if((pid = fork()) == 0)
            {   execlp(
		    soc_con[cnt].tip_exe,
		    arg0,
		    soc_con[cnt].domain_string,
		    soc_con[cnt].tip_string,
		    NULL);
	        exit(1);
            }
#ifdef __hpux
            if(waitpid(pid, &statusp, WNOHANG))
#else
            if(wait4(pid, &statusp, WNOHANG, NULL))
#endif
	    {
	        sprintf(dfe_errmsg, "DFE-ipc ERROR: can't exec the TIP\n");
	        return UDIErrorCantStartTIP;
	    }
	    sleep(2);
			/* not TIP is running, try conect() again */
    	    if(connect(soc_con[cnt].dfe_sd, 
		&soc_con[cnt].tip_sockaddr,
		sizeof(soc_con[cnt].tip_sockaddr)) == -1)
	    {   sprintf(dfe_errmsg, "DFE-ipc ERROR, connect() call failed: %s",
	    		sys_errlist[errno]);
	        return UDIErrorCantConnect;
	    }
	  }
    }
/*--------------------------------------------------------- AF_INET CONNECT */
    if(domain == AF_INET)
    {
	fprintf(stderr,"DFE-ipc WARNING, need to have first started remote TIP\n");
     	memset( (char*)&soc_con[cnt].tip_sockaddr_in, 0,
	    sizeof(soc_con[cnt].tip_sockaddr_in));
	soc_con[cnt].tip_sockaddr_in.sin_family = domain;
	soc_con[cnt].tip_sockaddr_in.sin_addr.s_addr =
	    inet_addr(soc_con[cnt].tip_string);
	if( soc_con[cnt].tip_sockaddr_in.sin_addr.s_addr == -1)
	{
	    tip_info_p = gethostbyname(soc_con[cnt].tip_string);
	    if( tip_info_p == NULL)
	    {   errmsg_m;
	       sprintf(dfe_errmsg,"DFE-ipc ERROR, %s not found in /etc/hosts",
		   soc_con[cnt].tip_string);
	    	return UDIErrorNoSuchConnection;
	    }
	    bcopy(tip_info_p->h_addr,
		(char *)&soc_con[cnt].tip_sockaddr_in.sin_addr,
	   	tip_info_p->h_length);
	}
	soc_con[cnt].tip_sockaddr_in.sin_port=htons(atoi(soc_con[cnt].tip_exe));
    	if(connect(soc_con[cnt].dfe_sd,
		&soc_con[cnt].tip_sockaddr_in,
		sizeof(soc_con[cnt].tip_sockaddr_in)) == -1)
	{   errmsg_m;
    	    sprintf(dfe_errmsg, "DFE-ipc ERROR, connect() call failed %s ",
	    	sys_errlist[errno]);
	    return UDIErrorCantConnect;
    	}
    }
/*------------------------------------------------------------- TIP CONNECT */
    if(cnt ==0) udr_create(udrs, soc_con[cnt].dfe_sd, SOC_BUF_SIZE);
tip_connect:
    current = cnt;
    session[*Session].in_use = TRUE;	/* session id is now in use*/

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    DFEIPCId = (company_c << 16) + (product_c << 12) + version_c;
    udr_UDIUInt32(udrs, &DFEIPCId);
    udr_string(udrs, tip_main_string);
    udr_sendnow(udrs);

    udrs->udr_op = UDR_DECODE;		/* recv all "out" parameters */
    udr_UDIUInt32(udrs, &TIPIPCId);
    if ((TIPIPCId & 0xfff) < version_c)
    {   fprintf(stderr, "DFE-ipc: Obsolete TIP Specified\n");
	return(UDIErrorExecutableNotTIP);
    }
    udr_UDIInt32(udrs, &soc_con[cnt].tip_pid);
    udr_UDISessionId(udrs, &session[*Session].tip_id);
    udr_UDIError(udrs, &dfe_errno);
    if(dfe_errno > 0) UDIKill(Session, 0);
    return dfe_errno;
}

/************************************************************** UDI_Disconnect
* UDIDisconnect() should be called before exiting the
* DFE to ensure proper shut down of the TIP.
*/
UDIError UDIDisconnect(Session,  Terminate)
UDISessionId	Session;
UDIBool		Terminate;
{
    int	cnt;
    UDIInt32	service_id = UDIDisconnect_c;
    if(Session < 0 || Session > MAX_SESSIONS)
    {	errmsg_m;
	sprintf(dfe_errmsg," SessionId not valid (%d)", Session);
	return UDIErrorNoSuchConfiguration;
    }
    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISessionId(udrs, &session[Session].tip_id);
    udr_UDIBool(udrs, &Terminate);
    udr_sendnow(udrs);

    session[Session].in_use = FALSE;	/* session id is now free */
    for (cnt=0; cnt < MAX_SESSIONS; cnt++)
        if(session[cnt].in_use
	&& session[cnt].soc_con_p == session[Session].soc_con_p
		) break;
    if(cnt >= MAX_SESSIONS)	/* test if socket not multiplexed */
        if(shutdown(session[Session].soc_con_p->dfe_sd, 2))
        {   errmsg_m;
     	    sprintf(dfe_errmsg, "DFE-ipc WARNING: socket shutdown failed");
	    return UDIErrorIPCInternal;
        }

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************************** UDI_KILL
* UDIKill() is used to send a signal to the TIP.
* This is a private IPC call.
*/
UDIError UDIKill(Session,  Signal)
UDISessionId	Session;
UDIInt32	Signal;
{
    int	cnt;
    UDIInt32	service_id = UDIKill_c;
    if(Session < 0 || Session > MAX_SESSIONS)
    {	errmsg_m;
	sprintf(dfe_errmsg," SessionId not valid (%d)", Session);
	return UDIErrorNoSuchConfiguration;
    }
    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISessionId(udrs, &session[Session].tip_id);
    udr_UDIInt32(udrs, &Signal);
    udr_sendnow(udrs);

    session[Session].in_use = FALSE;	/* session id is now free */
    for (cnt=0; cnt < MAX_SESSIONS; cnt++)
        if(session[cnt].in_use
	&& session[cnt].soc_con_p == session[Session].soc_con_p
		) break;
    if(cnt < MAX_SESSIONS)	/* test if socket not multiplexed */
        if(shutdown(session[Session].soc_con_p->dfe_sd, 2))
        {   errmsg_m;
     	    sprintf(dfe_errmsg, "DFE-ipc WARNING: socket shutdown failed");
	    return UDIErrorIPCInternal;
        }

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************** UDI_Set_Current_Connection
* If you are connected to multiple TIPs, you can change
* TIPs using UDISetCurrentConnection().
*/
UDIError UDISetCurrentConnection(Session)
UDISessionId	Session;
{
    UDIInt32	service_id = UDISetCurrentConnection_c;

    if(Session < 0 || Session > MAX_SESSIONS)
	return UDIErrorNoSuchConfiguration;
    if(!session[Session].in_use) 		/* test if not in use yet */
	return UDIErrorNoSuchConnection;

    current = Session;
    /* change socket or multiplex the same socket  */
    udrs->sd = session[Session].soc_con_p->dfe_sd;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISessionId(udrs, &session[Session].tip_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************************ UDI_Capabilities
* The DFE uses UDICapabilities() to both inform the TIP
* of what services the DFE offers and to inquire of the
* TIP what services the TIP offers.
*/
UDIError UDICapabilities(TIPId, TargetId, DFEId, DFE, TIP, DFEIPCId,
		TIPIPCId, TIPString)
UDIUInt32	*TIPId;		/* out */
UDIUInt32	*TargetId;	/* out */
UDIUInt32	DFEId;		/* in */
UDIUInt32	DFE;		/* in */
UDIUInt32	*TIP;		/* out */
UDIUInt32	*DFEIPCId;	/* out */
UDIUInt32	*TIPIPCId;	/* out */
char		*TIPString;	/* out */
{
    UDIInt32	service_id = UDICapabilities_c;
    int		size;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIInt32(udrs, &DFEId);
    udr_UDIInt32(udrs, &DFE);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" paramters */
    udr_UDIInt32(udrs, TIPId);
    udr_UDIInt32(udrs, TargetId);
    udr_UDIInt32(udrs, TIP);
    udr_UDIInt32(udrs, DFEIPCId);
    *DFEIPCId = (company_c << 16) + (product_c << 12) + version_c;
    udr_UDIInt32(udrs, TIPIPCId);
    udr_string(udrs, sbuf);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    size = strlen(sbuf);
    if(size +1 > 80) return -1;		/* test if sufficient space */
    strcpy(TIPString, sbuf);
    return dfe_errno;
}

/********************************************************** UDI_Enumerate_TIPs
* Used by the DFE to enquire about available TIP
* connections.
*/
UDIError UDIEnumerateTIPs(UDIETCallback)
  int (*UDIETCallback)();		/* In -- function to callback */
{
    FILE	*fp;

    fp = fopen(config_file, "r");
    if(fp == NULL)
	return UDIErrorCantOpenConfigFile;
    while(fgets( sbuf, SBUF_SIZE, fp))
	if(UDIETCallback( sbuf) == UDITerminateEnumeration)
	    break;
    fclose( fp);
    return UDINoError;			/* return success */
}

/*********************************************************** UDI_GET_ERROR_MSG
* Some errors are target specific. They are indicated
* by a negative error return value. The DFE uses
* UDIGetErrorMsg() to get the descriptive text for
* the error message which can then  be  displayed  to
* the user.
*/
UDIError UDIGetErrorMsg(error_code, msg_len, msg, CountDone)
UDIError	error_code;		/* In */
UDISizeT	msg_len;		/* In  -- allowed message space */
char*		msg;			/* Out -- length of message*/
UDISizeT	*CountDone;		/* Out -- number of characters */
{
    UDIInt32	service_id = UDIGetErrorMsg_c;
    int		size;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIError(udrs, &error_code);
    udr_UDISizeT(udrs, &msg_len);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_string(udrs, sbuf);
    udr_UDISizeT(udrs, CountDone);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    size = strlen(sbuf);
    if(size +1 > msg_len) return -1;	/* test if sufficient space */
    strcpy(msg, sbuf);
    return dfe_errno;
}

/******************************************************* UDI_GET_TARGET_CONFIG
* UDIGetTargetConfig() gets information about the target.
*/
UDIError UDIGetTargetConfig(KnownMemory, NumberOfRanges, ChipVersions,
		NumberOfChips)
UDIMemoryRange	KnownMemory[];		/* Out */
UDIInt		*NumberOfRanges;	/* In and Out */
UDIUInt32	ChipVersions[];		/* Out */
UDIInt		*NumberOfChips;		/* In and Out */
{
    UDIInt32	service_id = UDIGetTargetConfig_c;
    int		cnt;
    int		MaxOfRanges = *NumberOfRanges;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIInt(udrs, NumberOfRanges);
    udr_UDIInt(udrs, NumberOfChips);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" paramters */
    for(cnt=1; cnt <= MaxOfRanges; cnt++)
    	udr_UDIMemoryRange(udrs, &KnownMemory[cnt-1]);
    udr_UDIInt(udrs, NumberOfRanges);
    udr_UDIInt(udrs, NumberOfChips);
    for(cnt=1; cnt <= *NumberOfChips; cnt++)
    	udr_UDIUInt32(udrs, &ChipVersions[cnt -1]);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/********************************************************** UDI_CREATE_PRCOESS
* UDICreateProcess() tells the  target  OS  that  a
* process is to be created and gets a PID back unless
* there is some error.
*/
UDIError UDICreateProcess(pid)
UDIPId	*pid;	/* out */
{
    UDIInt32	service_id = UDICreateProcess_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIPId(udrs, pid);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/***************************************************** UDI_Set_Current_Process
* UDISetCurrentProcess  uses   a   pid   supplied   by
* UDICreateProcess  and  sets it as the default for all
* udi calls until a new one is set.  A user of  a
*/
UDIError UDISetCurrentProcess (pid)
UDIPId	pid;			/* In */
{
    UDIInt32	service_id = UDISetCurrentProcess_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIPId(udrs, &pid);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/****************************************************** UDI_INITIALISE_PROCESS
* UDIInitializeProcess() prepare process for
* execution. (Reset processor if process os processor).
*/
UDIError UDIInitializeProcess( ProcessMemory, NumberOfRanges, EntryPoint,
		StackSizes, NumberOfStacks, ArgString)
UDIMemoryRange	ProcessMemory[];	/* In */
UDIInt		NumberOfRanges;		/* In */
UDIResource	EntryPoint;		/* In */
CPUSizeT	*StackSizes;		/* In */
UDIInt		NumberOfStacks;		/* In */
char		*ArgString;		/* In */
{
    UDIInt32	service_id = UDIInitializeProcess_c;
    int		cnt;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIInt(udrs, &NumberOfRanges);
    for(cnt = 0; cnt < NumberOfRanges; cnt++)
	udr_UDIMemoryRange(udrs, &ProcessMemory[cnt] );
    udr_UDIResource(udrs, &EntryPoint);
    udr_UDIInt(udrs, &NumberOfStacks);
    for(cnt = 0; cnt < NumberOfStacks; cnt++)
	udr_CPUSizeT(udrs, &StackSizes[cnt]);
    udr_string(udrs, ArgString);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/********************************************************* UDI_DESTROY_PROCESS
* UDIDestroyProcess() frees a process resource
* previously created by UDICreateProcess().
*/
UDIError UDIDestroyProcess(pid)
UDIPId   pid;	/* in */
{
    UDIInt32	service_id = UDIDestroyProcess_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIPId(udrs, &pid);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/****************************************************************** UDI_READ
* UDIRead() reads a block of objects from  a  target
* address space  to host space.
*/

UDIError UDIRead (from, to, count, size, count_done, host_endian)
UDIResource	from;		/* in - source address on target */
UDIHostMemPtr	to;		/* out - destination address on host */
UDICount	count;		/* in -- count of objects to be transferred */
UDISizeT	size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		host_endian;	/* in -- flag for endian information */
{
    UDIInt32	service_id = UDIRead_c;
    int		byte_count;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIResource(udrs, &from);
    udr_UDICount(udrs, &count);
    udr_UDISizeT(udrs, &size);
    udr_UDIBool(udrs, &host_endian);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" paramters */
    udr_UDICount(udrs, count_done);
    byte_count = (*count_done) * size;
    if(*count_done > 0 && *count_done <= count)
        udr_bytes(udrs, to, byte_count);
    if(udr_errno) return udr_errno;
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/****************************************************************** UDI_WRITE
* UDIWrite() writes a block  of  objects  from  host
* space  to  a  target  address+space.
*/
UDIError UDIWrite( from, to, count, size, count_done, host_endian )
UDIHostMemPtr	from;		/* in -- source address on host */
UDIResource	to;		/* in -- destination address on target */
UDICount	count;		/* in -- count of objects to be transferred */
UDISizeT	size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		host_endian;	/* in -- flag for endian information */
{
    UDIInt32	service_id = UDIWrite_c;
    int		byte_count = count * size;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIResource(udrs, &to);
    udr_UDICount(udrs, &count);
    udr_UDISizeT(udrs, &size);
    udr_UDIBool(udrs, &host_endian);
    udr_bytes(udrs, from, byte_count);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" paramters */
    udr_UDICount(udrs, count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************************** UDI_COPY
* UDICopy() copies a block of objects from one  target
* get  address/space to another target address/space.
*/
UDIError UDICopy(from, to, count, size, count_done, direction )
UDIResource	from;		/* in -- destination address on target */
UDIResource	to;		/* in -- source address on target */
UDICount	count;		/* in -- count of objects to be transferred */
UDISizeT	size;		/* in -- size of each object */
UDICount	*count_done;	/* out - count actually transferred */
UDIBool		direction;	/* in -- high-to-low or reverse */
{
    UDIInt32	service_id = UDICopy_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIResource(udrs, &from);
    udr_UDIResource(udrs, &to);
    udr_UDICount(udrs, &count);
    udr_UDISizeT(udrs, &size);
    udr_UDIBool(udrs, &direction);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDICount(udrs, count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/***************************************************************** UDI_EXECUTE
* UDIExecute() continues execution  of  the  default
* process from the current PC.
*/
UDIError UDIExecute()
{
    UDIInt32	service_id = UDIExecute_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************************** UDI_STEP
* UDIStep()  specifies  a  number  of  "instruction"
* steps  to  make.
*/
UDIError UDIStep(steps, steptype, range)
UDIUInt32	steps;		/* in -- number of steps */
UDIStepType	steptype;       /* in -- type of stepping to be done */
UDIRange	range;          /* in -- range if StepInRange is TRUE */
{
    UDIInt32	service_id = UDIStep_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIInt32(udrs, &steps);
    udr_UDIStepType(udrs, &steptype);
    udr_UDIRange(udrs, &range);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************************** UDI_STOP
* UDIStop() stops the default process
*/
UDIVoid UDIStop()
{
    if(!strcmp(session[current].soc_con_p->domain_string, "AF_UNIX"))
        kill(session[current].soc_con_p->tip_pid, SIGUSR1);
    else
	udr_signal(udrs);
    return;
}

/******************************************************************** UDI_WAIT
* UDIWait() returns the state of the target  procesor.
*/
UDIError UDIWait(maxtime, pid, stop_reason)
UDIInt32   maxtime;        /* in -- maximum time to wait for completion */
UDIPId     *pid;           /* out -- pid of process which stopped if any */
UDIUInt32  *stop_reason;   /* out -- PC where process stopped */
{
    UDIInt32	service_id = UDIWait_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIInt32(udrs, &maxtime);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIPId(udrs, pid);
    udr_UDIUInt32(udrs, stop_reason);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/********************************************************** UDI_SET_BREAKPOINT
* UDISetBreakpoint() sets a breakpoint  at  an  adress
* and  uses  the  passcount  to state how many
* times that instruction should  be  hit  before  the
* break  occurs.
*/
UDIError UDISetBreakpoint (addr, passcount, type, break_id)
UDIResource	addr;		/* in -- where breakpoint gets set */
UDIInt32	passcount;	/* in -- passcount for breakpoint  */
UDIBreakType	type;		/* in -- breakpoint type */
UDIBreakId	*break_id;	/* out - assigned break id */
{
    UDIInt32	service_id = UDISetBreakpoint_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIResource(udrs, &addr);
    udr_UDIInt32(udrs, &passcount);
    udr_UDIBreakType(udrs, &type);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIBreakId(udrs, break_id);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************** UDI_QUERY_BREAKPOINT
*/
UDIError UDIQueryBreakpoint (break_id, addr, passcount, type, current_count)
UDIBreakId	break_id;	/* in -- assigned break id */
UDIResource	*addr;		/* out - where breakpoint was set */
UDIInt32	*passcount;	/* out - trigger passcount for breakpoint  */
UDIBreakType	*type;		/* out - breakpoint type */
UDIInt32	*current_count;	/* out - current count for breakpoint  */
{
    UDIInt32	service_id = UDIQueryBreakpoint_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIBreakId(udrs, &break_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIResource(udrs, addr);
    udr_UDIInt32(udrs, passcount);
    udr_UDIBreakType(udrs, type);
    udr_UDIInt32(udrs, current_count);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/******************************************************** UDI_CLEAR_BREAKPOINT
* UDIClearBreakpoint() is used to  clear  a  breakpoint.
*/
UDIError UDIClearBreakpoint (break_id)
UDIBreakId	break_id;	/* in -- assigned break id */
{
    UDIInt32	service_id = UDIClearBreakpoint_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIBreakId(udrs, &break_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************************** UDI_GET_STDOUT
* UDIGetStdout()  is  called   when   a   call   to
* UDIWait() indicates there is STD output data ready. 
*/
UDIError UDIGetStdout(buf, bufsize, count_done)
UDIHostMemPtr	buf;		/* out -- buffer to be filled */
UDISizeT	bufsize;	/* in  -- buffer size in bytes */
UDISizeT	*count_done;	/* out -- number of bytes written to buf */
{
    UDIInt32	service_id = UDIGetStdout_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISizeT(udrs, &bufsize);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDISizeT(udrs, count_done);
    udr_bytes(udrs, buf, *count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************************** UDI_GET_STDERR
* UDIGetStderr()  is  called   when   a   call   to
* UDIWait() indicates there is STDERR output data ready
*/
UDIError UDIGetStderr(buf, bufsize, count_done)
UDIHostMemPtr	buf;		/* out -- buffer to be filled */
UDISizeT	bufsize;	/* in  -- buffer size in bytes */
UDISizeT	*count_done;	/* out -- number of bytes written to buf */
{
    UDIInt32	service_id = UDIGetStderr_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISizeT(udrs, &bufsize);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDISizeT(udrs, count_done);
    udr_bytes(udrs, buf, *count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/*************************************************************** UDI_PUT_STDIN
* UDIPutStdin() is called whenever the DFE wants to
* deliver an input character to the TIP.
*/
UDIError UDIPutStdin (buf, count, count_done)
UDIHostMemPtr	buf;		/* in -- buffer to be filled */
UDISizeT	count;		/* in -- buffer size in bytes */
UDISizeT	*count_done;	/* out - number of bytes written to buf */
{
    UDIInt32	service_id = UDIPutStdin_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISizeT(udrs, &count);
    udr_bytes(udrs, buf, count);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDISizeT(udrs, count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************************** UDI_STDIN_MODE
* UDIStdinMode() is used to change the mode that chazcters
* are fetched from the user.
*/
UDIError	UDIStdinMode(mode)
UDIMode		*mode;		/* out - */
{
    UDIInt32	service_id = UDIStdinMode_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIMode(udrs, mode);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/*************************************************************** UDI_PUT_TRANS
* UDIPutTrans() is used to feed input to  the  passthru  mode.
*/
UDIError	UDIPutTrans (buf, count, count_done)
UDIHostMemPtr	buf;		/* in -- buffer address containing input data */
UDISizeT	count;		/* in -- number of bytes in buf */
UDISizeT	*count_done;	/* out-- number of bytes transfered */
{
    UDIInt32	service_id = UDIPutTrans_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISizeT(udrs, &count);
    udr_bytes(udrs, buf, count);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDISizeT(udrs, count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/*************************************************************** UDI_GET_TRANS
* UDIGetTrans() is used to get output lines from the
* passthru mode.
*/
UDIError	UDIGetTrans (buf, bufsize, count_done)
UDIHostMemPtr	buf;		/* out -- buffer to be filled */
UDISizeT	bufsize;	/* in  -- size of buf */
UDISizeT	*count_done;	/* out -- number of bytes in buf */
{
    UDIInt32	service_id = UDIGetTrans_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDISizeT(udrs, &bufsize);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDISizeT(udrs, count_done);
    udr_bytes(udrs, buf, *count_done);
    udr_UDIError(udrs, &dfe_errno);	/* get any TIP error */
    return dfe_errno;
}

/************************************************************** UDI_Trans_Mode
* UDITransMode() is used to change the mode that the
* transparent routines operate in.
*/
UDIError UDITransMode(mode)
UDIMode		*mode;		/* out  -- selected mode */
{
    UDIInt32	service_id = UDITransMode_c;

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);
    udr_UDIMode(udrs, mode);
    udr_sendnow(udrs);
    if(udr_errno) return udr_errno;

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    udr_UDIError(udrs, &dfe_errno);
    return dfe_errno;
}

/******************************************************************** UDI_TEST
*/
UDIError UDITest( cnt, str_p, array)
UDISizeT	cnt;
UDIHostMemPtr	str_p;
UDIInt32	array[];
{
    UDIInt32	service_id = UDITest_c;
    UDIInt16	scnt = cnt;
    UDISizeT	r_cnt;
    char	buf[256];

    udr_errno = 0;
    udrs->udr_op = UDR_ENCODE;		/* send all "in" parameters */
    udr_UDIInt32(udrs, &service_id);

    printf("send	cnt=%d scnt=%d\n", cnt, scnt);
    udr_UDISizeT(udrs, &cnt);
    udr_UDIInt16(udrs, &scnt);
    printf("	array[0]=0x%x array[1]=0x%x array[2]=0x%x array[3]=0x%x\n",
    	array[0], array[1], array[2], array[3]);
    udr_bytes(udrs, (char*)array, 4*sizeof(UDIInt32));
    printf("	string=%s\n", str_p);
    udr_string(udrs, str_p);
    udr_sendnow(udrs);
    if(udr_errno)
    {	fprintf(stderr, " DFE-ipc Send ERROR\n");
	return udr_errno;
    }

    udrs->udr_op = UDR_DECODE;		/* receive all "out" parameters */
    printf("recv	");
    udr_UDISizeT(udrs, &r_cnt);
    udr_UDIInt16(udrs, &scnt);
    printf("	rcnt=%d scnt=%d\n", r_cnt, scnt);
    udr_bytes(udrs, (char*)array, 4*sizeof(UDIInt32));

    printf("	array[0]=0x%x array[1]=0x%x array[2]=0x%x array[3]=0x%x\n",
    	array[0], array[1], array[2], array[3]);
    udr_string(udrs, str_p);
    printf("	string=%s\n", str_p);

    udr_UDIError(udrs, &dfe_errno);
    return dfe_errno;
}

UDIUInt32 UDIGetDFEIPCId()
{
    return ((company_c << 16) + (product_c << 12) + version_c);
}

