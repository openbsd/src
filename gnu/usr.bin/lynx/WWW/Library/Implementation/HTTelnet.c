/*		Telnet Acees, Roligin, etc			HTTelnet.c
**		==========================
**
** Authors
**	TBL	Tim Berners-Lee timbl@info.cern.ch
**	JFG	Jean-Francois Groff jgh@next.com
**	DD	Denis DeLaRoca (310) 825-4580  <CSP1DWD@mvs.oac.ucla.edu>
** History
**	 8 Jun 92 Telnet hopping prohibited as telnet is not secure (TBL)
**	26 Jun 92 When over DECnet, suppressed FTP, Gopher and News. (JFG)
**	 6 Oct 92 Moved HTClientHost and logfile into here. (TBL)
**	17 Dec 92 Tn3270 added, bug fix. (DD)
**	 2 Feb 93 Split from HTAccess.c. Registration.(TBL)
*/

#include "HTUtils.h"
#include "tcp.h"

/* Implements:
*/
#include "HTTelnet.h"

#include "HTParse.h"
#include "HTAnchor.h"
#include "HTTP.h"
#include "HTFile.h"
/*#include <errno.h> included by tcp.h -- FM */
/*#include <stdio.h> included by HTUtils.h -- FM */

#include "HText.h"

#include "HTAccess.h"
#include "HTAlert.h"
#if !defined (VMS) && !defined (_WINDOWS)
#include "../../../userdefs.h"	/* for TELNET_COMMAND and RLOGIN_COMMAND */
#endif /* not VMS */

#ifdef _WINDOWS /* ../../.. doesn't work for me */
#include "userdefs.h"  /* for TELNET_COMMAND and RLOGIN_COMMAND */
#endif

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

#define HT_NO_DATA -9999


