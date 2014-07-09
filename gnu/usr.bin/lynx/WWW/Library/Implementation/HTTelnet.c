/*
 * $LynxId: HTTelnet.c,v 1.41 2013/11/28 11:15:19 tom Exp $
 *
 *		Telnet Access, Rlogin, etc			HTTelnet.c
 *		==========================
 *
 * Authors
 *	TBL	Tim Berners-Lee timbl@info.cern.ch
 *	JFG	Jean-Francois Groff jgh@next.com
 *	DD	Denis DeLaRoca (310) 825-4580  <CSP1DWD@mvs.oac.ucla.edu>
 * History
 *	 8 Jun 92 Telnet hopping prohibited as telnet is not secure (TBL)
 *	26 Jun 92 When over DECnet, suppressed FTP, Gopher and News. (JFG)
 *	 6 Oct 92 Moved HTClientHost and logfile into here. (TBL)
 *	17 Dec 92 Tn3270 added, bug fix. (DD)
 *	 2 Feb 93 Split from HTAccess.c.  Registration.(TBL)
 */

#include <HTUtils.h>
#include <LYUtils.h>

/* Implements:
*/
#include <HTTelnet.h>

#include <HTParse.h>
#include <HTAnchor.h>
#include <HTTP.h>
#include <HTFile.h>

#include <HTTCP.h>
#include <HText.h>

#include <HTAccess.h>
#include <HTAlert.h>

#include <LYStrings.h>
#include <LYClean.h>
#include <LYLeaks.h>

#ifdef __GNUC__
static void do_system(char *) GCC_UNUSED;
#endif

static void do_system(char *command)
{
    if (non_empty(command)) {
	CTRACE((tfp, "HTTelnet: Command is: %s\n\n", command));
	LYSystem(command);
    }
    FREE(command);
}

/*	Telnet or "rlogin" access
 *	-------------------------
 */
