/*			File Transfer Protocol (FTP) Client
**			for a WorldWideWeb browser
**			===================================
**
**	A cache of control connections is kept.
**
** Note: Port allocation
**
**	It is essential that the port is allocated by the system, rather
**	than chosen in rotation by us (POLL_PORTS), or the following
**	problem occurs.
**
**	It seems that an attempt by the server to connect to a port which has
**	been used recently by a listen on the same socket, or by another
**	socket this or another process causes a hangup of (almost exactly)
**	one minute.  Therefore, we have to use a rotating port number.
**	The problem remains that if the application is run twice in quick
**	succession, it will hang for what remains of a minute.
**
** Authors
**	TBL	Tim Berners-lee <timbl@info.cern.ch>
**	DD	Denis DeLaRoca 310 825-4580 <CSP1DWD@mvs.oac.ucla.edu>
**	LM	Lou Montulli <montulli@ukanaix.cc.ukans.edu>
**	FM	Foteos Macrides <macrides@sci.wfeb.edu>
** History:
**	 2 May 91	Written TBL, as a part of the WorldWideWeb project.
**	15 Jan 92	Bug fix: close() was used for NETCLOSE for control soc
**	10 Feb 92	Retry if cached connection times out or breaks
**	 8 Dec 92	Bug fix 921208 TBL after DD
**	17 Dec 92	Anon FTP password now just WWWuser@ suggested by DD
**			fails on princeton.edu!
**	27 Dec 93 (FM)	Fixed up so FTP now works with VMS hosts.  Path
**			must be Unix-style and cannot include the device
**			or top directory.
**	?? ??? ?? (LM)	Added code to prompt and send passwords for non
**			anonymous FTP
**	25 Mar 94 (LM)	Added code to recognize different ftp server types
**			and code to parse dates and sizes on most hosts.
**	27 Mar 93 (FM)	Added code for getting dates and sizes on VMS hosts.
**
** Notes:
**			Portions Copyright 1994 Trustees of Dartmouth College
**			Code for recognizing different FTP servers and
**			parsing "ls -l" output taken from Macintosh Fetch
**			program with permission from Jim Matthews,
**			Dartmouth Software Development Team.
*/

/*
BUGS:	@@@	Limit connection cache size!
		Error reporting to user.
		400 & 500 errors are ack'ed by user with windows.
		Use configuration file for user names

**		Note for portability this version does not use select() and
**		so does not watch the control and data channels at the
**		same time.
*/

#if defined(DJGPP) && !defined(WATT32)
#define u_long unsigned long
#endif

#include <HTUtils.h>

#include <HTAlert.h>

#include <HTFTP.h>	/* Implemented here */
#include <HTTCP.h>
#include <HTFont.h>

#define REPEAT_PORT	/* Give the port number for each file */
#define REPEAT_LISTEN	/* Close each listen socket and open a new one */

/* define POLL_PORTS		 If allocation does not work, poll ourselves.*/
#define LISTEN_BACKLOG 2	/* Number of pending connect requests (TCP)*/

#define FIRST_TCP_PORT	1024	/* Region to try for a listening port */
#define LAST_TCP_PORT	5999

#define LINE_LENGTH 256

#include <HTParse.h>
#include <HTAnchor.h>
#include <HTFile.h>	/* For HTFileFormat() */
#include <HTBTree.h>
#include <HTChunk.h>
#ifndef IPPORT_FTP
#define IPPORT_FTP	21
#endif /* !IPORT_FTP */

#include <LYUtils.h>
#include <LYGlobalDefs.h>
#include <LYStrings.h>
#include <LYLeaks.h>

typedef struct _connection {
    struct _connection *	next;	/* Link on list		*/
    u_long			addr;	/* IP address		*/
    int				socket; /* Socket number for communication */
    BOOL			binary; /* Binary mode? */
} connection;

/*		Hypertext object building machinery
*/
#include <HTML.h>

#define PUTC(c) (*targetClass.put_character)(target, c)
#define PUTS(s) (*targetClass.put_string)(target, s)
#define START(e) (*targetClass.start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*targetClass.end_element)(target, e, 0)
#define FREE_TARGET (*targetClass._free)(target)
#define ABORT_TARGET (*targetClass._free)(target)
struct _HTStructured {
	CONST HTStructuredClass *	isa;
	/* ... */
};

/*	Global Variables
**	---------------------
*/
PUBLIC int HTfileSortMethod = FILE_BY_NAME;
#ifndef DISABLE_FTP /*This disables everything to end-of-file */
PRIVATE char ThisYear[8];
PRIVATE char LastYear[8];
PRIVATE int TheDate;
PRIVATE BOOLEAN HaveYears = FALSE;
#ifdef SOCKS
extern BOOLEAN socks_flag;
extern unsigned long socks_bind_remoteAddr;
#endif /* SOCKS */
extern char *personal_mail_address;

/*	Module-Wide Variables
**	---------------------
*/
PRIVATE connection * connections = NULL;/* Linked list of connections */
PRIVATE char response_text[LINE_LENGTH+1];/* Last response from ftp host */
PRIVATE connection * control = NULL;	/* Current connection */
PRIVATE int data_soc = -1;		/* Socket for data transfer =invalid */
PRIVATE char *user_entered_password = NULL;
PRIVATE char *last_username_and_host = NULL;

typedef enum {
	GENERIC_SERVER
	, MACHTEN_SERVER
	, UNIX_SERVER
	, VMS_SERVER
	, CMS_SERVER
	, DCTS_SERVER
	, TCPC_SERVER
	, PETER_LEWIS_SERVER
	, NCSA_SERVER
	, WINDOWS_NT_SERVER
	, WINDOWS_2K_SERVER
	, MS_WINDOWS_SERVER
	, MSDOS_SERVER
	, APPLESHARE_SERVER
	, NETPRESENZ_SERVER
	, DLS_SERVER
} eServerType;

PRIVATE eServerType server_type = GENERIC_SERVER; /* the type of ftp host */
PRIVATE int	unsure_type = FALSE;		/* sure about the type? */
PRIVATE BOOLEAN use_list = FALSE;		/* use the LIST command? */

PRIVATE int	interrupted_in_next_data_char = FALSE;

#ifdef POLL_PORTS
PRIVATE PortNumber	port_number = FIRST_TCP_PORT;
#endif /* POLL_PORTS */

PRIVATE int	master_socket = -1;	/* Listening socket = invalid	*/
PRIVATE char	port_command[255];	/* Command for setting the port */
PRIVATE fd_set	open_sockets;		/* Mask of active channels */
PRIVATE int	num_sockets;		/* Number of sockets to scan */
PRIVATE PortNumber	passive_port;	/* Port server specified for data */


#define NEXT_CHAR HTGetCharacter()	/* Use function in HTFormat.c */

#define DATA_BUFFER_SIZE 2048
PRIVATE char data_buffer[DATA_BUFFER_SIZE];		/* Input data buffer */
PRIVATE char * data_read_pointer;
PRIVATE char * data_write_pointer;
#define NEXT_DATA_CHAR next_data_char()
PRIVATE int close_connection PARAMS((
	connection *	con));


#ifdef LY_FIND_LEAKS
/*
**  This function frees module globals. - FM
*/
PRIVATE void free_FTPGlobals NOARGS
{
    FREE(user_entered_password);
    FREE(last_username_and_host);
    if (control) {
	if (control->socket != -1)
	    close_connection(control);
	FREE(control);
    }
}
#endif /* LY_FIND_LEAKS */

/* PUBLIC						HTVMS_name()
**		CONVERTS WWW name into a VMS name
** ON ENTRY:
**	nn		Node Name (optional)
**	fn		WWW file name
**
** ON EXIT:
**	returns		vms file specification
**
** Bug: Returns pointer to static -- non-reentrant
*/
PUBLIC char * HTVMS_name ARGS2(
	CONST char *,	nn,
	CONST char *,	fn)
{

/*	We try converting the filename into Files-11 syntax.  That is, we assume
**	first that the file is, like us, on a VMS node.  We try remote
**	(or local) DECnet access.  Files-11, VMS, VAX and DECnet
**	are trademarks of Digital Equipment Corporation.
**	The node is assumed to be local if the hostname WITHOUT DOMAIN
**	matches the local one. @@@
*/
    static char *vmsname;
    char * filename = (char*)malloc(strlen(fn)+1);
    char * nodename = (char*)malloc(strlen(nn)+2+1);	/* Copies to hack */
    char *second;		/* 2nd slash */
    char *last;			/* last slash */

    CONST char * hostname = HTHostName();

    if (!filename || !nodename)
	outofmem(__FILE__, "HTVMSname");
    strcpy(filename, fn);
    strcpy(nodename, "");	/* On same node?  Yes if node names match */
    if (strncmp(nn, "localhost", 9)) {
	CONST char *p;
	CONST char *q;
	for (p = hostname, q = nn;
	     *p && *p != '.' && *q && *q != '.'; p++, q++){
	    if (TOUPPER(*p) != TOUPPER(*q)) {
		char *r;
		strcpy(nodename, nn);
		r = strchr(nodename, '.');	/* Mismatch */
		if (r)
		    *r = '\0';			/* Chop domain */
		strcat(nodename, "::");		/* Try decnet anyway */
		break;
	    }
	}
    }

    second = strchr(filename+1, '/');		/* 2nd slash */
    last = strrchr(filename, '/');	/* last slash */

    if (!second) {				/* Only one slash */
	HTSprintf0(&vmsname, "%s%s", nodename, filename + 1);
    } else if (second == last) {		/* Exactly two slashes */
	*second = '\0';		/* Split filename from disk */
	HTSprintf0(&vmsname, "%s%s:%s", nodename, filename+1, second+1);
	*second = '/';	/* restore */
    } else {				/* More than two slashes */
	char * p;
	*second = '\0';		/* Split disk from directories */
	*last = '\0';		/* Split dir from filename */
	HTSprintf0(&vmsname, "%s%s:[%s]%s",
		nodename, filename+1, second+1, last+1);
	*second = *last = '/';	/* restore filename */
	for (p = strchr(vmsname, '['); *p!=']'; p++)
	    if (*p == '/')
		*p = '.';		/* Convert dir sep.  to dots */
    }
    FREE(nodename);
    FREE(filename);
    return vmsname;
}

/*	Procedure: Read a character from the data connection
**	----------------------------------------------------
*/
PRIVATE int next_data_char NOARGS
{
    int status;
    if (data_read_pointer >= data_write_pointer) {
	status = NETREAD(data_soc, data_buffer, DATA_BUFFER_SIZE);
      if (status == HT_INTERRUPTED)
	interrupted_in_next_data_char = 1;
      if (status <= 0)
	return -1;
      data_write_pointer = data_buffer + status;
      data_read_pointer = data_buffer;
    }
#ifdef NOT_ASCII
    {
	char c = *data_read_pointer++;
	return FROMASCII(c);
    }
#else
    return UCH(*data_read_pointer++);
#endif /* NOT_ASCII */
}


/*	Close an individual connection
**
*/
PRIVATE int close_connection ARGS1(
	connection *,	con)
{
    connection * scan;
    int status;
    CTRACE((tfp, "HTFTP: Closing control socket %d\n", con->socket));
    status = NETCLOSE(con->socket);
    if (TRACE && status != 0) {
#ifdef UNIX
	CTRACE((tfp, "HTFTP:close_connection: %s", LYStrerror(errno)));
#else
	if (con->socket != INVSOC)
	    HTInetStatus("HTFTP:close_connection");
#endif
    }
    con->socket = -1;
    if (connections == con) {
	connections = con->next;
	return status;
    }
    for (scan = connections; scan; scan = scan->next) {
	if (scan->next == con) {
	    scan->next = con->next;	/* Unlink */
	    if (control == con)
		control = (connection*)0;
	    return status;
	} /*if */
    } /* for */
    return -1;		/* very strange -- was not on list. */
}

PRIVATE char *help_message_buffer = NULL;  /* global :( */

PRIVATE void init_help_message_cache NOARGS
{
    FREE(help_message_buffer);
}

PRIVATE void help_message_cache_add ARGS1(
	char *,		string)
{
    if (help_message_buffer)
	StrAllocCat(help_message_buffer, string);
    else
	StrAllocCopy(help_message_buffer, string);

    CTRACE((tfp,"Adding message to help cache: %s\n",string));
}

PRIVATE char *help_message_cache_non_empty NOARGS
{
  return(help_message_buffer);
}
PRIVATE char *help_message_cache_contents NOARGS
{
   return(help_message_buffer);
}

/*	Send One Command
**	----------------
**
**	This function checks whether we have a control connection, and sends
**	one command if given.
**
** On entry,
**	control	points to the connection which is established.
**	cmd	points to a command, or is zero to just get the response.
**
**	The command should already be terminated with the CRLF pair.
**
** On exit,
**	returns:  1 for success,
**		  or negative for communication failure (in which case
**		  the control connection will be closed).
*/
PRIVATE int write_cmd ARGS1(
	char *,		cmd)
{
    int status;

    if (!control) {
	CTRACE((tfp, "HTFTP: No control connection set up!!\n"));
	return -99;
    }

    if (cmd) {
	CTRACE((tfp, "  Tx: %s", cmd));
#ifdef NOT_ASCII
	{
	    char * p;
	    for (p = cmd; *p; p++) {
		*p = TOASCII(*p);
	    }
	}
#endif /* NOT_ASCII */
	status = NETWRITE(control->socket, cmd, (int)strlen(cmd));
	if (status < 0) {
	    CTRACE((tfp, "HTFTP: Error %d sending command: closing socket %d\n",
			status, control->socket));
	    close_connection(control);
	    return status;
	}
    }
    return 1;
}