/*	Telnet or "rlogin" access
**	-------------------------
*/
PRIVATE int remote_session ARGS2(char *, acc_method, char *, host)
{
	char * user = host;
	char * password = NULL;
	char * cp;
	char * hostname;
	char * port;
	char   command[256];
	enum _login_protocol { telnet, rlogin, tn3270 } login_protocol =
		strcmp(acc_method, "rlogin") == 0 ? rlogin :
		strcmp(acc_method, "tn3270") == 0 ? tn3270 : telnet;
#ifdef VMS
	extern int DCLsystem PARAMS((char *command));
#define system(a) DCLsystem(a) /* use LYCurses.c routines for spawns */
#endif /* VMS */

	/*
	 *	Modified to allow for odd chars in a username only if exists.
	 *	05-28-94 Lynx 2-3-1 Garrett Arch Blythe
	 */
	/* prevent telnet://hostname;rm -rf *  URL's (VERY BAD)
	 *  *cp=0;  / * terminate at any ;,<,>,`,|,",' or space or return
	 *  or tab to prevent security whole
	 */
	for(cp = host; *cp != '\0'; cp++) {
	    if(!isalnum(*cp) && *cp != '_' && *cp != '-' &&
				*cp != ':' && *cp != '.' && *cp != '@') {
		*cp = '\0';
		break;
	    }
	}

	hostname = strchr(host, '@');

	if (hostname) {
	    *hostname++ = '\0'; /* Split */
	} else {
	    hostname = host;
	    user = NULL;	/* No user specified */
	}

	port = strchr(hostname, ':');
	if (port)
	    *port++ = '\0';	/* Split */

    if (!hostname || *hostname == '\0') {
	if (TRACE)
	    fprintf(stderr, "HTTelnet: No host specified!\n");
	return HT_NO_DATA;
    }

    if (user) {
	password = strchr(user, ':');
	if (password) {
	    *password++ = '\0';
	}
    }

/* If the person is already telnetting etc, forbid hopping */
/* This is a security precaution, for us and remote site */

	if (HTSecure) {

#ifdef TELNETHOPPER_MAIL
	    sprintf(command,
	      "finger @%s | mail -s \"**telnethopper %s\" tbl@dxcern.cern.ch",
	       HTClientHost, HTClientHost);
	    system(command);
#endif
	    printf("\n\nSorry, but the service you have selected is one\n");
	    printf("to which you have to log in.  If you were running www\n");
	    printf("on your own computer, you would be automatically connected.\n");
	    printf("For security reasons, this is not allowed when\n");
	    printf("you log in to this information service remotely.\n\n");

	    printf("You can manually connect to this service using %s\n",
		   acc_method);
	    printf("to host %s", hostname);
	    if (user) printf(", user name %s", user);
	    if (password) printf(", password %s", password);
	    if (port) printf(", port %s", port);
	    printf(".\n\n");
	    return HT_NO_DATA;
	}

/* Not all telnet servers get it even if user name is specified
** so we always tell the guy what to log in as
*/
	if (user && login_protocol != rlogin)
	    printf("When you are connected, log in as:  %s\n", user);
	if (password && login_protocol != rlogin)
	    printf("                  The password is:  %s\n", password);

/*
 *	NeXTSTEP is the implied version of the NeXT operating system.
 *		You may need to define this yourself.
 */
#if	defined(NeXT) && defined(NeXTSTEP) && NeXTSTEP<=20100
	sprintf(command, "%s%s%s %s %s", TELNET_COMMAND,
		user ? " -l " : "",
		user ? user : "",
		hostname,
		port ? port : "");

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
#define TELNET_DONE
#endif

/* Most unix machines suppport username only with rlogin */
#if defined(unix) || defined(DOSPATH)
#ifndef TELNET_DONE
	if (login_protocol == rlogin) {
	    snprintf(command, sizeof(command) - 1, "%s %s%s%s", RLOGIN_COMMAND,
		hostname,
		user ? " -l " : "",
		user ? user : "");

	} else if (login_protocol == tn3270) {
	    snprintf(command, sizeof(command) - 1, "%s %s %s", TN3270_COMMAND,
		hostname,
		port ? port : "");

	} else {  /* TELNET */
	    snprintf(command, sizeof(command) - 1, "%s %s %s", TELNET_COMMAND,
		hostname,
		port ? port : "");
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Normal: Command is: %s\n\n", command);
#ifdef __DJGPP__
       __djgpp_set_ctrl_c(0);
       _go32_want_ctrl_break(1);
#endif /* __DJGPP__ */
	system(command);
#ifdef __DJGPP__
       __djgpp_set_ctrl_c(1);
       _go32_want_ctrl_break(0);
#endif /* __DJGPP__ */
	return HT_NO_DATA;		/* Ok - it was done but no data */
#define TELNET_DONE
#endif /* !TELNET_DONE */
#endif /* unix */

/* VMS varieties */
#if defined(MULTINET)
	if (login_protocol == rlogin) {
	    sprintf(command, "RLOGIN%s%s%s%s%s %s",  /*lm 930713 */
		user ? "/USERNAME=\"" : "",
		user ? user : "",
		user ? "\"" : "",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);

	} else if (login_protocol == tn3270) {
	    sprintf(command, "TELNET/TN3270 %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);

	} else {  /* TELNET */
	    sprintf(command, "TELNET %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
#define TELNET_DONE
#endif /* MULTINET */

#if defined(WIN_TCP)
	{
	    char *cp;

	    if ((cp=getenv("WINTCP_COMMAND_STYLE")) != NULL &&
		0==strncasecomp(cp, "VMS", 3)) { /* VMS command syntax */
		if (login_protocol == rlogin) {
		    sprintf(command, "RLOGIN%s%s%s%s%s %s",  /*lm 930713 */
			user ? "/USERNAME=\"" : "",
			user ? user : "",
			user ? "\"" : "",
			port ? "/PORT=" : "",
			port ? port : "",
			hostname);

		} else if (login_protocol == tn3270) {
		    sprintf(command, "TELNET/TN3270 %s%s %s",
			port ? "/PORT=" : "",
			port ? port : "",
			hostname);

		} else {  /* TELNET */
		    sprintf(command, "TELNET %s%s %s",
			port ? "/PORT=" : "",
			port ? port : "",
			hostname);
		}

	    } else { /* UNIX command syntax */
	       if (login_protocol == rlogin) {
		   sprintf(command, "RLOGIN %s%s%s%s%s",
		       hostname,
		       user ? " -l " : "",
		       user ? "\"" : "",
		       user ? user : "",
		       user ? "\"" : "");

		} else if (login_protocol == tn3270) {
		    sprintf(command, "TN3270 %s %s",
			hostname,
			port ? port : "");

		} else {  /* TELNET */
		    sprintf(command, "TELNET %s %s",
			hostname,
			port ? port : "");
		}
	    }

	    if (TRACE)
		fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	    system(command);
	    return HT_NO_DATA;		/* Ok - it was done but no data */
	}
#define TELNET_DONE
#endif /* WIN_TCP */

#ifdef UCX
	if (login_protocol == rlogin) {
	    sprintf(command, "RLOGIN%s%s%s %s %s",
		user ? "/USERNAME=\"" : "",
		user ? user : "",
		user ? "\"" : "",
		hostname,
		port ? port : "");

	} else if (login_protocol == tn3270) {
	    sprintf(command, "TN3270 %s %s",
		hostname,
		port ? port : "");

	} else {  /* TELNET */
	    sprintf(command, "TELNET %s %s",
		hostname,
		port ? port : "");
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
#define TELNET_DONE
#endif /* UCX */

#ifdef CMU_TCP
	if (login_protocol == telnet) {
	    sprintf(command, "TELNET %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);
	    if (TRACE)
		fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	    system(command);
	}
	else {
	    extern int LYgetch NOPARAMS;
	    extern BOOLEAN HadVMSInterrupt;

	    printf(
	"\nSorry, this browser was compiled without the %s access option.\n",
		acc_method);
	    printf("\nPress <return> to return to Lynx.");
	    LYgetch();
	    HadVMSInterrupt = FALSE;
	}
	return HT_NO_DATA;		/* Ok - it was done but no data */
#define TELNET_DONE
#endif /* CMU_TCP */

#ifdef SOCKETSHR_TCP
  {
    char *cp;

    if (getenv("MULTINET_SOCKET_LIBRARY") != NULL) {
	if (login_protocol == rlogin) {
	    sprintf(command, "MULTINET RLOGIN%s%s%s%s %s",  /*lm 930713 */
		user ? "/USERNAME=" : "",
		user ? user : "",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);

	} else if (login_protocol == tn3270) {
	    sprintf(command, "MULTINET TELNET/TN3270 %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);

	} else {  /* TELNET */
	    sprintf(command, "MULTINET TELNET %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
    }
    else if ((cp=getenv("WINTCP_COMMAND_STYLE")) != NULL) {
	if (0==strncasecomp(cp, "VMS", 3)) { /* VMS command syntax */
	    if (login_protocol == rlogin) {
		sprintf(command, "RLOGIN%s%s%s%s %s",  /*lm 930713 */
		    user ? "/USERNAME=" : "",
		    user ? user : "",
		    port ? "/PORT=" : "",
		    port ? port : "",
		    hostname);
	    } else if (login_protocol == tn3270) {
		sprintf(command, "TELNET/TN3270 %s%s %s",
		    port ? "/PORT=" : "",
		    port ? port : "",
		    hostname);
	    } else {  /* TELNET */
		sprintf(command, "TELNET %s%s %s",
		    port ? "/PORT=" : "",
		    port ? port : "",
		    hostname);
	    }
	} else { /* UNIX command syntax */
	    if (login_protocol == rlogin) {
		sprintf(command, "RLOGIN %s%s%s",
		    hostname,
		    user ? " -l " : "",
		    user ? user : "");
	    } else if (login_protocol == tn3270) {
		sprintf(command, "TN3270 %s %s",
		    hostname,
		    port ? port : "");
	    } else {  /* TELNET */
		sprintf(command, "TELNET %s %s",
		    hostname,
		    port ? port : "");
	    }
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
    }
    else if (getenv("UCX$DEVICE") != NULL) {
	if (login_protocol == rlogin) {
	    sprintf(command, "RLOGIN%s%s %s %s",
		user ? "/USERNAME=" : "",
		user ? user : "",
		hostname,
		port ? port : "");

	} else if (login_protocol == tn3270) {
	    sprintf(command, "TN3270 %s %s",
		hostname,
		port ? port : "");

	} else {  /* TELNET */
	    sprintf(command, "TELNET %s %s",
		hostname,
		port ? port : "");
	}

	if (TRACE)
	    fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	system(command);
	return HT_NO_DATA;		/* Ok - it was done but no data */
    }
    else if (getenv("CMUTEK_ROOT") != NULL) {
	if (login_protocol == telnet) {
	    sprintf(command, "TELNET %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);
	    if (TRACE)
		fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	    system(command);
	}
	else {
	    extern int LYgetch NOPARAMS;
	    extern BOOLEAN HadVMSInterrupt;

	    printf(
	  "\nSorry, this browser was compiled without the %s access option.\n",
		acc_method);
	    printf("\nPress <return> to return to Lynx.");
	    LYgetch();
	    HadVMSInterrupt = FALSE;
	}
	return HT_NO_DATA;		/* Ok - it was done but no data */
    }
    else {
	if (login_protocol == telnet) {
	    sprintf(command, "TELNET %s%s %s",
		port ? "/PORT=" : "",
		port ? port : "",
		hostname);
	    if (TRACE)
		fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	    system(command);
	}
	else {
	    extern int LYgetch NOPARAMS;
	    extern BOOLEAN HadVMSInterrupt;

	    printf(
	  "\nSorry, this browser was compiled without the %s access option.\n",
		acc_method);
	    printf("\nPress <return> to return to Lynx.");
	    LYgetch();
	    HadVMSInterrupt = FALSE;
	}
	return HT_NO_DATA;		/* Ok - it was done but no data */
    }
  }
#define TELNET_DONE
#endif /* SOCKETSHR_TCP */

#ifdef VM
#define SIMPLE_TELNET
#endif
#ifdef SIMPLE_TELNET
	if (login_protocol == telnet) { 		/* telnet only */
	    sprintf(command, "TELNET  %s",	/* @@ Bug: port ignored */
		hostname);
	    if (TRACE)
		fprintf(stderr, "HTTelnet: Command is: %s\n\n", command);
	    system(command);
	    return HT_NO_DATA;		/* Ok - it was done but no data */
	}
#endif

#ifndef TELNET_DONE
	printf(
	"\nSorry, this browser was compiled without the %s access option.\n",
		acc_method);
	printf(
	"\nTo access the information you must %s to %s", acc_method, hostname);
	if (port)
	    printf(" (port %s)", port);
	if (user)
	    printf("\nlogging in with username %s", user);
	printf(".\n");
	{
	    extern int LYgetch NOPARAMS;

	    printf("\nPress <return> to return to Lynx.");
	    fflush(stdout);
	    LYgetch();
#ifdef VMS
	    {
		extern BOOLEAN HadVMSInterrupt;
		HadVMSInterrupt = FALSE;
	    }
#endif /* VMS */
	}
	return HT_NO_DATA;
#endif /* !TELNET_DONE */
}

/*	"Load a document" -- establishes a session
**	------------------------------------------
**
** On entry,
**	addr		must point to the fully qualified hypertext reference.
**
** On exit,
**	returns 	<0	Error has occured.
**			>=0	Value of file descriptor or socket to be used
**				 to read data.
**	*pFormat	Set to the format of the file, if known.
**			(See WWW.h)
**
*/
PRIVATE int HTLoadTelnet
ARGS4
(
 CONST char *,		addr,
 HTParentAnchor *,	anchor GCC_UNUSED,
 HTFormat,		format_out GCC_UNUSED,
 HTStream *,		sink			/* Ignored */
)
{
    char * acc_method;
    char * host;
    int status;

    if (sink) {
	if (TRACE)
	    fprintf(stderr,
	   "HTTelnet: Can't output a live session -- must be interactive!\n");
	return HT_NO_DATA;
    }
    acc_method =  HTParse(addr, "file:", PARSE_ACCESS);

    host = HTParse(addr, "", PARSE_HOST);
    if (!host || *host == '\0') {
	status = HT_NO_DATA;
	if (TRACE)
	    fprintf(stderr, "HTTelnet: No host specified!\n");
    } else {
	status = remote_session(acc_method, host);
    }

    FREE(host);
    FREE(acc_method);
    return status;
}


#ifdef GLOBALDEF_IS_MACRO
#define _HTTELNET_C_1_INIT { "telnet", HTLoadTelnet, NULL }
#define _HTTELNET_C_2_INIT { "rlogin", HTLoadTelnet, NULL }
#define _HTTELNET_C_3_INIT { "tn3270", HTLoadTelnet, NULL }
GLOBALDEF (HTProtocol, HTTelnet, _HTTELNET_C_1_INIT );
GLOBALDEF (HTProtocol, HTRlogin, _HTTELNET_C_2_INIT );
GLOBALDEF (HTProtocol, HTTn3270, _HTTELNET_C_3_INIT );
#else
GLOBALDEF PUBLIC HTProtocol HTTelnet = { "telnet", HTLoadTelnet, NULL };
GLOBALDEF PUBLIC HTProtocol HTRlogin = { "rlogin", HTLoadTelnet, NULL };
GLOBALDEF PUBLIC HTProtocol HTTn3270 = { "tn3270", HTLoadTelnet, NULL };
#endif /* GLOBALDEF_IS_MACRO */