static int remote_session(char *acc_method, char *host)
{
    const char *program;
    char *user = host;
    char *password = NULL;
    char *cp;
    char *hostname;
    char *port;
    char *command = NULL;
    enum _login_protocol {
	telnet,
	rlogin,
	tn3270
    } login_protocol =
      strcmp(acc_method, "rlogin") == 0 ? rlogin :
      strcmp(acc_method, "tn3270") == 0 ? tn3270 : telnet;

    /*
     * Modified to allow for odd chars in a username only if exists.
     * 05-28-94 Lynx 2-3-1 Garrett Arch Blythe
     */
    /* prevent telnet://hostname;rm -rf *  URL's (VERY BAD)
     *  *cp=0;        // terminate at any ;,<,>,`,|,",' or space or return
     * or tab to prevent security hole
     */
    for (cp = (StrChr(host, '@') ? StrChr(host, '@') : host); *cp != '\0';
	 cp++) {
	if (!isalnum(UCH(*cp)) && *cp != '_' && *cp != '-' &&
	    *cp != ':' && *cp != '.' && *cp != '@') {
	    *cp = '\0';
	    break;
	}
    }

    hostname = StrChr(host, '@');

    if (hostname) {
	*hostname++ = '\0';	/* Split */
    } else {
	hostname = host;
	user = NULL;		/* No user specified */
    }

    port = StrChr(hostname, ':');
    if (port)
	*port++ = '\0';		/* Split */

    if (*hostname == '\0') {
	CTRACE((tfp, "HTTelnet: No host specified!\n"));
	return HT_NO_DATA;
    } else if (!valid_hostname(hostname)) {
	char *prefix = NULL;
	char *line = NULL;

	CTRACE((tfp, "HTTelnet: Invalid hostname %s!\n", host));
	HTSprintf0(&prefix,
		   gettext("remote %s session:"), acc_method);
	HTSprintf0(&line,
		   gettext("Invalid hostname %s"), host);
	HTAlwaysAlert(prefix, line);
	FREE(prefix);
	FREE(line);
	return HT_NO_DATA;
    }

    if (user) {
	password = StrChr(user, ':');
	if (password) {
	    *password++ = '\0';
	}
    }

    /* If the person is already telnetting etc, forbid hopping */
    /* This is a security precaution, for us and remote site */

    if (HTSecure) {

#ifdef TELNETHOPPER_MAIL
	HTSprintf0(&command,
		   "finger @%s | mail -s \"**telnethopper %s\" tbl@dxcern.cern.ch",
		   HTClientHost, HTClientHost);
	do_system(command);
#endif
	printf("\n\nSorry, but the service you have selected is one\n");
	printf("to which you have to log in.  If you were running www\n");
	printf("on your own computer, you would be automatically connected.\n");
	printf("For security reasons, this is not allowed when\n");
	printf("you log in to this information service remotely.\n\n");

	printf("You can manually connect to this service using %s\n",
	       acc_method);
	printf("to host %s", hostname);
	if (user)
	    printf(", user name %s", user);
	if (password)
	    printf(", password %s", password);
	if (port)
	    printf(", port %s", port);
	printf(".\n\n");
	return HT_NO_DATA;
    }

    /* Not all telnet servers get it even if user name is specified so we
     * always tell the guy what to log in as.
     */
    if (user && login_protocol != rlogin)
	printf("When you are connected, log in as:  %s\n", user);
    if (password && login_protocol != rlogin)
	printf("                  The password is:  %s\n", password);
    fflush(stdout);

/*
 *	NeXTSTEP is the implied version of the NeXT operating system.
 *		You may need to define this yourself.
 */
#if	!defined(TELNET_DONE) && (defined(NeXT) && defined(NeXTSTEP) && NeXTSTEP<=20100)
#define FMT_TELNET "%s%s%s %s %s"

    if ((program = HTGetProgramPath(ppTELNET)) != NULL) {
	HTAddParam(&command, FMT_TELNET, 1, program);
	HTOptParam(&command, FMT_TELNET, 2, user ? " -l " : "");
	HTAddParam(&command, FMT_TELNET, 3, user);
	HTAddParam(&command, FMT_TELNET, 4, hostname);
	HTAddParam(&command, FMT_TELNET, 5, port);
	HTEndParam(&command, FMT_TELNET, 5);
    }
    do_system(command);
#define TELNET_DONE
#endif

/* Most unix machines support username only with rlogin */
#if !defined(TELNET_DONE) && (defined(UNIX) || defined(DOSPATH) || defined(__CYGWIN__))

#define FMT_RLOGIN "%s %s%s%s"
#define FMT_TN3270 "%s %s %s"
#define FMT_TELNET "%s %s %s"

    switch (login_protocol) {
    case rlogin:
	if ((program = HTGetProgramPath(ppRLOGIN)) != NULL) {
	    HTAddParam(&command, FMT_RLOGIN, 1, program);
	    HTAddParam(&command, FMT_RLOGIN, 2, hostname);
	    HTOptParam(&command, FMT_RLOGIN, 3, user ? " -l " : "");
	    HTAddParam(&command, FMT_RLOGIN, 4, user);
	    HTEndParam(&command, FMT_RLOGIN, 4);
	}
	break;

    case tn3270:
	if ((program = HTGetProgramPath(ppTN3270)) != NULL) {
	    HTAddParam(&command, FMT_TN3270, 1, program);
	    HTAddParam(&command, FMT_TN3270, 2, hostname);
	    HTAddParam(&command, FMT_TN3270, 3, port);
	    HTEndParam(&command, FMT_TN3270, 3);
	}
	break;

    case telnet:
	if ((program = HTGetProgramPath(ppTELNET)) != NULL) {
	    HTAddParam(&command, FMT_TELNET, 1, program);
	    HTAddParam(&command, FMT_TELNET, 2, hostname);
	    HTAddParam(&command, FMT_TELNET, 3, port);
	    HTEndParam(&command, FMT_TELNET, 3);
	}
	break;
    }

    LYSystem(command);
#define TELNET_DONE
#endif /* unix */

/* VMS varieties */
#if !defined(TELNET_DONE) && (defined(MULTINET))
    if (login_protocol == rlogin) {
	HTSprintf0(&command, "RLOGIN%s%s%s%s%s %s",	/*lm 930713 */
		   user ? "/USERNAME=\"" : "",
		   NonNull(user),
		   user ? "\"" : "",
		   port ? "/PORT=" : "",
		   NonNull(port),
		   hostname);

    } else if (login_protocol == tn3270) {
	HTSprintf0(&command, "TELNET/TN3270 %s%s %s",
		   port ? "/PORT=" : "",
		   NonNull(port),
		   hostname);

    } else {			/* TELNET */
	HTSprintf0(&command, "TELNET %s%s %s",
		   port ? "/PORT=" : "",
		   NonNull(port),
		   hostname);
    }

    do_system(command);
#define TELNET_DONE
#endif /* MULTINET */

#if !defined(TELNET_DONE) && defined(WIN_TCP)
    if ((cp = getenv("WINTCP_COMMAND_STYLE")) != NULL &&
	0 == strncasecomp(cp, "VMS", 3)) {	/* VMS command syntax */
	if (login_protocol == rlogin) {
	    HTSprintf0(&command, "RLOGIN%s%s%s%s%s %s",		/*lm 930713 */
		       user ? "/USERNAME=\"" : "",
		       NonNull(user),
		       user ? "\"" : "",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);

	} else if (login_protocol == tn3270) {
	    HTSprintf0(&command, "TELNET/TN3270 %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);

	} else {		/* TELNET */
	    HTSprintf0(&command, "TELNET %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);
	}

    } else {			/* UNIX command syntax */
	if (login_protocol == rlogin) {
	    HTSprintf0(&command, "RLOGIN %s%s%s%s%s",
		       hostname,
		       user ? " -l " : "",
		       user ? "\"" : "",
		       NonNull(user),
		       user ? "\"" : "");

	} else if (login_protocol == tn3270) {
	    HTSprintf0(&command, "TN3270 %s %s",
		       hostname,
		       NonNull(port));

	} else {		/* TELNET */
	    HTSprintf0(&command, "TELNET %s %s",
		       hostname,
		       NonNull(port));
	}
    }

    do_system(command);
#define TELNET_DONE
#endif /* WIN_TCP */

#if !defined(TELNET_DONE) && defined(UCX)
    if (login_protocol == rlogin) {
	HTSprintf0(&command, "RLOGIN%s%s%s %s %s",
		   user ? "/USERNAME=\"" : "",
		   NonNull(user),
		   user ? "\"" : "",
		   hostname,
		   NonNull(port));

    } else if (login_protocol == tn3270) {
	HTSprintf0(&command, "TN3270 %s %s",
		   hostname,
		   NonNull(port));

    } else {			/* TELNET */
	HTSprintf0(&command, "TELNET %s %s",
		   hostname,
		   NonNull(port));
    }

    do_system(command);
#define TELNET_DONE
#endif /* UCX */

#if !defined(TELNET_DONE) && defined(CMU_TCP)
    if (login_protocol == telnet) {
	HTSprintf0(&command, "TELNET %s%s %s",
		   port ? "/PORT=" : "",
		   NonNull(port),
		   hostname);
	do_system(command);
    } else {
	printf("\nSorry, this browser was compiled without the %s access option.\n",
	       acc_method);
	printf("\nPress <return> to return to Lynx.");
	LYgetch();
	HadVMSInterrupt = FALSE;
    }
#define TELNET_DONE
#endif /* CMU_TCP */

#if !defined(TELNET_DONE) && defined(SOCKETSHR_TCP)
    if (getenv("MULTINET_SOCKET_LIBRARY") != NULL) {
	if (login_protocol == rlogin) {
	    HTSprintf0(&command, "MULTINET RLOGIN%s%s%s%s %s",	/*lm 930713 */
		       user ? "/USERNAME=" : "",
		       NonNull(user),
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);

	} else if (login_protocol == tn3270) {
	    HTSprintf0(&command, "MULTINET TELNET/TN3270 %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);

	} else {		/* TELNET */
	    HTSprintf0(&command, "MULTINET TELNET %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);
	}

	do_system(command);
	return HT_NO_DATA;	/* Ok - it was done but no data */
    } else if ((cp = getenv("WINTCP_COMMAND_STYLE")) != NULL) {
	if (0 == strncasecomp(cp, "VMS", 3)) {	/* VMS command syntax */
	    if (login_protocol == rlogin) {
		HTSprintf0(&command, "RLOGIN%s%s%s%s %s",	/*lm 930713 */
			   user ? "/USERNAME=" : "",
			   NonNull(user),
			   port ? "/PORT=" : "",
			   NonNull(port),
			   hostname);
	    } else if (login_protocol == tn3270) {
		HTSprintf0(&command, "TELNET/TN3270 %s%s %s",
			   port ? "/PORT=" : "",
			   NonNull(port),
			   hostname);
	    } else {		/* TELNET */
		HTSprintf0(&command, "TELNET %s%s %s",
			   port ? "/PORT=" : "",
			   NonNull(port),
			   hostname);
	    }
	} else {		/* UNIX command syntax */
	    if (login_protocol == rlogin) {
		HTSprintf0(&command, "RLOGIN %s%s%s",
			   hostname,
			   user ? " -l " : "",
			   NonNull(user));
	    } else if (login_protocol == tn3270) {
		HTSprintf0(&command, "TN3270 %s %s",
			   hostname,
			   NonNull(port));
	    } else {		/* TELNET */
		HTSprintf0(&command, "TELNET %s %s",
			   hostname,
			   NonNull(port));
	    }
	}

	do_system(command);
	return HT_NO_DATA;	/* Ok - it was done but no data */
    } else if (getenv("UCX$DEVICE") != NULL
	       || getenv("TCPIP$DEVICE") != NULL) {
	if (login_protocol == rlogin) {
	    HTSprintf0(&command, "RLOGIN%s%s %s %s",
		       user ? "/USERNAME=" : "",
		       NonNull(user),
		       hostname,
		       NonNull(port));

	} else if (login_protocol == tn3270) {
	    HTSprintf0(&command, "TN3270 %s %s",
		       hostname,
		       NonNull(port));

	} else {		/* TELNET */
	    HTSprintf0(&command, "TELNET %s %s",
		       hostname,
		       NonNull(port));
	}

	do_system(command);
	return HT_NO_DATA;	/* Ok - it was done but no data */
    } else if (getenv("CMUTEK_ROOT") != NULL) {
	if (login_protocol == telnet) {
	    HTSprintf0(&command, "TELNET %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);
	    do_system(command);
	} else {
	    printf("\nSorry, this browser was compiled without the %s access option.\n",
		   acc_method);
	    printf("\nPress <return> to return to Lynx.");
	    LYgetch();
	    HadVMSInterrupt = FALSE;
	}
    } else {
	if (login_protocol == telnet) {
	    HTSprintf0(&command, "TELNET %s%s %s",
		       port ? "/PORT=" : "",
		       NonNull(port),
		       hostname);
	    do_system(command);
	} else {
	    printf("\nSorry, this browser was compiled without the %s access option.\n",
		   acc_method);
	    printf("\nPress <return> to return to Lynx.");
	    LYgetch();
	    HadVMSInterrupt = FALSE;
	}
    }
#define TELNET_DONE
#endif /* SOCKETSHR_TCP */

#if !defined(TELNET_DONE) && (defined(SIMPLE_TELNET) || defined(VM))
    if (login_protocol == telnet) {	/* telnet only */
	HTSprintf0(&command, "TELNET  %s",	/* @@ Bug: port ignored */
		   hostname);
	do_system(command);
	return HT_NO_DATA;	/* Ok - it was done but no data */
    }
#define TELNET_DONE
#endif

#ifndef TELNET_DONE
    printf("\nSorry, this browser was compiled without the %s access option.\n",
	   acc_method);
    printf("\nTo access the information you must %s to %s", acc_method, hostname);
    if (port)
	printf(" (port %s)", port);
    if (user)
	printf("\nlogging in with username %s", user);
    printf(".\n");
    {
	printf("\nPress <return> to return to Lynx.");
	fflush(stdout);
	LYgetch();
#ifdef VMS
	HadVMSInterrupt = FALSE;
#endif /* VMS */
    }
#endif /* !TELNET_DONE */
    return HT_NO_DATA;
}

/*	"Load a document" -- establishes a session
 *	------------------------------------------
 *
 * On entry,
 *	addr		must point to the fully qualified hypertext reference.
 *
 * On exit,
 *	returns		<0	Error has occurred.
 *			>=0	Value of file descriptor or socket to be used
 *				 to read data.
 *	*pFormat	Set to the format of the file, if known.
 *			(See WWW.h)
 *
 */
static int HTLoadTelnet(const char *addr,
			HTParentAnchor *anchor GCC_UNUSED,
			HTFormat format_out GCC_UNUSED,
			HTStream *sink)		/* Ignored */
{
    char *acc_method;
    char *host;
    int status;

    if (sink) {
	CTRACE((tfp,
		"HTTelnet: Can't output a live session -- must be interactive!\n"));
	return HT_NO_DATA;
    }
    acc_method = HTParse(addr, STR_FILE_URL, PARSE_ACCESS);

    host = HTParse(addr, "", PARSE_HOST);
    if (!host || *host == '\0') {
	status = HT_NO_DATA;
	CTRACE((tfp, "HTTelnet: No host specified!\n"));
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
GLOBALDEF(HTProtocol, HTTelnet, _HTTELNET_C_1_INIT);
GLOBALDEF(HTProtocol, HTRlogin, _HTTELNET_C_2_INIT);
GLOBALDEF(HTProtocol, HTTn3270, _HTTELNET_C_3_INIT);
#else
GLOBALDEF HTProtocol HTTelnet =
{"telnet", HTLoadTelnet, NULL};
GLOBALDEF HTProtocol HTRlogin =
{"rlogin", HTLoadTelnet, NULL};
GLOBALDEF HTProtocol HTTn3270 =
{"tn3270", HTLoadTelnet, NULL};
#endif /* GLOBALDEF_IS_MACRO */