/*	Execute Command and get Response
**	--------------------------------
**
**	See the state machine illustrated in RFC959, p57. This implements
**	one command/reply sequence.  It also interprets lines which are to
**	be continued, which are marked with a "-" immediately after the
**	status code.
**
**	Continuation then goes on until a line with a matching reply code
**	an a space after it.
**
** On entry,
**	control	points to the connection which is established.
**	cmd	points to a command, or is zero to just get the response.
**
**	The command must already be terminated with the CRLF pair.
**
** On exit,
**	returns:  The first digit of the reply type,
**		  or negative for communication failure.
*/
PRIVATE int response ARGS1(
	char *,		cmd)
{
    int result;				/* Three-digit decimal code */
    int continuation_response = -1;
    int status;

    if ((status = write_cmd(cmd)) < 0)
	return status;

    do {
	char *p = response_text;
	for (;;) {
	    int ich = NEXT_CHAR;
	    if (((*p++ = (char) ich) == LF)
			|| (p == &response_text[LINE_LENGTH])) {

		char continuation;

		if (interrupted_in_htgetcharacter) {
		    CTRACE((tfp, "HTFTP: Interrupted in HTGetCharacter, apparently.\n"));
		    NETCLOSE (control->socket);
		    control->socket = -1;
		    return HT_INTERRUPTED;
		}

		*p = '\0';			/* Terminate the string */
		CTRACE((tfp, "    Rx: %s", response_text));

		/* Check for login or help messages */
		if (!strncmp(response_text,"230-",4) ||
		    !strncmp(response_text,"250-",4) ||
		    !strncmp(response_text,"220-",4))
		    help_message_cache_add(response_text+4);

		sscanf(response_text, "%d%c", &result, &continuation);
		if  (continuation_response == -1) {
			if (continuation == '-')  /* start continuation */
			    continuation_response = result;
		} else {	/* continuing */
			if (continuation_response == result &&
			    continuation == ' ')
			    continuation_response = -1; /* ended */
		}
		break;
	    } /* if end of line */

	    if (interrupted_in_htgetcharacter) {
		CTRACE((tfp, "HTFTP: Interrupted in HTGetCharacter, apparently.\n"));
		NETCLOSE (control->socket);
		control->socket = -1;
		return HT_INTERRUPTED;
	    }

	    if (ich == EOF) {
		CTRACE((tfp, "Error on rx: closing socket %d\n",
			    control->socket));
		strcpy(response_text, "000 *** TCP read error on response\n");
		close_connection(control);
		return -1;	/* End of file on response */
	    }
	} /* Loop over characters */

    } while (continuation_response != -1);

    if (result == 421) {
	CTRACE((tfp, "HTFTP: They close so we close socket %d\n",
		    control->socket));
	close_connection(control);
	return -1;
    }
    if ((result == 255 && server_type == CMS_SERVER) &&
	(0 == strncasecomp(cmd, "CWD", 3) ||
	 0 == strcasecomp(cmd, "CDUP"))) {
	/*
	**  Alas, CMS returns 255 on failure to CWD to parent of root. - PG
	*/
	result = 555;
    }
    return result/100;
}

PRIVATE int send_cmd_1 ARGS1(char *, verb)
{
    char command[80];

    sprintf(command, "%.*s%c%c", (int) sizeof(command)-4, verb, CR, LF);
    return response (command);
}

PRIVATE int send_cmd_2 ARGS2(char *, verb, char *, param)
{
    char *command = 0;
    int status;

    HTSprintf0(&command, "%s %s%c%c", verb, param, CR, LF);
    status = response (command);
    FREE(command);

    return status;
}

#define send_cwd(path) send_cmd_2("CWD", path)

/*
 *  This function should try to set the macintosh server into binary mode.
 *  Some servers need an additional letter after the MACB command.
 */
PRIVATE int set_mac_binary ARGS1(
	eServerType,	ServerType)
{
    /* try to set mac binary mode */
    if (ServerType == APPLESHARE_SERVER ||
	ServerType == NETPRESENZ_SERVER) {
	/*
	 *  Presumably E means "Enable".  - KW
	 */
	return(2 == response("MACB E\r\n"));
    } else {
	return(2 == response("MACB\r\n"));
    }
}

/* This function gets the current working directory to help
 * determine what kind of host it is
 */

PRIVATE void get_ftp_pwd ARGS2(
	eServerType *,	ServerType,
	BOOLEAN *,	UseList)
{

    char *cp;
    /* get the working directory (to see what it looks like) */
    int status = response("PWD\r\n");
    if (status < 0) {
	return;
    } else {
	cp = strchr(response_text+5,'"');
	if (cp)
	    *cp = '\0';
	if (*ServerType == TCPC_SERVER) {
	    *ServerType = ((response_text[5] == '/') ?
					  NCSA_SERVER : TCPC_SERVER);
	    CTRACE((tfp, "HTFTP: Treating as %s server.\n",
			 ((*ServerType == NCSA_SERVER) ?
						 "NCSA" : "TCPC")));
	} else if (response_text[5] == '/') {
	    /* path names beginning with / imply Unix,
	     * right?
	     */
	    if (set_mac_binary(*ServerType)) {
		*ServerType = NCSA_SERVER;
		CTRACE((tfp, "HTFTP: Treating as NCSA server.\n"));
	    } else {
		 *ServerType = UNIX_SERVER;
		 *UseList = TRUE;
		 CTRACE((tfp, "HTFTP: Treating as Unix server.\n"));
	    }
	    return;
	} else if (response_text[strlen(response_text)-1] == ']') {
	    /* path names ending with ] imply VMS, right? */
	    *ServerType = VMS_SERVER;
	    *UseList = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as VMS server.\n"));
	} else {
	    *ServerType = GENERIC_SERVER;
	    CTRACE((tfp, "HTFTP: Treating as Generic server.\n"));
	}

	if ((*ServerType == NCSA_SERVER) ||
	    (*ServerType == TCPC_SERVER) ||
	    (*ServerType == PETER_LEWIS_SERVER) ||
	    (*ServerType == NETPRESENZ_SERVER))
	    set_mac_binary(*ServerType);
    }
}

/* This function turns MSDOS-like directory output off for
 * Windows NT servers.
 */

PRIVATE void set_unix_dirstyle ARGS2(
	eServerType *,	ServerType,
	BOOLEAN *,	UseList)
{

    char *cp;
    /* This is a toggle.  It seems we have to toggle in order to see
     * the current state (after toggling), so we may end up toggling
     * twice.  - kw
     */
    int status = response("SITE DIRSTYLE\r\n");
    if (status != 2) {
	*ServerType = GENERIC_SERVER;
	CTRACE((tfp, "HTFTP: DIRSTYLE failed, treating as Generic server.\n"));
	return;
    } else {
	*UseList = TRUE;
	/* Expecting one of:
	 * 200 MSDOS-like directory output is off
	 * 200 MSDOS-like directory output is on
	 * The following code doesn't look for the full exact string -
	 * who knows how the wording may change in some future version.
	 * If the first response isn't recognized, we toggle again
	 * anyway, under the assumption that it's more likely that
	 * the MSDOS setting was "off" originally. - kw
	 */
	cp = strstr(response_text+4, "MSDOS");
	if (cp && strstr(cp, " off")) {
	    return;		/* already off now. */
	} else {
	    response("SITE DIRSTYLE\r\n");
	}
    }
}

/*	Get a valid connection to the host
**	----------------------------------
**
** On entry,
**	arg	points to the name of the host in a hypertext address
** On exit,
**	returns <0 if error
**		socket number if success
**
**	This routine takes care of managing timed-out connections, and
**	limiting the number of connections in use at any one time.
**
**	It ensures that all connections are logged in if they exist.
**	It ensures they have the port number transferred.
*/
PRIVATE int get_connection ARGS2(
	CONST char *,		arg,
	HTParentAnchor *,	anchor)
{
    int status;
    char * command = 0;
    connection * con;
    char * username = NULL;
    char * password = NULL;
    static BOOLEAN firstuse = TRUE;

    if (firstuse) {
	/*
	**  Set up freeing at exit. - FM
	*/
#ifdef LY_FIND_LEAKS
	atexit(free_FTPGlobals);
#endif
	firstuse = FALSE;
    }

    if (control) {
	/*
	**  Reuse this object - KW, DW & FM
	*/
	if (control->socket != -1) {
	    NETCLOSE(control->socket);
	}
	con = control;
	con->addr = 0;
	con->binary = FALSE;
    } else {
	/*
	**  Allocate and init control struct.
	*/
	con = typecalloc(connection);
	if (con == NULL)
	    outofmem(__FILE__, "get_connection");
    }
    con->socket = -1;

    if (!arg) return -1;		/* Bad if no name specified	*/
    if (!*arg) return -1;		/* Bad if name had zero length	*/

/* Get node name:
*/
    CTRACE((tfp, "get_connection(%s)\n", arg));
    {
	char *p1 = HTParse(arg, "", PARSE_HOST);
	char *p2 = strrchr(p1, '@');	/* user? */
	char * pw = NULL;

	if (p2 != NULL) {
	    username = p1;
	    *p2 = '\0';			/* terminate */
	    p1 = p2+1;			/* point to host */
	    pw = strchr(username, ':');
	    if (pw != NULL) {
		*pw++ = '\0';
		password = HTUnEscape(pw);
	    }
	    if (*username)
		HTUnEscape(username);

	    /*
	     *	If the password doesn't exist then we are going to have
	     *	to ask the user for it.  The only problem is that we
	     *	don't want to ask for it every time, so we will store
	     *	away in a primitive fashion.
	     */
	    if (!password) {
		char *tmp = NULL;

		HTSprintf0(&tmp, "%s@%s", username, p1);
		/*
		 *  If the user@host is not equal to the last time through
		 *  or user_entered_password has no data then we need
		 *  to ask the user for the password.
		 */
		if (!last_username_and_host ||
		    strcmp(tmp, last_username_and_host) ||
		    !user_entered_password) {

		    StrAllocCopy(last_username_and_host, tmp);
		    HTSprintf0(&tmp, gettext("Enter password for user %s@%s:"),
				     username, p1);
		    FREE(user_entered_password);
		    user_entered_password = HTPromptPassword(tmp);

		} /* else we already know the password */
		password = user_entered_password;
		FREE(tmp);
	    }
	}

	if (!username)
	    FREE(p1);
    } /* scope of p1 */

    status = HTDoConnect (arg, "FTP", IPPORT_FTP, (int *)&con->socket);

    if (status < 0) {
	if (status == HT_INTERRUPTED) {
	    CTRACE((tfp, "HTFTP: Interrupted on connect\n"));
	} else {
	    CTRACE((tfp, "HTFTP: Unable to connect to remote host for `%s'.\n",
			arg));
	}
	if (status == HT_INTERRUPTED) {
	    _HTProgress (CONNECTION_INTERRUPTED);
	    status = HT_NOT_LOADED;
	} else {
	    HTAlert(gettext("Unable to connect to FTP host."));
	}
	if (con->socket != -1)
	{
	  NETCLOSE(con->socket);
	}

	FREE(username);
	if (control == con)
	    control = NULL;
	FREE(con);
	return status;			/* Bad return */
    }

    CTRACE((tfp, "FTP connected, socket %d  control %ld\n",
		con->socket, (long)con));
    control = con;		/* Current control connection */

    /* Initialise buffering for control connection */
    HTInitInput(control->socket);
    init_help_message_cache();	/* Clear the login message buffer. */


/*	Now we log in		Look up username, prompt for pw.
*/
    status = response((char *)0);	/* Get greeting */

    if (status == HT_INTERRUPTED) {
	CTRACE((tfp, "HTFTP: Interrupted at beginning of login.\n"));
	_HTProgress (CONNECTION_INTERRUPTED);
	NETCLOSE(control->socket);
	control->socket = -1;
	return HT_INTERRUPTED;
    }
    server_type = GENERIC_SERVER;	/* reset */
    if (status == 2) {		/* Send username */
	char *cp;		/* look at greeting text */

	/* don't gettext() this -- incoming text: */
	if (strlen(response_text) > 4) {
	    if ((cp = strstr(response_text, " awaits your command")) ||
		(cp = strstr(response_text, " ready."))) {
		*cp = '\0';
	    }
	    cp = response_text + 4;
	    if (!strncasecomp(cp, "NetPresenz", 10))
		server_type = NETPRESENZ_SERVER;
	} else {
	    cp = response_text;
	}
	StrAllocCopy(anchor->server, cp);

	status = send_cmd_2("USER", (username && *username)
			    ? username
			    : "anonymous");

	if (status == HT_INTERRUPTED) {
	    CTRACE((tfp, "HTFTP: Interrupted while sending username.\n"));
	    _HTProgress (CONNECTION_INTERRUPTED);
	    NETCLOSE(control->socket);
	    control->socket = -1;
	    return HT_INTERRUPTED;
	}
    }
    if (status == 3) {		/* Send password */
	if (password) {
	    /*
	     * We have non-zero length password, so send it. - FM
	     */
	    HTSprintf0(&command, "PASS %s%c%c", password, CR, LF);
	} else {
	    /*
	     * Create and send a mail address as the password. - FM
	     */
	    char *user = NULL;
	    CONST char *host = NULL;
	    char * cp;

	    if (personal_mail_address && *personal_mail_address) {
		/*
		 * We have a non-zero length personal
		 * mail address, so use that. - FM
		 */
		StrAllocCopy(user, personal_mail_address);
		if ((cp=strchr(user, '@')) != NULL) {
		    *cp++ = '\0';
		    host = cp;
		} else {
		    host = HTHostName();
		}
	    } else {
		/*
		 * Use an environment variable and the host global. - FM
		 */
		if ((cp=getenv("USER")) != NULL)
		    StrAllocCopy(user, cp);
		else
		    StrAllocCopy(user, "WWWuser");
		host = HTHostName();
	    }

	    /*
	     * If host is not fully qualified, suppress it
	     * as ftp.uu.net prefers a blank to a bad name
	     */
	    if (!(host) || strchr(host, '.') == NULL)
		host = "";

	    HTSprintf0(&command, "PASS %s@%s%c%c", user, host, CR, LF);
	    FREE(user);
	}
	status = response(command);
	FREE(command);
	if (status == HT_INTERRUPTED) {
	    CTRACE((tfp, "HTFTP: Interrupted while sending password.\n"));
	    _HTProgress (CONNECTION_INTERRUPTED);
	    NETCLOSE(control->socket);
	    control->socket = -1;
	    return HT_INTERRUPTED;
	}
    }
    FREE(username);

    if (status == 3) {
	status = send_cmd_1("ACCT noaccount");
	if (status == HT_INTERRUPTED) {
	    CTRACE((tfp, "HTFTP: Interrupted while sending password.\n"));
	    _HTProgress (CONNECTION_INTERRUPTED);
	    NETCLOSE(control->socket);
	    control->socket = -1;
	    return HT_INTERRUPTED;
	}

    }
    if (status != 2) {
	CTRACE((tfp, "HTFTP: Login fail: %s", response_text));
	/* if (control->socket > 0) close_connection(control->socket); */
	return -1;		/* Bad return */
    }
    CTRACE((tfp, "HTFTP: Logged in.\n"));

    /** Check for host type **/
    if (server_type != NETPRESENZ_SERVER)
	server_type = GENERIC_SERVER;	/* reset */
    use_list = FALSE;			/* reset */
    if ((status=response("SYST\r\n")) == 2) {
	/* we got a line -- what kind of server are we talking to? */
	if (strncmp(response_text+4,
		    "UNIX Type: L8 MAC-OS MachTen", 28) == 0) {
	    server_type = MACHTEN_SERVER;
	    use_list = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as MachTen server.\n"));

	} else if (strstr(response_text+4, "UNIX") != NULL ||
		   strstr(response_text+4, "Unix") != NULL) {
	    server_type = UNIX_SERVER;
	    unsure_type = FALSE; /* to the best of out knowledge... */
	    use_list = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as Unix server.\n"));

	} else if (strstr(response_text+4, "MSDOS") != NULL) {
	    server_type = MSDOS_SERVER;
	    use_list = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as MSDOS (Unix emulation) server.\n"));

	} else if (strncmp(response_text+4, "VMS", 3) == 0) {
	    char *tilde = strstr(arg, "/~");
	    use_list = TRUE;
	    if (tilde != 0
	     && tilde[2] != 0
	     && strstr(response_text+4, "MadGoat") != 0) {
		server_type = UNIX_SERVER;
		CTRACE((tfp, "HTFTP: Treating VMS as UNIX server.\n"));
	    } else {
		server_type = VMS_SERVER;
		CTRACE((tfp, "HTFTP: Treating as VMS server.\n"));
	    }

	} else if ((strncmp(response_text+4, "VM/CMS", 6) == 0) ||
		   (strncmp(response_text+4, "VM ", 3) == 0)) {
	    server_type = CMS_SERVER;
	    use_list = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as CMS server.\n"));

	} else if (strncmp(response_text+4, "DCTS", 4) == 0) {
	    server_type = DCTS_SERVER;
	    CTRACE((tfp, "HTFTP: Treating as DCTS server.\n"));

	} else if (strstr(response_text+4, "MAC-OS TCP/Connect II") != NULL) {
	    server_type = TCPC_SERVER;
	    CTRACE((tfp, "HTFTP: Looks like a TCPC server.\n"));
	    get_ftp_pwd(&server_type, &use_list);
	    unsure_type = TRUE;

	} else if (server_type == NETPRESENZ_SERVER) { /* already set above */
	    use_list = TRUE;
	    set_mac_binary(server_type);
	    CTRACE((tfp, "HTFTP: Treating as NetPresenz (MACOS) server.\n"));

	} else if (strncmp(response_text+4, "MACOS Peter's Server", 20) == 0) {
	    server_type = PETER_LEWIS_SERVER;
	    use_list = TRUE;
	    set_mac_binary(server_type);
	    CTRACE((tfp, "HTFTP: Treating as Peter Lewis (MACOS) server.\n"));

	} else if (strncmp(response_text+4, "Windows_NT", 10) == 0) {
	    server_type = WINDOWS_NT_SERVER;
	    CTRACE((tfp, "HTFTP: Treating as Window_NT server.\n"));
	    set_unix_dirstyle(&server_type, &use_list);

	} else if (strncmp(response_text+4, "Windows2000", 11) == 0) {
	    server_type = WINDOWS_2K_SERVER;
	    CTRACE((tfp, "HTFTP: Treating as Window_2K server.\n"));
	    set_unix_dirstyle(&server_type, &use_list);

	} else if (strncmp(response_text+4, "MS Windows", 10) == 0) {
	    server_type = MS_WINDOWS_SERVER;
	    use_list = TRUE;
	    CTRACE((tfp, "HTFTP: Treating as MS Windows server.\n"));

	} else if (strncmp(response_text+4,
			   "MACOS AppleShare IP FTP Server", 30) == 0) {
	    server_type = APPLESHARE_SERVER;
	    use_list = TRUE;
	    set_mac_binary(server_type);
	    CTRACE((tfp, "HTFTP: Treating as AppleShare server.\n"));

	} else	{
	    server_type = GENERIC_SERVER;
	    CTRACE((tfp, "HTFTP: Ugh!  A Generic server.\n"));
	    get_ftp_pwd(&server_type, &use_list);
	    unsure_type = TRUE;
	 }
    } else {
	/* SYST fails :(  try to get the type from the PWD command */
	 get_ftp_pwd(&server_type, &use_list);
    }

/*  Now we inform the server of the port number we will listen on
*/
#ifdef NOTREPEAT_PORT
    {
	int status = response(port_command);
	if (status != 2) {
	    if (control->socket)
		close_connection(control->socket);
	    return -status;		/* Bad return */
	}
	CTRACE((tfp, "HTFTP: Port defined.\n"));
    }
#endif /* NOTREPEAT_PORT */
    return con->socket;			/* Good return */
}


/*	Close Master (listening) socket
**	-------------------------------
**
**
*/
PRIVATE int close_master_socket NOARGS
{
    int status;

    if (master_socket != -1)
	FD_CLR(master_socket, &open_sockets);
    status = NETCLOSE(master_socket);
    CTRACE((tfp, "HTFTP: Closed master socket %d\n", master_socket));
    master_socket = -1;
    if (status < 0)
	return HTInetStatus(gettext("close master socket"));
    else
	return status;
}


/*	Open a master socket for listening on
**	-------------------------------------
**
**	When data is transferred, we open a port, and wait for the server to
**	connect with the data.
**
** On entry,
**	master_socket	Must be negative if not set up already.
** On exit,
**	Returns		socket number if good
**			less than zero if error.
**	master_socket	is socket number if good, else negative.
**	port_number	is valid if good.
*/
PRIVATE int get_listen_socket NOARGS
{
#ifdef INET6
    struct sockaddr_storage soc_address;	/* Binary network address */
    struct sockaddr_in* soc_in = (struct sockaddr_in *)&soc_address;
    int af;
    int slen;
#else
    struct sockaddr_in soc_address;	/* Binary network address */
    struct sockaddr_in* soc_in = &soc_address;
#endif /* INET6 */
    int new_socket;			/* Will be master_socket */


    FD_ZERO(&open_sockets);	/* Clear our record of open sockets */
    num_sockets = 0;

#ifndef REPEAT_LISTEN
    if (master_socket >= 0)
	return master_socket;  /* Done already */
#endif /* !REPEAT_LISTEN */

#ifdef INET6
    /* query address family of control connection */
    slen = sizeof(soc_address);
    if (getsockname(control->socket, (struct sockaddr *)&soc_address,
		&slen) < 0) {
	return HTInetStatus("getsockname failed");
    }
    af = ((struct sockaddr *)&soc_address)->sa_family;
    memset(&soc_address, 0, sizeof(soc_address));
#endif /* INET6 */

/*  Create internet socket
*/
#ifdef INET6
    new_socket = socket(af, SOCK_STREAM, IPPROTO_TCP);
#else
    new_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#endif /* INET6 */

    if (new_socket < 0)
	return HTInetStatus(gettext("socket for master socket"));

    CTRACE((tfp, "HTFTP: Opened master socket number %d\n", new_socket));

/*  Search for a free port.
*/
#ifdef INET6
    memset(&soc_address, 0, sizeof(soc_address));
    ((struct sockaddr *)&soc_address)->sa_family = af;
    switch (af) {
    case AF_INET:
#ifdef SIN6_LEN
	((struct sockaddr *)&soc_address)->sa_len = sizeof(struct sockaddr_in);
#endif /* SIN6_LEN */
	break;
    case AF_INET6:
#ifdef SIN6_LEN
	((struct sockaddr *)&soc_address)->sa_len = sizeof(struct sockaddr_in6);
#endif /* SIN6_LEN */
	break;
    default:
	HTInetStatus("AF");
    }
#else
    soc_in->sin_family = AF_INET;	    /* Family = internet, host order  */
    soc_in->sin_addr.s_addr = INADDR_ANY;   /* Any peer address */
#endif /* INET6 */
#ifdef POLL_PORTS
    {
	PortNumber old_port_number = port_number;
	for (port_number = (old_port_number+1); ; port_number++) {
	    int status;
	    if (port_number > LAST_TCP_PORT)
		port_number = FIRST_TCP_PORT;
	    if (port_number == old_port_number) {
		return HTInetStatus("bind");
	    }
#ifdef INET6
	    soc_in->sin_port = htons(port_number);
#else
	    soc_address.sin_port = htons(port_number);
#endif /* INET6 */
#ifdef SOCKS
	    if (socks_flag)
		if ((status=Rbind(new_socket,
			(struct sockaddr*)&soc_address,
			    /* Cast to generic sockaddr */
			SOCKADDR_LEN(soc_address)
#ifndef SHORTENED_RBIND
			,socks_bind_remoteAddr
#endif /* !SHORTENED_RBIND */
						)) == 0) {
		    break;
		} else
#endif /* SOCKS */
	    if ((status=bind(new_socket,
		    (struct sockaddr*)&soc_address,
			    /* Cast to generic sockaddr */
		    SOCKADDR_LEN(soc_address)
		    )) == 0) {
		break;
	    }
	    CTRACE((tfp, "TCP bind attempt to port %d yields %d, errno=%d\n",
		port_number, status, SOCKET_ERRNO));
	} /* for */
    }
#else
    {
	int status;
	int address_length = sizeof(soc_address);
#ifdef SOCKS
	if (socks_flag)
	    status = Rgetsockname(control->socket,
				  (struct sockaddr *)&soc_address,
				  (void *)&address_length);
	else
#endif /* SOCKS */
	status = getsockname(control->socket,
			     (struct sockaddr *)&soc_address,
			     (void *)&address_length);
	if (status<0) return HTInetStatus("getsockname");
#ifdef INET6
	CTRACE((tfp, "HTFTP: This host is %s\n",
	    HTInetString((SockA *)soc_in)));

	soc_in->sin_port = 0;	/* Unspecified: please allocate */
#else
	CTRACE((tfp, "HTFTP: This host is %s\n",
	    HTInetString(soc_in)));

	soc_address.sin_port = 0;	/* Unspecified: please allocate */
#endif /* INET6 */
#ifdef SOCKS
	if (socks_flag)
	    status=Rbind(new_socket,
			 (struct sockaddr*)&soc_address,
			 /* Cast to generic sockaddr */
			 sizeof(soc_address)
#ifndef SHORTENED_RBIND
#ifdef INET6
			socks_bind_remoteAddr
#else
			,socks_bind_remoteAddr
#endif /* INET6 */
#endif /* !SHORTENED_RBIND */
						);
	else
#endif /* SOCKS */
	status=bind(new_socket,
		    (struct sockaddr*)&soc_address,
		    /* Cast to generic sockaddr */
		    SOCKADDR_LEN(soc_address)
		    );
	if (status<0) return HTInetStatus("bind");

	address_length = sizeof(soc_address);
#ifdef SOCKS
	if (socks_flag)
	    status = Rgetsockname(new_socket,
				  (struct sockaddr*)&soc_address,
				  (void *)&address_length);
	else
#endif /* SOCKS */
	status = getsockname(new_socket,
			     (struct sockaddr*)&soc_address,
			     (void *)&address_length);
	if (status<0) return HTInetStatus("getsockname");
    }
#endif /* POLL_PORTS */

#ifdef INET6
    CTRACE((tfp, "HTFTP: bound to port %d on %s\n",
		(int)ntohs(soc_in->sin_port),
		HTInetString((SockA *)soc_in)));
#else
    CTRACE((tfp, "HTFTP: bound to port %d on %s\n",
		(int)ntohs(soc_in->sin_port),
		HTInetString(soc_in)));
#endif /* INET6 */

#ifdef REPEAT_LISTEN
    if (master_socket >= 0)
	(void) close_master_socket();
#endif /* REPEAT_LISTEN */

    master_socket = new_socket;

/*	Now we must find out who we are to tell the other guy
*/
    (void)HTHostName();		/* Make address valid - doesn't work*/
#ifdef INET6
    switch (((struct sockaddr *)&soc_address)->sa_family) {
    case AF_INET:
#endif /* INET6 */
	sprintf(port_command, "PORT %d,%d,%d,%d,%d,%d%c%c",
		    (int)*((unsigned char *)(&soc_in->sin_addr)+0),
		    (int)*((unsigned char *)(&soc_in->sin_addr)+1),
		    (int)*((unsigned char *)(&soc_in->sin_addr)+2),
		    (int)*((unsigned char *)(&soc_in->sin_addr)+3),
		    (int)*((unsigned char *)(&soc_in->sin_port)+0),
		    (int)*((unsigned char *)(&soc_in->sin_port)+1),
		    CR, LF);

#ifdef INET6
	break;

    case AF_INET6:
      {
	char hostbuf[MAXHOSTNAMELEN];
	char portbuf[MAXHOSTNAMELEN];
	getnameinfo((struct sockaddr *)&soc_address,
	    SOCKADDR_LEN(soc_address),
	    hostbuf, sizeof(hostbuf), portbuf, sizeof(portbuf),
	    NI_NUMERICHOST | NI_NUMERICSERV);
	sprintf(port_command, "EPRT |%d|%s|%s|%c%c", 2, hostbuf, portbuf,
		CR, LF);
	break;
      }
    default:
	sprintf(port_command, "JUNK%c%c", CR, LF);
	break;
    }
#endif /* INET6 */

    /*	Inform TCP that we will accept connections
    */
    {
	int status;
#ifdef SOCKS
	if (socks_flag)
	    status = Rlisten(master_socket, 1);
	else
#endif /* SOCKS */
	    status = listen(master_socket, 1);
	if (status < 0) {
	    master_socket = -1;
	    return HTInetStatus("listen");
	}
    }
    CTRACE((tfp, "TCP: Master socket(), bind() and listen() all OK\n"));
    FD_SET(master_socket, &open_sockets);
    if ((master_socket+1) > num_sockets)
	num_sockets = master_socket+1;

    return master_socket;		/* Good */

} /* get_listen_socket */

PRIVATE char * months[12] = {
    "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

/*	Procedure: Set the current and last year strings and date integer
**	-----------------------------------------------------------------
**
**	Bug:
**		This code is for sorting listings by date, if that option
**		is selected in Lynx, and doesn't take into account time
**		zones or ensure resetting at midnight, so the sort may not
**		be perfect, but the actual date isn't changed in the display,
**		i.e., the date is still correct. - FM
*/
PRIVATE void set_years_and_date NOARGS
{
    char day[8], month[8], date[12];
    time_t NowTime;
    int i;

    NowTime = time(NULL);
    strncpy(day, (char *)ctime(&NowTime)+8, 2);
    day[2] = '\0';
    if (day[0] == ' ') {
	day[0] = '0';
    }
    strncpy(month, (char *)ctime(&NowTime)+4, 3);
    month[3] = '\0';
    for (i = 0; i < 12; i++) {
	if (!strcasecomp(month, months[i])) {
	    break;
	}
    }
    i++;
    sprintf(date, "9999%02d%.2s", i, day);
    TheDate = atoi(date);
    strcpy(ThisYear, (char *)ctime(&NowTime)+20);
    ThisYear[4] = '\0';
    sprintf(LastYear, "%d", (atoi(ThisYear) - 1));
    HaveYears = TRUE;
}

typedef struct _EntryInfo {
    char *	 filename;
    char *	 type;
    char *	 date;
    unsigned int size;
    BOOLEAN	 display;  /* show this entry? */
} EntryInfo;

PRIVATE void free_entryinfo_struct_contents ARGS1(
	EntryInfo *,	entry_info)
{
    if (entry_info) {
	FREE(entry_info->filename);
	FREE(entry_info->type);
	FREE(entry_info->date);
    }
   /* dont free the struct */
}

/*
 * is_ls_date() --
 *	Return TRUE if s points to a string of the form:
 *		"Sep  1  1990 " or
 *		"Sep 11 11:59 " or
 *		"Dec 12 1989  " or
 *		"FCv 23 1990  " ...
 */
PRIVATE BOOLEAN is_ls_date ARGS1(
	char *,		s)
{
    /* must start with three alpha characters */
    if (!isalpha(UCH(*s++)) || !isalpha(UCH(*s++)) || !isalpha(UCH(*s++)))
	return FALSE;

    /* space or HT_NON_BREAK_SPACE */
    if (!(*s == ' ' || *s == HT_NON_BREAK_SPACE)) {
	s++;
	return FALSE;
    }
    s++;

    /* space or digit */
    if (!(*s == ' ' || isdigit(UCH(*s)))) {
	s++;
	return FALSE;
    }
    s++;

    /* digit */
    if (!isdigit(UCH(*s++)))
	return FALSE;

    /* space */
    if (*s++ != ' ')
	return FALSE;

    /* space or digit */
    if (!(*s == ' ' || isdigit(UCH(*s)))) {
	s++;
	return FALSE;
    }
    s++;

    /* digit */
    if (!isdigit(UCH(*s++)))
	return FALSE;

    /* colon or digit */
    if (!(*s == ':' || isdigit(UCH(*s)))) {
	s++;
	return FALSE;
    }
    s++;

    /* digit */
    if (!isdigit(UCH(*s++)))
	return FALSE;

    /* space or digit */
    if (!(*s == ' ' || isdigit(UCH(*s)))) {
	s++;
	return FALSE;
    }
    s++;

    /* space */
    if (*s++ != ' ')
	return FALSE;

    return TRUE;
} /* is_ls_date() */

/*
 *  parse_eplf_line() --
 *	Extract the name, size, and date from an EPLF line. - 08-06-96 DJB
 */
PRIVATE void parse_eplf_line ARGS2(
	char *,		line,
	EntryInfo *,	info)
{
    char *cp = line;
    char ct[26];
    unsigned long size;
    time_t secs;
    static time_t base; /* time() value on this OS in 1970 */
    static int flagbase = 0;

    if (!flagbase) {
	struct tm t;
	t.tm_year = 70; t.tm_mon = 0; t.tm_mday = 0;
	t.tm_hour = 0; t.tm_min = 0; t.tm_sec = 0;
	t.tm_isdst = -1;
	base = mktime(&t); /* could return -1 */
	flagbase = 1;
    }

    while (*cp) {
	switch(*cp) {
	    case '\t':
		StrAllocCopy(info->filename, cp + 1);
		return;
	    case 's':
		size = 0;
		while (*(++cp) && (*cp != ','))
		    size = (size * 10) + (*cp - '0');
		info->size = size;
		break;
	    case 'm':
		secs = 0;
		while (*(++cp) && (*cp != ','))
		    secs = (secs * 10) + (*cp - '0');
		secs += base; /* assumes that time_t is #seconds */
		strcpy(ct, ctime(&secs));
		ct[24] = 0;
		StrAllocCopy(info->date, ct);
		break;
	    case '/':
		StrAllocCopy(info->type, ENTRY_IS_DIRECTORY);
		/* FALLTHRU */
	    default:
		while (*cp) {
		    if (*cp++ == ',')
		      break;
		}
		break;
	}
    }
} /* parse_eplf_line */

/*
 * parse_ls_line() --
 *	Extract the name, size, and date from an ls -l line.
 */
PRIVATE void parse_ls_line ARGS2(
	char *,		line,
	EntryInfo *,	entry_info)
{
    int    i, j;
    int    base=1;
    int    size_num=0;

    for (i = strlen(line) - 1;
	 (i > 13) && (!isspace(UCH(line[i])) || !is_ls_date(&line[i-12])); i--)
	; /* null body */
    line[i] = '\0';
    if (i > 13) {
	StrAllocCopy(entry_info->date, &line[i-12]);
	/* replace the 4th location with nbsp if it is a space or zero */
	if (entry_info->date[4] == ' ' || entry_info->date[4] == '0')
	    entry_info->date[4] = HT_NON_BREAK_SPACE;
	/* make sure year or time is flush right */
	if (entry_info->date[11] == ' ') {
	    for (j = 11; j > 6; j--) {
		entry_info->date[j] = entry_info->date[j-1];
	    }
	}
    }
    j = i - 14;
    while (isdigit(UCH(line[j]))) {
	size_num += (line[j] - '0') * base;
	base *= 10;
	j--;
    }
    entry_info->size = size_num;
    StrAllocCopy(entry_info->filename, &line[i + 1]);
} /* parse_ls_line() */

/*
 * parse_dls_line() --
 *	Extract the name and size info and whether it refers to a
 *      directory from a LIST line in "dls" format.
 */
PRIVATE void parse_dls_line ARGS3(
	char *,		line,
	EntryInfo *,	entry_info,
	char **,	pspilledname)
{
    short  j;
    int    base=1;
    int    size_num=0;
    int    len;
    char *cps = NULL;

    /* README		   763	Information about this server\0
       bin/		     -	\0
       etc/		     =	\0
       ls-lR		     0	\0
       ls-lR.Z		     3	\0
       pub/		     =	Public area\0
       usr/		     -	\0
       morgan		    14	-> ../real/morgan\0
       TIMIT.mostlikely.Z\0
			 79215	\0
	*/

    len = strlen(line);
    if (len == 0) {
	FREE(*pspilledname);
	entry_info->display = FALSE;
	return;
    }
    cps = LYSkipNonBlanks(line);
    if (*cps == '\0') {		/* only a filename, save it and return. */
	StrAllocCopy(*pspilledname, line);
	entry_info->display = FALSE;
	return;
    }
    if (len < 24 || line[23] != ' ' ||
	(isspace(UCH(line[0])) && !*pspilledname)) {
	/* this isn't the expected "dls" format! */
	if (!isspace(UCH(line[0])))
	    *cps = '\0';
	if (*pspilledname && !*line) {
	    entry_info->filename = *pspilledname;
	    *pspilledname = NULL;
	    if (entry_info->filename[strlen(entry_info->filename)-1] == '/')
		StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
	    else
		StrAllocCopy(entry_info->type, "");
	} else {
	    StrAllocCopy(entry_info->filename, line);
	    if (cps && cps != line && *(cps-1) == '/')
		StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
	    else
		StrAllocCopy(entry_info->type, "");
	    FREE(*pspilledname);
	}
	return;
    }

    j = 22;
    if (line[j] == '=' || line[j] == '-') {
	StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
    } else {
	while (isdigit(UCH(line[j]))) {
	    size_num += (line[j] - '0') * base;
	    base *= 10;
	    j--;
	}
    }
    entry_info->size = size_num;

    cps = LYSkipBlanks(&line[23]);
    if (!strncmp(cps, "-> ", 3) && cps[3] != '\0' && cps[3] != ' ') {
	StrAllocCopy(entry_info->type, gettext("Symbolic Link"));
	entry_info->size = 0;	/* don't display size */
    }

    if (j > 0)
	line[j] = '\0';

    LYTrimTrailing(line);

    len = strlen(line);
    if (len == 0 && *pspilledname && **pspilledname) {
	line = *pspilledname;
	len = strlen(*pspilledname);
    }
    if (len > 0 && line[len-1] == '/') {
		/*
		**  It's a dir, remove / and mark it as such.
		*/
	if (len > 1)
	    line[len-1] = '\0';
	if (!entry_info->type)
	    StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
    }

    StrAllocCopy(entry_info->filename, line);
    FREE(*pspilledname);
} /* parse_dls_line() */

/*
 * parse_vms_dir_entry()
 *	Format the name, date, and size from a VMS LIST line
 *	into the EntryInfo structure - FM
 */
PRIVATE void parse_vms_dir_entry ARGS2(
	char *,		line,
	EntryInfo *,	entry_info)
{
    int i, j;
    unsigned int ialloc;
    char *cp, *cpd, *cps, date[16], *sp = " ";

    /**  Get rid of blank lines, and information lines.  **/
    /**  Valid lines have the ';' version number token.  **/
    if (!strlen(line) || (cp = strchr(line, ';')) == NULL) {
	entry_info->display = FALSE;
	return;
    }

    /** Cut out file or directory name at VMS version number. **/
    *cp++ ='\0';
    StrAllocCopy(entry_info->filename,line);

    /** Cast VMS non-README file and directory names to lowercase. **/
    if (strstr(entry_info->filename, "READ") == NULL) {
	LYLowerCase(entry_info->filename);
	i = strlen(entry_info->filename);
    } else {
	i = ((strstr(entry_info->filename, "READ") - entry_info->filename) + 4);
	if (!strncmp(&entry_info->filename[i], "ME", 2)) {
	    i += 2;
	    while (entry_info->filename[i] && entry_info->filename[i] != '.') {
		i++;
	    }
	} else if (!strncmp(&entry_info->filename[i], ".ME", 3)) {
	    i = strlen(entry_info->filename);
	} else {
	    i = 0;
	}
	LYLowerCase(entry_info->filename + i);
    }

    /** Uppercase terminal .z's or _z's. **/
    if ((--i > 2) &&
	entry_info->filename[i] == 'z' &&
	(entry_info->filename[i-1] == '.' ||
	 entry_info->filename[i-1] == '_'))
	entry_info->filename[i] = 'Z';

    /** Convert any tabs in rest of line to spaces. **/
    cps = cp-1;
    while ((cps=strchr(cps+1, '\t')) != NULL)
	*cps = ' ';

    /** Collapse serial spaces. **/
    i = 0; j = 1;
    cps = cp;
    while (cps[j] != '\0') {
	if (cps[i] == ' ' && cps[j] == ' ')
	    j++;
	else
	    cps[++i] = cps[j++];
    }
    cps[++i] = '\0';

    /* Set the years and date, if we don't have them yet. **/
    if (!HaveYears) {
	set_years_and_date();
    }

    /** Track down the date. **/
    if ((cpd=strchr(cp, '-')) != NULL &&
	strlen(cpd) > 9 && isdigit(UCH(*(cpd-1))) &&
	isalpha(UCH(*(cpd+1))) && *(cpd+4) == '-') {

	/** Month **/
	*(cpd+2) = (char) TOLOWER(*(cpd+2));
	*(cpd+3) = (char) TOLOWER(*(cpd+3));
	sprintf(date, "%.3s ", cpd+1);

	/** Day **/
	if (isdigit(UCH(*(cpd-2))))
	    sprintf(date+4, "%.2s ", cpd-2);
	else
	    sprintf(date+4, "%c%.1s ", HT_NON_BREAK_SPACE, cpd-1);

	/** Time or Year **/
	if (!strncmp(ThisYear, cpd+5, 4) &&
	    strlen(cpd) > 15 && *(cpd+12) == ':') {
	    sprintf(date+7, "%.5s", cpd+10);
	} else {
	    sprintf(date+7, " %.4s", cpd+5);
	}

	StrAllocCopy(entry_info->date, date);
    }

    /** Track down the size **/
    if ((cpd=strchr(cp, '/')) != NULL) {
	/* Appears be in used/allocated format */
	cps = cpd;
	while (isdigit(UCH(*(cps-1))))
	    cps--;
	if (cps < cpd)
	    *cpd = '\0';
	entry_info->size = atoi(cps);
	cps = cpd+1;
	while (isdigit(UCH(*cps)))
	    cps++;
	*cps = '\0';
	ialloc = atoi(cpd+1);
	/* Check if used is in blocks or bytes */
	if (entry_info->size <= ialloc)
	    entry_info->size *= 512;

    } else if ((cps=strtok(cp, sp)) != NULL) {
	/* We just initialized on the version number */
	/* Now let's hunt for a lone, size number    */
	while ((cps=strtok(NULL, sp)) != NULL) {
	    cpd = cps;
	    while (isdigit(UCH(*cpd)))
		cpd++;
	    if (*cpd == '\0') {
		/* Assume it's blocks */
		entry_info->size = atoi(cps) * 512;
		break;
	    }
	}
    }

    /** Wrap it up **/
    CTRACE((tfp, "HTFTP: VMS filename: %s  date: %s  size: %d\n",
		entry_info->filename,
		entry_info->date ? entry_info->date : "",
		entry_info->size));
    return;
} /* parse_vms_dir_entry() */

/*
 * parse_ms_windows_dir_entry() --
 *	Format the name, date, and size from an MS_WINDOWS LIST line into
 *	the EntryInfo structure (assumes Chameleon NEWT format). - FM
 */
PRIVATE void parse_ms_windows_dir_entry ARGS2(
	char *,		line,
	EntryInfo *,	entry_info)
{
    char *cp = line;
    char *cps, *cpd, date[16];
    char *end = line + strlen(line);

    /**  Get rid of blank or junk lines.  **/
    cp = LYSkipBlanks(cp);
    if (!(*cp)) {
	entry_info->display = FALSE;
	return;
    }

    /** Cut out file or directory name. **/
    cps = LYSkipNonBlanks(cp);
    *cps++ ='\0';
    cpd = cps;
    StrAllocCopy(entry_info->filename, cp);

    /** Track down the size **/
    if (cps < end) {
	cps = LYSkipBlanks(cps);
	cpd = LYSkipNonBlanks(cps);
	*cpd++ = '\0';
	if (isdigit(UCH(*cps))) {
	    entry_info->size = atoi(cps);
	} else {
	    StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
	}
    } else {
	StrAllocCopy(entry_info->type, "");
    }

    /* Set the years and date, if we don't have them yet. **/
    if (!HaveYears) {
	set_years_and_date();
    }

    /** Track down the date. **/
    if (cpd < end) {
	cpd = LYSkipBlanks(cpd);
	if (strlen(cpd) > 17) {
	    *(cpd+6)  = '\0';  /* Month and Day */
	    *(cpd+11) = '\0';  /* Year */
	    *(cpd+17) = '\0';  /* Time */
	    if (strcmp(ThisYear, cpd+7))
		/* Not this year, so show the year */
		sprintf(date, "%.6s  %.4s", cpd, (cpd+7));
	    else
		/* Is this year, so show the time */
		sprintf(date, "%.6s %.5s", cpd, (cpd+12));
	    StrAllocCopy(entry_info->date, date);
	    if (entry_info->date[4] == ' '|| entry_info->date[4] == '0') {
		entry_info->date[4] = HT_NON_BREAK_SPACE;
	    }
	}
    }

    /** Wrap it up **/
    CTRACE((tfp, "HTFTP: MS Windows filename: %s  date: %s  size: %d\n",
		entry_info->filename,
		entry_info->date ? entry_info->date : "",
		entry_info->size));
    return;
} /* parse_ms_windows_dir_entry */

/*
 * parse_windows_nt_dir_entry() --
 *	Format the name, date, and size from a WINDOWS_NT LIST line into
 *	the EntryInfo structure (assumes Chameleon NEWT format). - FM
 */
#ifdef NOTDEFINED
PRIVATE void parse_windows_nt_dir_entry ARGS2(
	char *,		line,
	EntryInfo *,	entry_info)
{
    char *cp = line;
    char *cps, *cpd, date[16];
    char *end = line + strlen(line);
    int i;

    /**  Get rid of blank or junk lines.  **/
    cp = LYSkipBlanks(cp);
    if (!(*cp)) {
	entry_info->display = FALSE;
	return;
    }

    /** Cut out file or directory name. **/
    cpd = cp;
    cps = LYSkipNonBlanks(end-1);
    cp = (cps+1);
    if (!strcmp(cp, ".") || !strcmp(cp, "..")) {
	entry_info->display = FALSE;
	return;
    }
    StrAllocCopy(entry_info->filename, cp);
    if (cps < cpd)
	return;
    *cp = '\0';
    end = cp;

    /* Set the years and date, if we don't have them yet. **/
    if (!HaveYears) {
	set_years_and_date();
    }

    /** Cut out the date. **/
    cp = cps = cpd;
    cps = LYSkipNonBlanks(cps);
    *cps++ ='\0';
    if (cps > end) {
	entry_info->display = FALSE;
	return;
    }
    cps = LYSkipBlanks(cps);
    cpd = LYSkipNonBlanks(cps);
    *cps++ ='\0';
    if (cps > end || cpd == cps || strlen(cpd) < 7) {
	entry_info->display = FALSE;
	return;
    }
    if (strlen(cp) == 8 &&
	isdigit(*cp) && isdigit(*(cp+1)) && *(cp+2) == '-' &&
	isdigit(*(cp+3)) && isdigit(*(cp+4)) && *(cp+5) == '-') {
	*(cp+2)  = '\0';	/* Month */
	i = atoi(cp) - 1;
	*(cp+5) = '\0';		/* Day */
	sprintf(date, "%.3s %.2s", months[i], (cp+3));
	if (date[4] == '0')
	    date[4] = ' ';
	cp += 6;			/* Year */
	if (strcmp((ThisYear+2), cp)) {
	    /* Not this year, so show the year */
	    if (atoi(cp) < 70) {
		sprintf(&date[6], "  20%.2s", cp);
	    } else {
		sprintf(&date[6], "  19%.2s", cp);
	    }
	} else {
	    /* Is this year, so show the time */
	    *(cpd+2) = '\0';	/* Hour */
	    i = atoi(cpd);
	    if (*(cpd+5) == 'P' || *(cpd+5) == 'p')
		i += 12;
	    sprintf(&date[6], " %02d:%.2s", i, (cpd+3));
	}
	StrAllocCopy(entry_info->date, date);
	if (entry_info->date[4] == ' '|| entry_info->date[4] == '0') {
	    entry_info->date[4] = HT_NON_BREAK_SPACE;
	}
    }

    /** Track down the size **/
    if (cps < end) {
	cps = LYSkipBlanks(cps);
	cpd = LYSkipNonBlanks(cps);
	*cpd = '\0';
	if (isdigit(*cps)) {
	    entry_info->size = atoi(cps);
	} else {
	    StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
	}
    } else {
	StrAllocCopy(entry_info->type, "");
    }

    /** Wrap it up **/
    CTRACE((tfp, "HTFTP: Windows NT filename: %s  date: %s  size: %d\n",
		entry_info->filename,
		entry_info->date ? entry_info->date : "",
		entry_info->size));
    return;
} /* parse_windows_nt_dir_entry */
#endif /* NOTDEFINED */

/*
 * parse_cms_dir_entry() --
 *	Format the name, date, and size from a VM/CMS line into
 *	the EntryInfo structure. - FM
 */
PRIVATE void parse_cms_dir_entry ARGS2(
	char *,		line,
	EntryInfo *,	entry_info)
{
    char *cp = line;
    char *cps, *cpd, date[16];
    char *end = line + strlen(line);
    int RecordLength = 0;
    int Records = 0;
    int i;

    /**  Get rid of blank or junk lines.  **/
    cp = LYSkipBlanks(cp);
    if (!(*cp)) {
	entry_info->display = FALSE;
	return;
    }

    /** Cut out file or directory name. **/
    cps = LYSkipNonBlanks(cp);
    *cps++ ='\0';
    StrAllocCopy(entry_info->filename, cp);
    if (strchr(entry_info->filename, '.') != NULL)
	/** If we already have a dot, we did an NLST. **/
	return;
    cp = LYSkipBlanks(cps);
    if (!(*cp)) {
	/** If we don't have more, we've misparsed. **/
	FREE(entry_info->filename);
	FREE(entry_info->type);
	entry_info->display = FALSE;
	return;
    }
    cps = LYSkipNonBlanks(cp);
    *cps++ ='\0';
    if ((0 == strcasecomp(cp, "DIR")) && (cp - line) > 17) {
	/** It's an SFS directory. **/
	StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
	entry_info->size = 0;
    } else {
	/** It's a file. **/
	cp--;
	*cp = '.';
	StrAllocCat(entry_info->filename, cp);

	/** Track down the VM/CMS RECFM or type. **/
	cp = cps;
	if (cp < end) {
	    cp = LYSkipBlanks(cp);
	    cps = LYSkipNonBlanks(cp);
	    *cps++ = '\0';
	    /** Check cp here, if it's relevant someday. **/
	}
    }

    /** Track down the record length or dash. **/
    cp = cps;
    if (cp < end) {
	cp = LYSkipBlanks(cp);
	cps = LYSkipNonBlanks(cp);
	*cps++ = '\0';
	if (isdigit(UCH(*cp))) {
	    RecordLength = atoi(cp);
	}
    }

    /** Track down the number of records or the dash. **/
    cp = cps;
    if (cps < end) {
	cp = LYSkipBlanks(cp);
	cps = LYSkipNonBlanks(cp);
	*cps++ = '\0';
	if (isdigit(UCH(*cp))) {
	    Records = atoi(cp);
	}
	if (Records > 0 && RecordLength > 0) {
	    /** Compute an approximate size. **/
	    entry_info->size = (Records * RecordLength);
	}
    }

    /** Set the years and date, if we don't have them yet. **/
    if (!HaveYears) {
	set_years_and_date();
    }

    /** Track down the date. **/
    cpd = cps;
    if (((cps < end) &&
	 (cps = strchr(cpd, ':')) != NULL) &&
	(cps < (end - 3) &&
	 isdigit(UCH(*(cps+1))) && isdigit(UCH(*(cps+2))) && *(cps+3) == ':')) {
	cps += 3;
	*cps = '\0';
	if ((cps - cpd) >= 14) {
	    cpd = (cps - 14);
	    *(cpd+2) = '\0';	/* Month */
	    *(cpd+5) = '\0';	/* Day */
	    *(cpd+8) = '\0';	/* Year */
	    cps -= 5;		/* Time */
	    if (*cpd == ' ')
		*cpd = '0';
	    i = atoi(cpd) - 1;
	    sprintf(date, "%.3s %.2s", months[i], (cpd+3));
	    if (date[4] == '0')
		date[4] = ' ';
	    cpd += 6;		/* Year */
	    if (strcmp((ThisYear+2), cpd)) {
		/* Not this year, so show the year. */
		if (atoi(cpd) < 70) {
		    sprintf(&date[6], "  20%.2s", cpd);
		} else {
		    sprintf(&date[6], "  19%.2s", cpd);
		}
	    } else {
		/* Is this year, so show the time. */
		*(cps+2) = '\0';	/* Hour */
		i = atoi(cps);
		sprintf(&date[6], " %02d:%.2s", i, (cps+3));
	    }
	    StrAllocCopy(entry_info->date, date);
	    if (entry_info->date[4] == ' '|| entry_info->date[4] == '0') {
		entry_info->date[4] = HT_NON_BREAK_SPACE;
	    }
	}
    }

    /** Wrap it up. **/
    CTRACE((tfp, "HTFTP: VM/CMS filename: %s  date: %s  size: %d\n",
		entry_info->filename,
		entry_info->date ? entry_info->date : "",
		entry_info->size));
    return;
} /* parse_cms_dir_entry */

/*
 *     parse_dir_entry()
 *	Given a line of LIST/NLST output in entry, return results
 *	and a file/dir name in entry_info struct
 *
 *	If first is true, this is the first name in a directory.
 */

PRIVATE EntryInfo * parse_dir_entry ARGS3(
	char *,		entry,
	BOOLEAN *,	first,
	char **,	pspilledname)
{
    EntryInfo *entry_info;
    int  i;
    int  len;
    BOOLEAN remove_size=FALSE;
    char *cp;

    entry_info = (EntryInfo *)malloc(sizeof(EntryInfo));
    if (entry_info == NULL)
	outofmem(__FILE__, "parse_dir_entry");
    entry_info->filename = NULL;
    entry_info->type = NULL;
    entry_info->date = NULL;
    entry_info->size = 0;
    entry_info->display = TRUE;

    switch (server_type) {
	case DLS_SERVER:

	    /*
	    **	Interpret and edit LIST output from a Unix server
	    **  in "dls" format.
	    **  This one must have claimed to be Unix in order to
	    **  get here; if the first line looks fishy, we revert
	    **  to Unix and hope that fits better (this recovery is
	    **  untested). - kw
	    */

	    if (*first) {
		len = strlen(entry);
		if (!len || entry[0] == ' ' ||
		    (len >= 24 && entry[23] != ' ') ||
		    (len < 24 && strchr(entry, ' '))) {
		    server_type = UNIX_SERVER;
		    CTRACE((tfp,
			   "HTFTP: Falling back to treating as Unix server.\n"));
		} else {
		    *first = FALSE;
		}
	    }

	    if (server_type == DLS_SERVER) {
		/* if still unchanged... */
		parse_dls_line(entry, entry_info, pspilledname);

		if (!entry_info->filename || *entry_info->filename == '\0') {
		    entry_info->display = FALSE;
		    return(entry_info);
		}
		if (!strcmp(entry_info->filename,"..") ||
		    !strcmp(entry_info->filename,"."))
		    entry_info->display = FALSE;
		if (entry_info->type && *entry_info->type == '\0') {
		    FREE(entry_info->type);
		    return(entry_info);
		}
		/*
		**	Goto the bottom and get real type.
		*/
		break;
	    } /* fall through if server_type changed for *first == TRUE ! */

	case UNIX_SERVER:
	case PETER_LEWIS_SERVER:
	case MACHTEN_SERVER:
	case MSDOS_SERVER:
	case WINDOWS_NT_SERVER:
	case WINDOWS_2K_SERVER:
	case APPLESHARE_SERVER:
	case NETPRESENZ_SERVER:
	    /*
	    **	Check for EPLF output (local times).
	    */
	    if (*entry == '+') {
		parse_eplf_line(entry, entry_info);
		break;
	    }

	    /*
	    **	Interpret and edit LIST output from Unix server.
	    */
	    len = strlen(entry);
	    if (*first) {
		/* don't gettext() this -- incoming text: */
		if (!strcmp(entry, "can not access directory .")) {
		    /*
		     *	Don't reset *first, nothing real will follow. - KW
		     */
		    entry_info->display = FALSE;
		    return(entry_info);
		}
		*first = FALSE;
		if (!strncmp(entry, "total ", 6) ||
		    strstr(entry, "not available") != NULL) {
		    entry_info->display = FALSE;
		    return(entry_info);
		} else if (unsure_type) {
		    /* this isn't really a unix server! */
		    server_type = GENERIC_SERVER;
		    entry_info->display = FALSE;
		    return(entry_info);
		}
	    }

	    /*
	    **	Check first character of ls -l output.
	    */
	    if (TOUPPER(entry[0]) == 'D')  {
		/*
		**  It's a directory.
		*/
		StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
		remove_size=TRUE; /* size is not useful */
	    } else if (entry[0] == 'l') {
		/*
		**  It's a symbolic link, does the user care about
		**  knowing if it is symbolic?	I think so since
		**  it might be a directory.
		*/
		StrAllocCopy(entry_info->type, gettext("Symbolic Link"));
		remove_size=TRUE; /* size is not useful */

		/*
		**  Strip off " -> pathname".
		*/
		for (i = len - 1; (i > 3) &&
				  (!isspace(UCH(entry[i])) ||
				   (entry[i-1] != '>')	||
				   (entry[i-2] != '-') ||
				   (entry[i-3] != ' ')); i--)
		    ; /* null body */
		if (i > 3) {
		    entry[i-3] = '\0';
		    len = i - 3;
		}
	    } /* link */

	    parse_ls_line(entry, entry_info);

	    if (!strcmp(entry_info->filename,"..") ||
		!strcmp(entry_info->filename,"."))
		entry_info->display = FALSE;
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;

	case VMS_SERVER:
	    /*
	    **	Interpret and edit LIST output from VMS server
	    **	and convert information lines to zero length.
	    */
	    parse_vms_dir_entry(entry, entry_info);

	    /*
	    **	Get rid of any junk lines.
	    */
	    if (!entry_info->display)
		return(entry_info);

	    /*
	    **	Trim off VMS directory extensions.
	    */
	    len = strlen(entry_info->filename);
	    if ((len > 4) && !strcmp(&entry_info->filename[len-4], ".dir")) {
		entry_info->filename[len-4] = '\0';
		StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
		remove_size=TRUE; /* size is not useful */
	    }
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;

	case MS_WINDOWS_SERVER:
	    /*
	    **	Interpret and edit LIST output from MS_WINDOWS server
	    **	and convert information lines to zero length.
	    */
	    parse_ms_windows_dir_entry(entry, entry_info);

	    /*
	    **	Get rid of any junk lines.
	    */
	    if (!entry_info->display)
		return(entry_info);
	    if (entry_info->type && *entry_info->type == '\0') {
		FREE(entry_info->type);
		return(entry_info);
	    }
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;

#ifdef NOTDEFINED
	case WINDOWS_NT_SERVER:
	    /*
	    **	Interpret and edit LIST output from MS_WINDOWS server
	    **	and convert information lines to zero length.
	    */
	    parse_windows_nt_dir_entry(entry, entry_info);

	    /*
	    **	Get rid of any junk lines.
	    */
	    if (!entry_info->display)
		return(entry_info);
	    if (entry_info->type && *entry_info->type == '\0') {
		FREE(entry_info->type);
		return(entry_info);
	    }
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;
#endif /* NOTDEFINED */

	case CMS_SERVER:
	  {
	    /*
	    **	Interpret and edit LIST output from VM/CMS server
	    **	and convert any information lines to zero length.
	    */
	    parse_cms_dir_entry(entry, entry_info);

	    /*
	    **	Get rid of any junk lines.
	    */
	    if (!entry_info->display)
		return(entry_info);
	    if (entry_info->type && *entry_info->type == '\0') {
		FREE(entry_info->type);
		return(entry_info);
	    }
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;
	  }

	case NCSA_SERVER:
	case TCPC_SERVER:
	    /*
	    **	Directories identified by trailing "/" characters.
	    */
	    StrAllocCopy(entry_info->filename, entry);
	    len = strlen(entry);
	    if (entry[len-1] == '/') {
		/*
		**  It's a dir, remove / and mark it as such.
		*/
		entry[len-1] = '\0';
		StrAllocCopy(entry_info->type, ENTRY_IS_DIRECTORY);
		remove_size=TRUE; /* size is not useful */
	    }
	    /*
	    **	Goto the bottom and get real type.
	    */
	    break;

	default:
	    /*
	    **	We can't tell if it is a directory since we only
	    **	did an NLST :(	List bad file types anyways?  NOT!
	    */
	    StrAllocCopy(entry_info->filename, entry);
	    return(entry_info); /* mostly empty info */

    } /* switch (server_type) */

    if (remove_size && entry_info->size) {
	entry_info->size = 0;
    }

    if (entry_info->filename && strlen(entry_info->filename) > 3) {
	if (((cp=strrchr(entry_info->filename, '.')) != NULL &&
	     0 == strncasecomp(cp, ".me", 3)) &&
	    (cp[3] == '\0' || cp[3] == ';')) {
	    /*
	    **	Don't treat this as application/x-Troff-me
	    **	if it's a Unix server but has the string
	    **	"read.me", or if it's not a Unix server. - FM
	    */
	    if ((server_type != UNIX_SERVER) ||
		(cp > (entry_info->filename + 3) &&
		 0 == strncasecomp((cp - 4), "read.me", 7))) {
		StrAllocCopy(entry_info->type, "text/plain");
	    }
	}
    }

    /*
    **	Get real types eventually.
    */
    if (!entry_info->type) {
	CONST char *cp2;
	HTFormat format;
	HTAtom * encoding;  /* @@ not used at all */
	format = HTFileFormat(entry_info->filename, &encoding, &cp2);

	if (cp2 == NULL) {
	    if (!strncmp(HTAtom_name(format), "application",11)) {
		cp2 = HTAtom_name(format) + 12;
		if (!strncmp(cp2,"x-",2))
		    cp2 += 2;
	    } else {
		cp2 = HTAtom_name(format);
	    }
	}

	StrAllocCopy(entry_info->type, cp2);
    }

    return(entry_info);
} /* parse_dir_entry */

PRIVATE int compare_EntryInfo_structs ARGS2(
	EntryInfo *,	entry1,
	EntryInfo *,	entry2)
{
    int i, status;
    char date1[16], date2[16], time1[8], time2[8], month[4];

    switch(HTfileSortMethod) {
	case FILE_BY_SIZE:
	    /* both equal or both 0 */
	    if (entry1->size == entry2->size)
		return(strcmp(entry1->filename, entry2->filename));
	    else
		if (entry1->size > entry2->size)
		    return(1);
		else
		    return(-1);

	case FILE_BY_TYPE:
	    if (entry1->type && entry2->type) {
		status = strcasecomp(entry1->type, entry2->type);
		if (status)
		    return(status);
		/* else fall to filename comparison */
	    }
	    return (strcmp(entry1->filename, entry2->filename));

	case FILE_BY_DATE:
	    if (entry1->date && entry2->date) {
		/*
		** Make sure we have the correct length. - FM
		*/
		if (strlen(entry1->date) != 12 || strlen(entry2->date) != 12) {
		    return(strcmp(entry1->filename, entry2->filename));
		}
		/*
		** Set the years and date,
		** if we don't have them yet.
		*/
		if (!HaveYears) {
		    set_years_and_date();
		}
		/*
		** Set up for sorting in reverse
		** chronological order. - FM
		*/
		if (entry1->date[9] == ':') {
		    strcpy(date1, "9999");
		    strcpy(time1, &entry1->date[7]);
		    if (time1[0] == ' ') {
			 time1[0] = '0';
		    }
		} else {
		    strcpy(date1, &entry1->date[8]);
		    strcpy(time1, "00:00");
		}
		strncpy(month, entry1->date, 3);
		month[3] = '\0';
		for (i = 0; i < 12; i++) {
		    if (!strcasecomp(month, months[i])) {
			break;
		    }
		}
		i++;
		sprintf(month, "%02d", i);
		strcat(date1, month);
		strncat(date1, &entry1->date[4], 2);
		date1[8] = '\0';
		if (date1[6] == ' ' || date1[6] == HT_NON_BREAK_SPACE) {
		    date1[6] = '0';
		}
		/*  If no year given, assume last year if it would otherwise
		 *  be in the future by more than one day.  The one day
		 *  tolerance is to account for a possible timezone
		 *  difference. - kw */
		if (date1[0] == '9' && atoi(date1) > TheDate + 1) {
		    for (i = 0; i < 4; i++) {
			date1[i] = LastYear[i];
		    }
		}
		strcat(date1, time1);
		if (entry2->date[9] == ':') {
		    strcpy(date2, "9999");
		    strcpy(time2, &entry2->date[7]);
		    if (time2[0] == ' ') {
			time2[0] = '0';
		    }
		} else {
		    strcpy(date2, &entry2->date[8]);
		    strcpy(time2, "00:00");
		}
		strncpy(month, entry2->date, 3);
		month[3] = '\0';
		for (i = 0; i < 12; i++) {
		    if (!strcasecomp(month, months[i])) {
			break;
		    }
		}
		i++;
		sprintf(month, "%02d", i);
		strcat(date2, month);
		strncat(date2, &entry2->date[4], 2);
		date2[8] = '\0';
		if (date2[6] == ' ' || date2[6] == HT_NON_BREAK_SPACE) {
		    date2[6] = '0';
		}
		/*  If no year given, assume last year if it would otherwise
		 *  be in the future by more than one day.  The one day
		 *  tolerance is to account for a possible timezone
		 *  difference. - kw */
		if (date2[0] == '9' && atoi(date2) > TheDate + 1) {
		    for (i = 0; i < 4; i++) {
			date2[i] = LastYear[i];
		    }
		}
		strcat(date2, time2);
		/*
		** Do the comparison. - FM
		*/
		status = strcasecomp(date2, date1);
		if (status)
		    return(status);
		/* else fall to filename comparison */
	    }
	    return (strcmp(entry1->filename, entry2->filename));

	case FILE_BY_NAME:
	default:
	    return (strcmp(entry1->filename, entry2->filename));
    }
}


/*	Read a directory into an hypertext object from the data socket
**	--------------------------------------------------------------
**
** On entry,
**	anchor		Parent anchor to link the this node to
**	address		Address of the directory
** On exit,
**	returns		HT_LOADED if OK
**			<0 if error.
*/
PRIVATE int read_directory ARGS4(
	HTParentAnchor *,	parent,
	CONST char *,		address,
	HTFormat,		format_out,
	HTStream *,		sink)
{
    int status;
    BOOLEAN WasInterrupted = FALSE;
    HTStructured* target = HTML_new(parent, format_out, sink);
    HTStructuredClass targetClass;
    char *filename = HTParse(address, "", PARSE_PATH + PARSE_PUNCTUATION);
    EntryInfo *entry_info;
    BOOLEAN first = TRUE;
    char string_buffer[64];
    char *lastpath = NULL;/* prefix for link, either "" (for root) or xxx  */
    BOOL need_parent_link = FALSE;
    BOOL tildeIsTop = FALSE;

    targetClass = *(target->isa);

    _HTProgress (gettext("Receiving FTP directory."));

    /*
     *  Force the current Date and Year (TheDate, ThisYear, and LastYear)
     *  to be recalculated for each directory request.  Otherwise we have
     *  a problem with long-running sessions assuming the wrong date for
     *  today. - kw
     */
    HaveYears = FALSE;
    /*
    **	Check whether we always want the home
    **	directory treated as Welcome. - FM
    */
    if (server_type == VMS_SERVER)
	tildeIsTop = TRUE;

    /*
    **	This should always come back FALSE, since the
    **	flag is set only for local directory listings
    **	if LONG_LIST was defined on compilation, but
    **	we could someday set up an equivalent listing
    **	for Unix ftp servers. - FM
    */
    need_parent_link = HTDirTitles(target, (HTAnchor*)parent, tildeIsTop);

    data_read_pointer = data_write_pointer = data_buffer;

    if (*filename == '\0') {		  /* Empty filename: use root. */
	StrAllocCopy (lastpath, "/");
    } else if (!strcmp(filename,"/")) {   /* Root path. */
	StrAllocCopy (lastpath, "/foo/..");
    } else {
	char * p = strrchr(filename, '/');	     /* Find the lastslash. */
	char *cp;

	if (server_type == CMS_SERVER) {
	    StrAllocCopy(lastpath, filename); /* Use absolute path for CMS. */
	} else {
	    StrAllocCopy(lastpath, p+1);   /* Take slash off the beginning. */
	}
	if ((cp = strrchr(lastpath, ';')) != NULL) {   /* Trim type= param. */
	    if (!strncasecomp((cp+1), "type=", 5)) {
		if (TOUPPER(*(cp+6)) == 'D' ||
		    TOUPPER(*(cp+6)) == 'A' ||
		    TOUPPER(*(cp+6)) == 'I')
		    *cp = '\0';
	    }
	}
    }
    FREE (filename);


    {
	HTBTree * bt = HTBTree_new((HTComparer)compare_EntryInfo_structs);
	int ic;
	HTChunk * chunk = HTChunkCreate(128);
	int BytesReceived = 0;
	int BytesReported = 0;
	char NumBytes[64];
	char *spilledname = NULL;
	PUTC('\n');  /* prettier LJM */
	for (ic = 0; ic != EOF;) {	/* For each entry in the directory */
	    HTChunkClear(chunk);

	    if (HTCheckForInterrupt()) {
		CTRACE((tfp,
		       "read_directory: interrupted after %d bytes\n",
		       BytesReceived));
		WasInterrupted = TRUE;
		if (BytesReceived) {
		    goto unload_btree;	/* unload btree */
		} else {
		    ABORT_TARGET;
		    HTBTreeAndObject_free(bt);
		    FREE(spilledname);
		    return HT_INTERRUPTED;
		}
	    }

	    /*	 read directory entry
	     */
	    interrupted_in_next_data_char = FALSE;
	    for (;;) {			/* Read in one line as filename */
		ic = NEXT_DATA_CHAR;
AgainForMultiNet:
		if (interrupted_in_next_data_char) {
		    CTRACE((tfp,
			   "read_directory: interrupted_in_next_data_char after %d bytes\n",
			   BytesReceived));
		    WasInterrupted = TRUE;
		    if (BytesReceived) {
			goto unload_btree;  /* unload btree */
		    } else {
			ABORT_TARGET;
			HTBTreeAndObject_free(bt);
			FREE(spilledname);
			return HT_INTERRUPTED;
		    }
		} else if ((char)ic == CR || (char)ic == LF) {    /* Terminator? */
		    if (chunk->size != 0) {  /* got some text */
			/* Deal with MultiNet's wrapping of long lines */
			if (server_type == VMS_SERVER) {
			/* Deal with MultiNet's wrapping of long lines - F.M. */
			    if (data_read_pointer < data_write_pointer &&
				*(data_read_pointer+1) == ' ')
				data_read_pointer++;
			    else if (data_read_pointer >= data_write_pointer) {
				status = NETREAD(data_soc, data_buffer,
						 DATA_BUFFER_SIZE);
				if (status == HT_INTERRUPTED) {
				    interrupted_in_next_data_char = 1;
				    goto AgainForMultiNet;
				}
				if (status <= 0) {
				    ic = EOF;
				    break;
				}
				data_write_pointer = data_buffer + status;
				data_read_pointer = data_buffer;
				if (*data_read_pointer == ' ')
				    data_read_pointer++;
				else
				    break;
			    }
			    else
				break;
			}
			else
			    break;	/* finish getting one entry */
		    }
		} else if (ic == EOF) {
		    break;		/* End of file */
		} else {
		    HTChunkPutc(chunk, (char)ic);
		}
	    }
	    HTChunkTerminate(chunk);

	    BytesReceived += chunk->size;
	    if (BytesReceived > BytesReported + 1024) {
#ifdef _WINDOWS
		extern int ws_read_per_sec;

		sprintf(NumBytes,gettext("Transferred %d bytes (%5d)"),
				BytesReceived, ws_read_per_sec);
#else
		sprintf(NumBytes, TRANSFERRED_X_BYTES, BytesReceived);
#endif
		HTProgress(NumBytes);
		BytesReported = BytesReceived;
	    }

	    if (ic == EOF && chunk->size == 1)
	    /* 1 means empty: includes terminating 0 */
		break;
	    CTRACE((tfp, "HTFTP: Line in %s is %s\n",
			lastpath, chunk->data));

	    entry_info = parse_dir_entry(chunk->data, &first, &spilledname);
	    if (entry_info->display) {
		FREE(spilledname);
		CTRACE((tfp, "Adding file to BTree: %s\n",
			    entry_info->filename));
		HTBTree_add(bt, (EntryInfo *)entry_info);
	    } else {
		free_entryinfo_struct_contents(entry_info);
		FREE(entry_info);
	    }

	}  /* next entry */

unload_btree:

	HTChunkFree(chunk);
	FREE(spilledname);

	/* print out the handy help message if it exits :) */
	if (help_message_cache_non_empty()) {
	    START(HTML_PRE);
	    START(HTML_HR);
	    PUTC('\n');
	    PUTS(help_message_cache_contents());
	    init_help_message_cache();	/* to free memory */
	    START(HTML_HR);
	    PUTC('\n');
	} else {
	    START(HTML_PRE);
	    PUTC('\n');
	}

	/* Put up header
	 */
	/* PUTS("    Date	 Type		  Size	   Filename\n");
	 */

	/* Run through tree printing out in order
	 */
	{
#ifdef SH_EX	/* 1997/10/18 (Sat) 14:14:28 */
	    char *p, name_buff[256];
	    int  name_len, dot_len;

#define	FNAME_WIDTH	30
#define	FILE_GAP	2

#endif
	    HTBTElement * ele;
	    int i;
	    for (ele = HTBTree_next(bt, NULL);
		 ele != NULL;
		 ele = HTBTree_next(bt, ele)) {
		entry_info = (EntryInfo *)HTBTree_object(ele);

		if (entry_info->date) {
		    PUTS(entry_info->date);
		    PUTS("  ");
		} else {
		    PUTS("     * ");
		}

		if (entry_info->type) {
		    for (i = 0; entry_info->type[i] != '\0' && i < 16; i++)
			PUTC(entry_info->type[i]);
		    for (; i < 17; i++)
			PUTC(' ');
		}

		/* start the anchor */
		HTDirEntry(target, lastpath, entry_info->filename);
#ifdef SH_EX	/* 1997/10/18 (Sat) 16:00 */
		name_len = strlen(entry_info->filename);

		sprintf(name_buff, "%-30s", entry_info->filename);

		if (name_len < FNAME_WIDTH) {
		    dot_len = FNAME_WIDTH - FILE_GAP - name_len;
		    if (dot_len > 0) {
			p = name_buff + name_len + 1;
			while (dot_len--)
			    *p++ = '.';
		    }
		} else {
		    name_buff[FNAME_WIDTH] = '\0';
		}

		PUTS(name_buff);
#else
		PUTS(entry_info->filename);
#endif
		END(HTML_A);

		if (entry_info->size) {
#ifdef SH_EX	/* 1998/02/02 (Mon) 16:34:52 */
		    if (entry_info->size < 1024)
			sprintf(string_buffer, "%6d bytes",
					       entry_info->size);
		    else
			sprintf(string_buffer, "%6d Kb",
						entry_info->size/1024);
#else
		    if (entry_info->size < 1024)
			sprintf(string_buffer, "  %d bytes",
					       entry_info->size);
		    else
			sprintf(string_buffer, "  %dKb",
						entry_info->size/1024);
#endif
		    PUTS(string_buffer);
		}

		PUTC('\n'); /* end of this entry */

		free_entryinfo_struct_contents(entry_info);
	    }
	}
	END(HTML_PRE);
	FREE_TARGET;
	HTBTreeAndObject_free(bt);
    }

    FREE(lastpath);

    if (WasInterrupted || data_soc != -1) { /* should always be true */
	/*
	 *  Without closing the data socket first,
	 *  the response(0) later may hang.
	 *  Some servers expect the client to fin/ack the close
	 *  of the data connection before proceeding with the
	 *  conversation on the control connection. - kw
	 */
	CTRACE((tfp, "HTFTP: Closing data socket %d\n", data_soc));
	status = NETCLOSE(data_soc);
	if (status == -1)
	    HTInetStatus("close");	/* Comment only */
	data_soc = -1;
    }

    if (WasInterrupted || HTCheckForInterrupt()) {
	_HTProgress(TRANSFER_INTERRUPTED);
    }
    return HT_LOADED;
}

/*	Retrieve File from Server
**	-------------------------
**
** On entry,
**	name		WWW address of a file: document, including hostname
** On exit,
**	returns		Socket number for file if good.
**			<0 if bad.
*/
PUBLIC int HTFTPLoad ARGS4(
	CONST char *,		name,
	HTParentAnchor *,	anchor,
	HTFormat,		format_out,
	HTStream *,		sink)
{
    BOOL isDirectory = NO;
    HTAtom * encoding = NULL;
    int status, final_status;
    int retry;			/* How many times tried? */
    int outstanding = 1;	/* outstanding control connection responses
				   that we are willing to wait for, if we
				   get to the point of reading data - kw */
    HTFormat format;

    /* set use_list to NOT since we don't know what kind of server
     * this is yet.  And set the type to GENERIC
     */
    use_list = FALSE;
    server_type = GENERIC_SERVER;
    HTReadProgress(0,0);

    for (retry = 0; retry < 2; retry++) { /* For timed out/broken connections */
	status = get_connection(name, anchor);
	if (status < 0)
	    return status;

	if (!ftp_passive) {
	    status = get_listen_socket();
	    if (status < 0) {
		NETCLOSE (control->socket);
		control->socket = -1;
#ifdef INET6
		if (master_socket >= 0)
		    (void)close_master_socket ();
#else
		close_master_socket ();
#endif /* INET6 */
		/* HT_INTERRUPTED would fall through, if we could interrupt
		   somehow in the middle of it, which we currently can't. */
		return status;
	    }

#ifdef REPEAT_PORT
	    /*	Inform the server of the port number we will listen on
	    */
	    status = response(port_command);
	    if (status == HT_INTERRUPTED) {
		CTRACE((tfp, "HTFTP: Interrupted in response (port_command)\n"));
		_HTProgress (CONNECTION_INTERRUPTED);
		NETCLOSE (control->socket);
		control->socket = -1;
		close_master_socket ();
		return HT_INTERRUPTED;
	    }
	    if (status != 2) {		/* Could have timed out */
		if (status < 0)
		    continue;		/* try again - net error*/
		return -status;		/* bad reply */
	    }
	    CTRACE((tfp, "HTFTP: Port defined.\n"));
#endif /* REPEAT_PORT */
	} else {		/* Tell the server to be passive */
	    char *command = NULL;
	    char *p;
	    int h0, h1, h2, h3, p0, p1;	/* Parts of reply */
#ifdef INET6
	    char dst[LINE_LENGTH+1];
#endif

	    data_soc = status;

#ifdef INET6
	    status = send_cmd_1(p = "EPSV");
	    if (status < 0)	/* retry or Bad return */
		continue;
	    else if (status != 2) {
		status = send_cmd_1(p = "PASV");
		if (status < 0)	/* retry or Bad return */
		    continue;
		else if (status != 2) {
		    return -status;	/* bad reply */
		}
	    }

	    if (strcmp(p, "PASV") == 0) {
		for (p = response_text; *p && *p != ','; p++)
		    ; /* null body */

		while (--p > response_text && '0' <= *p && *p <= '9')
		    ; /* null body */
		status = sscanf(p+1, "%d,%d,%d,%d,%d,%d",
		       &h0, &h1, &h2, &h3, &p0, &p1);
		if (status < 4) {
		    fprintf(tfp, "HTFTP: PASV reply has no inet address!\n");
		    return -99;
		}
		passive_port = (p0<<8) + p1;
		snprintf(dst, sizeof(dst), "%d.%d.%d.%d", h0, h1, h2, h3);
	    } else if (strcmp(p, "EPSV") == 0) {
		unsigned char c0, c1, c2, c3;
		struct sockaddr_storage ss;
		int sslen;

		/*
		 * EPSV bla (|||port|)
		 */
		for (p = response_text; *p && !isspace(*p); p++)
		    ; /* null body */
		for (/*nothing*/; *p && *p && *p != '('; p++)	/*)*/
		    ; /* null body */
		status = sscanf(p, "(%c%c%c%d%c)", &c0, &c1, &c2, &p0, &c3);
		if (status != 5) {
		    fprintf(tfp, "HTFTP: EPSV reply has invalid format!\n");
		    return -99;
		}
		passive_port = p0;

		sslen = sizeof(ss);
		if (getpeername(control->socket, (struct sockaddr *)&ss,
		    &sslen) < 0) {
		    fprintf(tfp, "HTFTP: getpeername(control) failed\n");
		    return -99;
		}
		if (getnameinfo((struct sockaddr *)&ss, sslen, dst,
		    sizeof(dst), NULL, 0, NI_NUMERICHOST)) {
		    fprintf(tfp, "HTFTP: getnameinfo failed\n");
		    return -99;
		}
	    }
#else
	    status = send_cmd_1("PASV");
	    if (status != 2) {
		if (status < 0)
		    continue;		/* retry or Bad return */
		return -status;		/* bad reply */
	    }
	    for (p = response_text; *p && *p != ','; p++)
		; /* null body */

	    while (--p > response_text && '0' <= *p && *p <= '9')
		; /* null body */

	    status = sscanf(p+1, "%d,%d,%d,%d,%d,%d",
			    &h0, &h1, &h2, &h3, &p0, &p1);
	    if (status < 4) {
		fprintf(tfp, "HTFTP: PASV reply has no inet address!\n");
		return -99;
	    }
	    passive_port = (PortNumber)((p0<<8) + p1);
#endif /* INET6 */
	    CTRACE((tfp, "HTFTP: Server is listening on port %d\n",
			 passive_port));

	    /* Open connection for data:  */

#ifdef INET6
	    HTSprintf0(&command, "ftp://%s:%d/", dst, passive_port);
#else
	    HTSprintf0(&command, "ftp://%d.%d.%d.%d:%d/",
		    h0, h1, h2, h3, passive_port);
#endif
	    status = HTDoConnect(command, "FTP data", passive_port, &data_soc);
	    FREE(command);

	    if (status < 0) {
		(void) HTInetStatus(gettext("connect for data"));
		NETCLOSE(data_soc);
		return status;			/* Bad return */
	    }

	    CTRACE((tfp, "FTP data connected, socket %d\n", data_soc));
	}
	status = 0;
	break;	/* No more retries */

    } /* for retries */
    if (status < 0)
	return status;		/* Failed with this code */

/*	Ask for the file:
*/
    {
	char *filename = HTParse(name, "", PARSE_PATH + PARSE_PUNCTUATION);
	char *fname = filename; /** Save for subsequent free() **/
	char *vmsname = NULL;
	BOOL binary;
	char *type = NULL;
	char *cp;

	if (server_type == CMS_SERVER) {
	    /** If the unescaped path has a %2f, reject it as illegal. - FM **/
	    if (((cp = strstr(filename, "%2")) != NULL) &&
		TOUPPER(cp[2]) == 'F') {
		FREE(fname);
		init_help_message_cache();  /* to free memory */
		NETCLOSE(control->socket);
		control->socket = -1;
		CTRACE((tfp, "HTFTP: Rejecting path due to illegal escaped slash.\n"));
		return -1;
	    }
	}

	if (!*filename) {
	    StrAllocCopy(filename, "/");
	    type = "D";
	} else if ((type = strrchr(filename, ';')) != NULL) {
	    /*
	    **	Check and trim the type= parameter. - FM
	    */
	    if (!strncasecomp((type+1), "type=", 5)) {
		switch(TOUPPER(*(type+6))) {
		case 'D':
		    *type = '\0';
		    type = "D";
		    break;
		case 'A':
		    *type = '\0';
		    type = "A";
		    break;
		case 'I':
		    *type = '\0';
		    type = "I";
		    break;
		default:
		    type = "";
		    break;
		}
		if (!*filename) {
		    *filename = '/';
		    *(filename+1) = '\0';
		}
	    }
	    if (*type != '\0') {
		CTRACE((tfp, "HTFTP: type=%s\n", type));
	    }
	}
	HTUnEscape(filename);
	CTRACE((tfp, "HTFTP: UnEscaped %s\n", filename));
	if (filename[1] == '~') {
	    /*
	    ** Check if translation of HOME as tilde is supported,
	    ** and adjust filename if so. - FM
	    */
	    char *cp2 = NULL;
	    char *fn = NULL;

	    if ((cp2 = strchr((filename+1), '/')) != NULL) {
		*cp2 = '\0';
	    }
	    status = send_cmd_1("PWD");
	    if (status == 2 && response_text[5] == '/') {
		status = send_cwd(filename+1);
		if (status == 2) {
		    StrAllocCopy(fn, (filename+1));
		    if (cp2) {
			*cp2 = '/';
			if (fn[strlen(fn)-1] != '/') {
			    StrAllocCat(fn, cp2);
			} else {
			    StrAllocCat(fn, (cp2+1));
			}
			cp2 = NULL;
		    }
		    FREE(fname);
		    fname = filename = fn;
		}
	    }
	    if (cp2) {
		*cp2 = '/';
	    }
	}
	if (strlen(filename) > 3) {
	    char *cp2;
	    if (((cp2=strrchr(filename, '.')) != NULL &&
		 0 == strncasecomp(cp2, ".me", 3)) &&
		(cp2[3] == '\0' || cp2[3] == ';')) {
		/*
		**  Don't treat this as application/x-Troff-me
		**  if it's a Unix server but has the string
		**  "read.me", or if it's not a Unix server. - FM
		*/
		if ((server_type != UNIX_SERVER) ||
		    (cp2 > (filename + 3) &&
		     0 == strncasecomp((cp2 - 4), "read.me", 7))) {
		    *cp2 = '\0';
		    format = HTFileFormat(filename, &encoding, NULL);
		    *cp2 = '.';
		} else {
		    format = HTFileFormat(filename, &encoding, NULL);
		}
	    } else {
		format = HTFileFormat(filename, &encoding, NULL);
	    }
	} else {
	    format = HTFileFormat(filename, &encoding, NULL);
	}
	format = HTCharsetFormat(format, anchor, -1);
	binary = (BOOL) (encoding != HTAtom_for("8bit") &&
		  encoding != HTAtom_for("7bit"));
	if (!binary &&
	    /*
	    **	Force binary if we're in source, download or dump
	    **	mode and this is not a VM/CMS server, so we don't
	    **	get CRLF instead of LF (or CR) for newlines in text
	    **	files.	Can't do this for VM/CMS or we'll get
	    **	raw EBCDIC.  - FM
	    */
	    (format_out == WWW_SOURCE ||
	     format_out == HTAtom_for("www/download") ||
	     format_out == HTAtom_for("www/dump")) &&
	    (server_type != CMS_SERVER))
	    binary = TRUE;
	if (!binary && type && *type == 'I') {
	    /*
	    **	Force binary if we had ;type=I - FM
	    */
	    binary = TRUE;
	} else if (binary && type && *type == 'A') {
	    /*
	    **	Force ASCII if we had ;type=A - FM
	    */
	    binary = FALSE;
	}
	if (binary != control->binary) {
	    /*
	    **	Act on our setting if not already set. - FM
	    */
	    char * mode = binary ? "I" : "A";
	    status = send_cmd_2("TYPE", mode);
	    if (status != 2) {
		init_help_message_cache();  /* to free memory */
		return ((status < 0) ? status : -status);
	    }
	    control->binary = binary;
	}
	switch (server_type) {
	/*
	**  Handle what for Lynx are special case servers, e.g.,
	**  for which we respect RFC 1738, or which have known
	**  conflicts in suffix mappings. - FM
	*/
	case VMS_SERVER:
	  {
	    char *cp1, *cp2;
	    BOOL included_device = FALSE;
	    BOOL found_tilde = FALSE;
	    /** Accept only Unix-style filename **/
	    if (strchr(filename, ':') != NULL ||
		strchr(filename, '[') != NULL) {
		FREE(fname);
		init_help_message_cache();  /* to free memory */
		NETCLOSE(control->socket);
		control->socket = -1;
		CTRACE((tfp, "HTFTP: Rejecting path due to non-Unix-style syntax.\n"));
		return -1;
	    }
	    /** Handle any unescaped "/%2F" path **/
	    if (!strncmp(filename, "//", 2)) {
		int i;
		included_device = TRUE;
		for (i = 0; filename[(i+1)]; i++)
		    filename[i] = filename[(i+1)];
		filename[i] = '\0';
		CTRACE((tfp, "HTFTP: Trimmed '%s'\n", filename));
		cp = HTVMS_name("", filename);
		CTRACE((tfp, "HTFTP: VMSized '%s'\n", cp));
		if ((cp1=strrchr(cp, ']')) != NULL) {
		    strcpy(filename, ++cp1);
		    CTRACE((tfp, "HTFTP: Filename '%s'\n", filename));
		    *cp1 = '\0';
		    status = send_cwd(cp);
		    if (status != 2) {
			char *dotslash = 0;
			if ((cp1=strchr(cp, '[')) != NULL) {
			    *cp1++ = '\0';
			    status = send_cwd(cp);
			    if (status != 2) {
				FREE(fname);
				init_help_message_cache(); /* to free memory */
				NETCLOSE(control->socket);
				control->socket = -1;
				return ((status < 0) ? status : -status);
			    }
			    HTSprintf0(&dotslash, "[.%s", cp1);
			    status = send_cwd(dotslash);
			    FREE(dotslash);
			    if (status != 2) {
				FREE(fname);
				init_help_message_cache(); /* to free memory */
				NETCLOSE(control->socket);
				control->socket = -1;
				return ((status < 0) ? status : -status);
			    }
			} else {
			    FREE(fname);
			    init_help_message_cache();	/* to free memory */
			    NETCLOSE(control->socket);
			    control->socket = -1;
			    return ((status < 0) ? status : -status);
			}
		    }
		} else if ((cp1=strchr(cp, ':')) != NULL &&
			   strchr(cp, '[') == NULL &&
			   strchr(cp, ']') == NULL) {
		    cp1++;
		    if (*cp1 != '\0') {
			strcpy(filename, cp1);
			CTRACE((tfp, "HTFTP: Filename '%s'\n", filename));
			HTSprintf0(&vmsname, "%.*s[%s]", cp1-cp, cp, filename);
			status = send_cwd(vmsname);
			if (status != 2) {
			    HTSprintf(&vmsname, "%.*s[000000]", cp1-cp, cp);
			    status = send_cwd(vmsname);
			    if (status != 2) {
				HTSprintf(&vmsname, "%.*s", cp1-cp, cp);
				status = send_cwd(vmsname);
				if (status != 2) {
				    FREE(fname);
				    init_help_message_cache();
				    NETCLOSE(control->socket);
				    control->socket = -1;
				    return ((status < 0) ? status : -status);
				}
			    }
			} else {
			    HTSprintf0(&vmsname, "000000");
			    filename = vmsname;
			}
		    }
		} else if (0==strcmp(cp, (filename+1))) {
		    status = send_cwd(cp);
		    if (status != 2) {
			HTSprintf0(&vmsname, "%s:", cp);
			status = send_cwd(vmsname);
			if (status != 2) {
			    FREE(fname);
			    init_help_message_cache();	/* to free memory */
			    NETCLOSE(control->socket);
			    control->socket = -1;
			    return ((status < 0) ? status : -status);
			}
		    }
		    HTSprintf0(&vmsname, "000000");
		    filename = vmsname;
		}
	    }
	    /** Trim trailing slash if filename is not the top directory **/
	    if (strlen(filename) > 1 && filename[strlen(filename)-1] == '/')
		filename[strlen(filename)-1] = '\0';

#ifdef MAINTAIN_CONNECTION /* Don't need this if always new connection - F.M. */
	    if (!included_device) {
		/** Get the current default VMS device:[directory] **/
		status = send_cmd_1("PWD");
		if (status != 2) {
		    FREE(fname);
		    init_help_message_cache();	/* to free memory */
		    NETCLOSE(control->socket);
		    control->socket = -1;
		    return ((status < 0) ? status : -status);
		}
		/** Go to the VMS account's top directory **/
		if ((cp=strchr(response_text, '[')) != NULL &&
		    (cp1=strrchr(response_text, ']')) != NULL) {
		    char *tmp = 0;
		    unsigned len = 4;

		    StrAllocCopy(tmp, cp);
		    if ((cp2=strchr(cp, '.')) != NULL && cp2 < cp1) {
			len += (cp2 - cp);
		    } else {
			len += (cp1 - cp);
		    }
		    tmp[len] = 0;
		    StrAllocCat(tmp, "]");

		    status = send_cwd(tmp);
		    FREE(tmp);

		    if (status != 2) {
			FREE(fname);
			init_help_message_cache();  /* to free memory */
			NETCLOSE(control->socket);
			control->socket = -1;
			return ((status < 0) ? status : -status);
		    }
		}
	    }
#endif /* MAINTAIN_CONNECTION */

	    /** If we want the VMS account's top directory, list it now **/
	    if (!(strcmp(filename, "/~")) ||
		(included_device && 0==strcmp(filename, "000000")) ||
		(strlen(filename) == 1 && *filename == '/')) {
		isDirectory = YES;
		status = send_cmd_1("LIST");
		FREE(fname);
		if (status != 1) {
		    /* Action not started */
		    init_help_message_cache();	/* to free memory */
		    NETCLOSE(control->socket);
		    control->socket = -1;
		    return ((status < 0) ? status : -status);
		}
		/** Big goto! **/
		goto listen;
	    }
	    /** Otherwise, go to appropriate directory and doctor filename **/
	    if (!strncmp(filename, "/~", 2)) {
		filename += 2;
		found_tilde = TRUE;
	    }
	    CTRACE((tfp, "check '%s' to translate x/y/ to [.x.y]\n", filename));
	    if (!included_device &&
		(cp = strchr(filename, '/')) != NULL &&
		(cp1 = strrchr(cp, '/')) != NULL &&
		(cp1 - cp) > 1) {
		char *tmp = 0;

		HTSprintf0(&tmp, "[.%.*s]", cp1-cp-1, cp+1);

		CTRACE((tfp, "change path '%s'\n", tmp));
		while ((cp2 = strrchr(tmp, '/')) != NULL)
		    *cp2 = '.';
		CTRACE((tfp, "...to  path '%s'\n", tmp));

		status = send_cwd(tmp);
		FREE(tmp);

		if (status != 2) {
		    FREE(fname);
		    init_help_message_cache();	/* to free memory */
		    NETCLOSE(control->socket);
		    control->socket = -1;
		    return ((status < 0) ? status : -status);
		}
		filename = cp1+1;
	    } else {
		if (!included_device && !found_tilde) {
		    filename += 1;
		}
	    }
	    break;
	  }
	case CMS_SERVER:
	  {
	    /*
	    **	If we want the CMS account's top directory, or a base
	    **	SFS or anonymous directory path (i.e., without a slash),
	    **	list it now. FM
	    */
	    if ((strlen(filename) == 1 && *filename == '/') ||
		((0 == strncasecomp((filename+1), "vmsysu:", 7)) &&
		 (cp = strchr((filename+1), '.')) != NULL &&
		 strchr(cp, '/') == NULL) ||
		(0 == strncasecomp(filename+1, "anonymou.", 9) &&
		 strchr(filename+1, '/') == NULL)) {
		if (filename[1] != '\0') {
		    status = send_cwd(filename+1);
		    if (status != 2) {
			/* Action not started */
			init_help_message_cache();  /* to free memory */
			NETCLOSE(control->socket);
			control->socket = -1;
			return ((status < 0) ? status : -status);
		    }
		}
		isDirectory = YES;
		if (use_list)
		    status = send_cmd_1("LIST");
		else
		    status = send_cmd_1("NLST");
		FREE(fname);
		if (status != 1) {
		    /* Action not started */
		    init_help_message_cache();	/* to free memory */
		    NETCLOSE(control->socket);
		    control->socket = -1;
		    return ((status < 0) ? status : -status);
		}
		/** Big goto! **/
		goto listen;
	    }
	    filename++;

	    /** Otherwise, go to appropriate directory and adjust filename **/
	    while ((cp = strchr(filename, '/')) != NULL) {
		*cp++ = '\0';
		status = send_cwd(filename);
		if (status == 2) {
		    if (*cp == '\0') {
			isDirectory = YES;
			if (use_list)
			    status = send_cmd_1("LIST");
			else
			    status = send_cmd_1("NLST");
			FREE(fname);
			if (status != 1) {
			    /** Action not started **/
			    init_help_message_cache();	/* to free memory */
			    NETCLOSE(control->socket);
			    control->socket = -1;
			    return ((status < 0) ? status : -status);
			}
			/** Clear any messages from the login directory **/
			init_help_message_cache();
			/** Big goto! **/
			goto listen;
		    }
		    filename = cp;
		}
	    }
	    break;
	  }
	default:
	    /** Shift for any unescaped "/%2F" path **/
	    if (!strncmp(filename, "//", 2))
		filename++;
	    break;
	}
	/*
	**  Act on a file or listing request, or try to figure out
	**  which we're dealing with if we don't know yet. - FM
	*/
	if (!(type) || (type && *type != 'D')) {
	    status = send_cmd_2("RETR", filename);
	} else {
	    status = 5;		/* Failed status set as flag. - FM */
	}
	if (status != 1) {	/* Failed : try to CWD to it */
	    /** Clear any login messages if this isn't the login directory **/
	    if (strcmp(filename, "/"))
		init_help_message_cache();

	    status = send_cwd(filename);
	    if (status == 2) {	/* Succeeded : let's NAME LIST it */
		isDirectory = YES;
		if (use_list)
		    status = send_cmd_1("LIST");
		else
		    status = send_cmd_1("NLST");
	    }
	}
	FREE(fname);
	FREE(vmsname);
	if (status != 1) {
	    init_help_message_cache();	/* to free memory */
	    NETCLOSE(control->socket);
	    control->socket = -1;
	    if (status < 0)
		return status;
	    else
		return -status;
	}
    }

listen:
    if(!ftp_passive) {
	/* Wait for the connection */
#ifdef INET6
	struct sockaddr_storage soc_address;
#else
	struct sockaddr_in soc_address;
#endif /* INET6 */
	int	soc_addrlen=sizeof(soc_address);
#ifdef SOCKS
	if (socks_flag)
	    status = Raccept(master_socket,
			     (struct sockaddr *)&soc_address,
			     (void *)&soc_addrlen);
	else
#endif /* SOCKS */
	status = accept(master_socket,
			(struct sockaddr *)&soc_address,
			(void *)&soc_addrlen);
	if (status < 0) {
	    init_help_message_cache();	/* to free memory */
	    return HTInetStatus("accept");
	}
	CTRACE((tfp, "TCP: Accepted new socket %d\n", status));
	data_soc = status;
    } /* !ftp_passive */

#if 0	/* no - this makes the data connection go away too soon (2.8.3dev.22) */
    if ((status = send_cmd_nowait("QUIT")) == 1)
	outstanding++;
#endif

    if (isDirectory) {
	if (server_type == UNIX_SERVER && !unsure_type &&
	    !strcmp(response_text,
		    "150 Opening ASCII mode data connection for /bin/dl.\n")) {
	    CTRACE((tfp, "HTFTP: Treating as \"dls\" server.\n"));
	    server_type = DLS_SERVER;
	}
	final_status = read_directory (anchor, name, format_out, sink);
	if (final_status > 0) {
	    if (server_type != CMS_SERVER)
		if (outstanding-- > 0) {
		    status = response(0);
		    if (status < 0 ||
			(status == 2 && !strncmp(response_text, "221", 3)))
			outstanding = 0;
		}
	} else {		/* HT_INTERRUPTED */
	    /* User may have pressed 'z' to give up because no
	       packets got through, so let's not make them wait
	       any longer - kw */
	    outstanding = 0;
	}

	if (data_soc != -1) { /* normally done in read_directory */
	    CTRACE((tfp, "HTFTP: Closing data socket %d\n", data_soc));
	    status = NETCLOSE(data_soc);
	    if (status == -1)
		HTInetStatus("close");	/* Comment only */
	}
	status = final_status;
    } else {
	int rv;
	char *FileName = HTParse(name, "", PARSE_PATH + PARSE_PUNCTUATION);

	/** Clear any login messages **/
	init_help_message_cache();

	/** Fake a Content-Encoding for compressed files. - FM **/
	HTUnEscape(FileName);
	if (!IsUnityEnc(encoding)) {
	    /*
	     *	We already know from the call to HTFileFormat above that
	     *	this is a compressed file, no need to look at the filename
	     *	again. - kw
	     */
	    StrAllocCopy(anchor->content_type, format->name);
	    StrAllocCopy(anchor->content_encoding, HTAtom_name(encoding));
	    format = HTAtom_for("www/compressed");

	} else {
	    char *dot;
	    CompressFileType cft = HTCompressFileType(FileName, "._-", &dot);

	    if (cft != cftNone) {
		*dot = '\0';
		format = HTFileFormat(FileName, &encoding, NULL);
		format = HTCharsetFormat(format, anchor, -1);
		StrAllocCopy(anchor->content_type, format->name);
		format = HTAtom_for("www/compressed");

		switch (cft) {
		case cftCompress:
		    StrAllocCopy(anchor->content_encoding, "x-compress");
		    break;
		case cftGzip:
		    StrAllocCopy(anchor->content_encoding, "x-gzip");
		    break;
		case cftBzip2:
		    StrAllocCopy(anchor->content_encoding, "x-bzip2");
		    break;
		default:
		    break;
		}
	    }
	}
	FREE(FileName);

	_HTProgress (gettext("Receiving FTP file."));
	rv = HTParseSocket(format, format_out, anchor, data_soc, sink);

	HTInitInput(control->socket);
	/* Reset buffering to control connection DD 921208 */

	if (rv < 0) {
#if 0	/* any known servers where ABOR would work this way? */
	    if (rv == HT_INTERRUPTED || rv == -501)
		if (send_cmd_nowait("ABOR") == 1) {
		    outstanding++;
		    CTRACE((tfp, "HTFTP: outstanding responses: %d\n", outstanding));
		}
#endif
	    if (rv == -2)	/* weird error, don't expect much response */
		outstanding--;
	    else if (rv == HT_INTERRUPTED || rv == -1)
		/* User may have pressed 'z' to give up because no
		   packets got through, so let's not make them wait
		   longer - kw */
		outstanding = 0;
	    CTRACE((tfp, "HTFTP: Closing data socket %d\n", data_soc));
	    status = NETCLOSE(data_soc);
	} else
	    status = 2;	/* data_soc already closed in HTCopy - kw */

	if (status < 0 && rv != HT_INTERRUPTED && rv != -1) {
	    (void) HTInetStatus("close");	/* Comment only */
	} else {
	    if (rv != HT_LOADED && outstanding--) {
		status = response(0);		/* Pick up final reply */
		if (status != 2 && rv != HT_INTERRUPTED && rv != -1) {
		    data_soc = -1;		/* invalidate it */
		    init_help_message_cache();  /* to free memory */
		    return HTLoadError(sink, 500, response_text);
		} else if (status <= 0) {
		    outstanding = 0;
		} else if (status == 2 && !strncmp(response_text, "221", 3))
		    outstanding = 0;
	    }
	}
	final_status = HT_LOADED;
    }
    while (outstanding-- > 0 &&
	   (status > 0)) {
	status = response(0);
	if (status == 2 && !strncmp(response_text, "221", 3))
	    break;
    }
    data_soc = -1;			/* invalidate it */
    CTRACE((tfp, "HTFTPLoad: normal end; "));
    if (control->socket < 0) {
	CTRACE((tfp, "control socket is %d\n", control->socket));
    } else {
	CTRACE((tfp, "closing control socket %d\n", control->socket));
	status = NETCLOSE(control->socket);
	if (status == -1)
	    HTInetStatus("control connection close");	/* Comment only */
    }
    control->socket = -1;
    init_help_message_cache();  /* to free memory */
      /* returns HT_LOADED (always for file if we get here) or error */
    return final_status;
} /* open_file_read */

/*
**  This function frees any user entered password, so that
**  it must be entered again for a future request. - FM
*/
PUBLIC void HTClearFTPPassword NOARGS
{
    /*
    **	Need code to check cached documents from
    **	non-anonymous ftp accounts and do something
    **	to ensure that they no longer can be accessed
    **	without a new retrieval. - FM
    */

    /*
    **	Now free the current user entered password,
    **	if any. - FM
    */
    FREE(user_entered_password);
}

#endif /* ifndef DISABLE_FTP */
