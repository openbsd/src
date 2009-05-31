/*			NEWS ACCESS				HTNews.c
 *			===========
 *
 * History:
 *	26 Sep 90	Written TBL
 *	29 Nov 91	Downgraded to C, for portable implementation.
 */

#include <HTUtils.h>		/* Coding convention macros */

#ifndef DISABLE_NEWS

/* Implements:
*/
#include <HTNews.h>

#include <HTCJK.h>
#include <HTMIME.h>
#include <HTFont.h>
#include <HTFormat.h>
#include <HTTCP.h>
#include <LYUtils.h>
#include <LYStrings.h>

#define NEWS_PORT 119		/* See rfc977 */
#define SNEWS_PORT 563		/* See Lou Montulli */
#define APPEND			/* Use append methods */
int HTNewsChunkSize = 30;	/* Number of articles for quick display */
int HTNewsMaxChunk = 40;	/* Largest number of articles in one window */

#ifndef DEFAULT_NEWS_HOST
#define DEFAULT_NEWS_HOST "news"
#endif /* DEFAULT_NEWS_HOST */

#ifndef NEWS_SERVER_FILE
#define NEWS_SERVER_FILE "/usr/local/lib/rn/server"
#endif /* NEWS_SERVER_FILE */

#ifndef NEWS_AUTH_FILE
#define NEWS_AUTH_FILE ".newsauth"
#endif /* NEWS_AUTH_FILE */

#ifdef USE_SSL
static SSL *Handle = NULL;
static int channel_s = 1;

#define NEWS_NETWRITE(sock, buff, size) \
	(Handle ? SSL_write(Handle, buff, size) : NETWRITE(sock, buff, size))
#define NEWS_NETCLOSE(sock) \
	{ (void)NETCLOSE(sock); if (Handle) { SSL_free(Handle); Handle = NULL; } }
static char HTNewsGetCharacter(void);

#define NEXT_CHAR HTNewsGetCharacter()
#else
#define NEWS_NETWRITE  NETWRITE
#define NEWS_NETCLOSE  NETCLOSE
#define NEXT_CHAR HTGetCharacter()
#endif /* USE_SSL */

#include <HTML.h>
#include <HTAccess.h>
#include <HTParse.h>
#include <HTFormat.h>
#include <HTAlert.h>

#include <LYNews.h>
#include <LYGlobalDefs.h>
#include <LYLeaks.h>

#define SnipIn(d,fmt,len,s)      sprintf(d, fmt,      (int)sizeof(d)-len, s)
#define SnipIn2(d,fmt,tag,len,s) sprintf(d, fmt, tag, (int)sizeof(d)-len, s)

struct _HTStructured {
    const HTStructuredClass *isa;
    /* ... */
};
struct _HTStream {
    HTStreamClass *isa;
};

#define LINE_LENGTH 512		/* Maximum length of line of ARTICLE etc */
#define GROUP_NAME_LENGTH	256	/* Maximum length of group name */

/*
 *  Module-wide variables.
 */
char *HTNewsHost = NULL;	/* Default host */
static char *NewsHost = NULL;	/* Current host */
static char *NewsHREF = NULL;	/* Current HREF prefix */
static int s;			/* Socket for NewsHost */
static int HTCanPost = FALSE;	/* Current POST permission */
static char response_text[LINE_LENGTH + 1];	/* Last response */

/* static HText *	HT;	*//* the new hypertext */
static HTStructured *target;	/* The output sink */
static HTStructuredClass targetClass;	/* Copy of fn addresses */
static HTStream *rawtarget = NULL;	/* The output sink for rawtext */
static HTStreamClass rawtargetClass;	/* Copy of fn addresses */
static HTParentAnchor *node_anchor;	/* Its anchor */
static int diagnostic;		/* level: 0=none 2=source */
static BOOL rawtext = NO;	/* Flag: HEAD or -mime_headers */
static HTList *NNTP_AuthInfo = NULL;	/* AUTHINFO database */
static char *name = NULL;
static char *address = NULL;
static char *dbuf = NULL;	/* dynamic buffer for long messages etc. */

#define PUTC(c) (*targetClass.put_character)(target, c)
#define PUTS(s) (*targetClass.put_string)(target, s)
#define RAW_PUTS(s) (*rawtargetClass.put_string)(rawtarget, s)
#define START(e) (*targetClass.start_element)(target, e, 0, 0, -1, 0)
#define END(e) (*targetClass.end_element)(target, e, 0)
#define MAYBE_END(e) if (HTML_dtd.tags[e].contents != SGML_EMPTY) \
			(*targetClass.end_element)(target, e, 0)
#define FREE_TARGET if (rawtext) (*rawtargetClass._free)(rawtarget); \
			else (*targetClass._free)(target)
#define ABORT_TARGET if (rawtext) (*rawtargetClass._abort)(rawtarget, NULL); \
			else (*targetClass._abort)(target, NULL)

typedef struct _NNTPAuth {
    char *host;
    char *user;
    char *pass;
} NNTPAuth;

#ifdef LY_FIND_LEAKS
static void free_news_globals(void)
{
    if (s >= 0) {
	NEWS_NETCLOSE(s);
	s = -1;
    }
    FREE(HTNewsHost);
    FREE(NewsHost);
    FREE(NewsHREF);
    FREE(name);
    FREE(address);
    FREE(dbuf);
}
#endif /* LY_FIND_LEAKS */

static void free_NNTP_AuthInfo(void)
{
    HTList *cur = NNTP_AuthInfo;
    NNTPAuth *auth = NULL;

    if (!cur)
	return;

    while (NULL != (auth = (NNTPAuth *) HTList_nextObject(cur))) {
	FREE(auth->host);
	FREE(auth->user);
	FREE(auth->pass);
	FREE(auth);
    }
    HTList_delete(NNTP_AuthInfo);
    NNTP_AuthInfo = NULL;
    return;
}

/*
 * Initialize the authentication list by loading the user's $HOME/.newsauth
 * file.  That file is part of tin's configuration and is used by a few other
 * programs.
 */
static void load_NNTP_AuthInfo(void)
{
    FILE *fp;
    char fname[LY_MAXPATH];
    char buffer[LINE_LENGTH + 1];

    LYAddPathToHome(fname, sizeof(fname), NEWS_AUTH_FILE);

    if ((fp = fopen(fname, "r")) != 0) {
	while (fgets(buffer, sizeof(buffer), fp) != 0) {
	    char the_host[LINE_LENGTH + 1];
	    char the_pass[LINE_LENGTH + 1];
	    char the_user[LINE_LENGTH + 1];

	    if (sscanf(buffer, "%s%s%s", the_host, the_pass, the_user) == 3
		&& strlen(the_host) != 0
		&& strlen(the_pass) != 0
		&& strlen(the_user) != 0) {
		NNTPAuth *auth = typecalloc(NNTPAuth);

		if (auth == NULL)
		    break;
		StrAllocCopy(auth->host, the_host);
		StrAllocCopy(auth->pass, the_pass);
		StrAllocCopy(auth->user, the_user);

		HTList_appendObject(NNTP_AuthInfo, auth);
	    }
	}
	fclose(fp);
    }
}

const char *HTGetNewsHost(void)
{
    return HTNewsHost;
}

void HTSetNewsHost(const char *value)
{
    StrAllocCopy(HTNewsHost, value);
}

/*	Initialisation for this module
 *	------------------------------
 *
 *	Except on the NeXT, we pick up the NewsHost name from
 *
 *	1.	Environment variable NNTPSERVER
 *	2.	File NEWS_SERVER_FILE
 *	3.	Compilation time macro DEFAULT_NEWS_HOST
 *	4.	Default to "news"
 *
 *	On the NeXT, we pick up the NewsHost name from, in order:
 *
 *	1.	WorldWideWeb default "NewsHost"
 *	2.	Global default "NewsHost"
 *	3.	News default "NewsHost"
 *	4.	Compilation time macro DEFAULT_NEWS_HOST
 *	5.	Default to "news"
 */
static BOOL initialized = NO;
static BOOL initialize(void)
{
#ifdef NeXTStep
    char *cp = NULL;
#endif

    /*
     * Get name of Host.
     */
#ifdef NeXTStep
    if ((cp = NXGetDefaultValue("WorldWideWeb", "NewsHost")) == 0) {
	if ((cp = NXGetDefaultValue("News", "NewsHost")) == 0) {
	    StrAllocCopy(HTNewsHost, DEFAULT_NEWS_HOST);
	}
    }
    if (cp) {
	StrAllocCopy(HTNewsHost, cp);
	cp = NULL;
    }
#else
    if (LYGetEnv("NNTPSERVER")) {
	StrAllocCopy(HTNewsHost, LYGetEnv("NNTPSERVER"));
	CTRACE((tfp, "HTNews: NNTPSERVER defined as `%s'\n",
		HTNewsHost));
    } else {
	FILE *fp = fopen(NEWS_SERVER_FILE, TXT_R);

	if (fp) {
	    char server_name[MAXHOSTNAMELEN + 1];

	    if (fgets(server_name, sizeof server_name, fp) != NULL) {
		char *p = strchr(server_name, '\n');

		if (p != NULL)
		    *p = '\0';
		StrAllocCopy(HTNewsHost, server_name);
		CTRACE((tfp, "HTNews: File %s defines news host as `%s'\n",
			NEWS_SERVER_FILE, HTNewsHost));
	    }
	    fclose(fp);
	}
    }
    if (!HTNewsHost)
	StrAllocCopy(HTNewsHost, DEFAULT_NEWS_HOST);
#endif /* NeXTStep */

    s = -1;			/* Disconnected */
#ifdef LY_FIND_LEAKS
    atexit(free_news_globals);
#endif
    return YES;
}

/*	Send NNTP Command line to remote host & Check Response
 *	------------------------------------------------------
 *
 * On entry,
 *	command points to the command to be sent, including CRLF, or is null
 *		pointer if no command to be sent.
 * On exit,
 *	Negative status indicates transmission error, socket closed.
 *	Positive status is an NNTP status.
 */
static int response(char *command)
{
    int result;
    char *p = response_text;
    int ich;

    if (command) {
	int status;
	int length = strlen(command);

	CTRACE((tfp, "NNTP command to be sent: %s", command));
#ifdef NOT_ASCII
	{
	    const char *p2;
	    char *q;
	    char ascii[LINE_LENGTH + 1];

	    for (p2 = command, q = ascii; *p2; p2++, q++) {
		*q = TOASCII(*p2);
	    }
	    status = NEWS_NETWRITE(s, ascii, length);
	}
#else
	status = NEWS_NETWRITE(s, (char *) command, length);
#endif /* NOT_ASCII */
	if (status < 0) {
	    CTRACE((tfp, "HTNews: Unable to send command. Disconnecting.\n"));
	    NEWS_NETCLOSE(s);
	    s = -1;
	    return status;
	}			/* if bad status */
    }
    /* if command to be sent */
    for (;;) {
	ich = NEXT_CHAR;
	if (((*p++ = (char) ich) == LF) ||
	    (p == &response_text[LINE_LENGTH])) {
	    *--p = '\0';	/* Terminate the string */
	    CTRACE((tfp, "NNTP Response: %s\n", response_text));
	    sscanf(response_text, "%d", &result);
	    return result;
	}
	/* if end of line */
	if (ich == EOF) {
	    *(p - 1) = '\0';
	    if (interrupted_in_htgetcharacter) {
		CTRACE((tfp,
			"HTNews: Interrupted on read, closing socket %d\n",
			s));
	    } else {
		CTRACE((tfp, "HTNews: EOF on read, closing socket %d\n",
			s));
	    }
	    NEWS_NETCLOSE(s);	/* End of file, close socket */
	    s = -1;
	    if (interrupted_in_htgetcharacter) {
		interrupted_in_htgetcharacter = 0;
		return (HT_INTERRUPTED);
	    }
	    return ((int) EOF);	/* End of file on response */
	}
    }				/* Loop over characters */
}

/*	Case insensitive string comparisons
 *	-----------------------------------
 *
 * On entry,
 *	template must be already in upper case.
 *	unknown may be in upper or lower or mixed case to match.
 */
static BOOL match(const char *unknown, const char *ctemplate)
{
    const char *u = unknown;
    const char *t = ctemplate;

    for (; *u && *t && (TOUPPER(*u) == *t); u++, t++) ;		/* Find mismatch or end */
    return (BOOL) (*t == 0);	/* OK if end of template */
}

typedef enum {
    NNTPAUTH_ERROR = 0,		/* general failure */
    NNTPAUTH_OK = 281,		/* authenticated successfully */
    NNTPAUTH_CLOSE = 502	/* server probably closed connection */
} NNTPAuthResult;

/*
 *  This function handles nntp authentication. - FM
 */
static NNTPAuthResult HTHandleAuthInfo(char *host)
{
    HTList *cur = NULL;
    NNTPAuth *auth = NULL;
    char *UserName = NULL;
    char *PassWord = NULL;
    char *msg = NULL;
    char buffer[512];
    int status, tries;

    /*
     * Make sure we have an interactive user and a host.  - FM
     */
    if (dump_output_immediately || !(host && *host))
	return NNTPAUTH_ERROR;

    /*
     * Check for an existing authorization entry.  - FM
     */
    if (NNTP_AuthInfo == NULL) {
	NNTP_AuthInfo = HTList_new();
	load_NNTP_AuthInfo();
#ifdef LY_FIND_LEAKS
	atexit(free_NNTP_AuthInfo);
#endif
    }

    cur = NNTP_AuthInfo;
    while (NULL != (auth = (NNTPAuth *) HTList_nextObject(cur))) {
	if (!strcmp(auth->host, host)) {
	    UserName = auth->user;
	    PassWord = auth->pass;
	    break;
	}
    }

    /*
     * Handle the username.  - FM
     */
    buffer[sizeof(buffer) - 1] = '\0';
    tries = 3;

    while (tries) {
	if (UserName == NULL) {
	    HTSprintf0(&msg, gettext("Username for news host '%s':"), host);
	    UserName = HTPrompt(msg, NULL);
	    FREE(msg);
	    if (!(UserName && *UserName)) {
		FREE(UserName);
		return NNTPAUTH_ERROR;
	    }
	}
	sprintf(buffer, "AUTHINFO USER %.*s%c%c",
		(int) sizeof(buffer) - 17, UserName, CR, LF);
	if ((status = response(buffer)) < 0) {
	    if (status == HT_INTERRUPTED)
		_HTProgress(CONNECTION_INTERRUPTED);
	    else
		HTAlert(FAILED_CONNECTION_CLOSED);
	    if (auth) {
		if (auth->user != UserName) {
		    FREE(auth->user);
		    auth->user = UserName;
		}
	    } else {
		FREE(UserName);
	    }
	    return NNTPAUTH_CLOSE;
	}
	if (status == 281) {
	    /*
	     * Username is accepted and no password is required.  - FM
	     */
	    if (auth) {
		if (auth->user != UserName) {
		    FREE(auth->user);
		    auth->user = UserName;
		}
	    } else {
		/*
		 * Store the accepted username and no password.  - FM
		 */
		if ((auth = typecalloc(NNTPAuth)) != NULL) {
		    StrAllocCopy(auth->host, host);
		    auth->user = UserName;
		    HTList_appendObject(NNTP_AuthInfo, auth);
		}
	    }
	    return NNTPAUTH_OK;
	}
	if (status != 381) {
	    /*
	     * Not success, nor a request for the password, so it must be an
	     * error.  - FM
	     */
	    HTAlert(response_text);
	    tries--;
	    if ((tries > 0) && HTConfirm(gettext("Change username?"))) {
		if (!auth || auth->user != UserName) {
		    FREE(UserName);
		}
		if ((UserName = HTPrompt(gettext("Username:"), UserName))
		    != NULL &&
		    *UserName) {
		    continue;
		}
	    }
	    if (auth) {
		if (auth->user != UserName) {
		    FREE(auth->user);
		}
		FREE(auth->pass);
	    }
	    FREE(UserName);
	    return NNTPAUTH_ERROR;
	}
	break;
    }

    if (status == 381) {
	/*
	 * Handle the password.  - FM
	 */
	tries = 3;
	while (tries) {
	    if (PassWord == NULL) {
		HTSprintf0(&msg, gettext("Password for news host '%s':"), host);
		PassWord = HTPromptPassword(msg);
		FREE(msg);
		if (!(PassWord && *PassWord)) {
		    FREE(PassWord);
		    return NNTPAUTH_ERROR;
		}
	    }
	    sprintf(buffer, "AUTHINFO PASS %.*s%c%c",
		    (int) sizeof(buffer) - 17, PassWord, CR, LF);
	    if ((status = response(buffer)) < 0) {
		if (status == HT_INTERRUPTED) {
		    _HTProgress(CONNECTION_INTERRUPTED);
		} else {
		    HTAlert(FAILED_CONNECTION_CLOSED);
		}
		if (auth) {
		    if (auth->user != UserName) {
			FREE(auth->user);
			auth->user = UserName;
		    }
		    if (auth->pass != PassWord) {
			FREE(auth->pass);
			auth->pass = PassWord;
		    }
		} else {
		    FREE(UserName);
		    FREE(PassWord);
		}
		return NNTPAUTH_CLOSE;
	    }
	    if (status == 502) {
		/*
		 * That's what INN's nnrpd returns.  It closes the connection
		 * after this.  - kw
		 */
		HTAlert(response_text);
		if (auth) {
		    if (auth->user == UserName)
			UserName = NULL;
		    FREE(auth->user);
		    if (auth->pass == PassWord)
			PassWord = NULL;
		    FREE(auth->pass);
		}
		FREE(UserName);
		FREE(PassWord);
		return NNTPAUTH_CLOSE;
	    }
	    if (status == 281) {
		/*
		 * Password also is accepted, and everything has been stored. 
		 * - FM
		 */
		if (auth) {
		    if (auth->user != UserName) {
			FREE(auth->user);
			auth->user = UserName;
		    }
		    if (auth->pass != PassWord) {
			FREE(auth->pass);
			auth->pass = PassWord;
		    }
		} else {
		    if ((auth = typecalloc(NNTPAuth)) != NULL) {
			StrAllocCopy(auth->host, host);
			auth->user = UserName;
			auth->pass = PassWord;
			HTList_appendObject(NNTP_AuthInfo, auth);
		    }
		}
		return NNTPAUTH_OK;
	    }
	    /*
	     * Not success, so it must be an error.  - FM
	     */
	    HTAlert(response_text);
	    if (!auth || auth->pass != PassWord) {
		FREE(PassWord);
	    } else {
		PassWord = NULL;
	    }
	    tries--;
	    if ((tries > 0) && HTConfirm(gettext("Change password?"))) {
		continue;
	    }
	    if (auth) {
		if (auth->user == UserName)
		    UserName = NULL;
		FREE(auth->user);
		FREE(auth->pass);
	    }
	    FREE(UserName);
	    break;
	}
    }

    return NNTPAUTH_ERROR;
}

/*	Find Author's name in mail address
 *	----------------------------------
 *
 * On exit,
 *	Returns allocated string which cannot be freed by the
 *	calling function, and is reallocated on subsequent calls
 *	to this function.
 *
 * For example, returns "Tim Berners-Lee" if given any of
 *	" Tim Berners-Lee <tim@online.cern.ch> "
 *  or	" tim@online.cern.ch ( Tim Berners-Lee ) "
 */
static char *author_name(char *email)
{
    char *p, *e;

    StrAllocCopy(name, email);
    CTRACE((tfp, "Trying to find name in: %s\n", name));

    if ((p = strrchr(name, '(')) && (e = strrchr(name, ')'))) {
	if (e > p) {
	    *e = '\0';		/* Chop off everything after the ')'  */
	    return HTStrip(p + 1);	/* Remove leading and trailing spaces */
	}
    }

    if ((p = strrchr(name, '<')) && (e = strrchr(name, '>'))) {
	if (e++ > p) {
	    while ((*p++ = *e++) != 0)	/* Remove <...> */
		;
	    return HTStrip(name);	/* Remove leading and trailing spaces */
	}
    }

    return HTStrip(name);	/* Default to the whole thing */
}

/*	Find Author's mail address
 *	--------------------------
 *
 * On exit,
 *	Returns allocated string which cannot be freed by the
 *	calling function, and is reallocated on subsequent calls
 *	to this function.
 *
 * For example, returns "montulli@spaced.out.galaxy.net" if given any of
 *	" Lou Montulli <montulli@spaced.out.galaxy.net> "
 *  or	" montulli@spaced.out.galaxy.net ( Lou "The Stud" Montulli ) "
 */
static char *author_address(char *email)
{
    char *p, *at, *e;

    StrAllocCopy(address, email);
    CTRACE((tfp, "Trying to find address in: %s\n", address));

    if ((p = strrchr(address, '<'))) {
	if ((e = strrchr(p, '>')) && (at = strrchr(p, '@'))) {
	    if (at < e) {
		*e = '\0';	/* Remove > */
		return HTStrip(p + 1);	/* Remove leading and trailing spaces */
	    }
	}
    }

    if ((p = strrchr(address, '(')) &&
	(e = strrchr(address, ')')) && (at = strchr(address, '@'))) {
	if (e > p && at < e) {
	    *p = '\0';		/* Chop off everything after the ')'  */
	    return HTStrip(address);	/* Remove leading and trailing spaces */
	}
    }

    if ((at = strrchr(address, '@')) && at > address) {
	p = (at - 1);
	e = (at + 1);
	while (p > address && !isspace(UCH(*p)))
	    p--;
	while (*e && !isspace(UCH(*e)))
	    e++;
	*e = 0;
	return HTStrip(p);
    }

    /*
     * Default to the first word.
     */
    p = address;
    while (isspace(UCH(*p)))
	p++;			/* find first non-space */
    e = p;
    while (!isspace(UCH(*e)) && *e != '\0')
	e++;			/* find next space or end */
    *e = '\0';			/* terminate space */

    return (p);
}

/*	Start anchor element
 *	--------------------
 */
static void start_anchor(const char *href)
{
    BOOL present[HTML_A_ATTRIBUTES];
    const char *value[HTML_A_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_A_ATTRIBUTES; i++)
	present[i] = (BOOL) (i == HTML_A_HREF);
    value[HTML_A_HREF] = href;
    (*targetClass.start_element) (target, HTML_A, present, value, -1, 0);
}

/*	Start link element
 *	------------------
 */
static void start_link(const char *href, const char *rev)
{
    BOOL present[HTML_LINK_ATTRIBUTES];
    const char *value[HTML_LINK_ATTRIBUTES];
    int i;

    for (i = 0; i < HTML_LINK_ATTRIBUTES; i++)
	present[i] = (BOOL) (i == HTML_LINK_HREF || i == HTML_LINK_REV);
    value[HTML_LINK_HREF] = href;
    value[HTML_LINK_REV] = rev;
    (*targetClass.start_element) (target, HTML_LINK, present, value, -1, 0);
}

/*	Start list element
 *	------------------
 */
static void start_list(int seqnum)
{
    BOOL present[HTML_OL_ATTRIBUTES];
    const char *value[HTML_OL_ATTRIBUTES];
    char SeqNum[20];
    int i;

    for (i = 0; i < HTML_OL_ATTRIBUTES; i++)
	present[i] = (BOOL) (i == HTML_OL_SEQNUM || i == HTML_OL_START);
    sprintf(SeqNum, "%d", seqnum);
    value[HTML_OL_SEQNUM] = SeqNum;
    value[HTML_OL_START] = SeqNum;
    (*targetClass.start_element) (target, HTML_OL, present, value, -1, 0);
}

/*	Paste in an Anchor
 *	------------------
 *
 *
 * On entry,
 *	HT	has a selection of zero length at the end.
 *	text	points to the text to be put into the file, 0 terminated.
 *	addr	points to the hypertext reference address,
 *		terminated by white space, comma, NULL or '>'
 */
static void write_anchor(const char *text, const char *addr)
{
    char href[LINE_LENGTH + 1];
    const char *p;
    char *q;

    for (p = addr; *p && (*p != '>') && !WHITE(*p) && (*p != ','); p++) ;
    if (strlen(NewsHREF) + (p - addr) + 1 < sizeof(href)) {
	q = href;
	strcpy(q, NewsHREF);
	strncat(q, addr, p - addr);	/* Make complete hypertext reference */
    } else {
	q = NULL;
	HTSprintf0(&q, "%s%.*s", NewsHREF, (int) (p - addr), addr);
    }

    start_anchor(q);
    PUTS(text);
    END(HTML_A);

    if (q != href)
	FREE(q);
}

/*	Write list of anchors
 *	---------------------
 *
 *	We take a pointer to a list of objects, and write out each,
 *	generating an anchor for each.
 *
 * On entry,
 *	HT	has a selection of zero length at the end.
 *	text	points to a comma or space separated list of addresses.
 * On exit,
 *	*text	is NOT any more chopped up into substrings.
 */
static void write_anchors(char *text)
{
    char *start = text;
    char *end;
    char c;

    for (;;) {
	for (; *start && (WHITE(*start)); start++) ;	/* Find start */
	if (!*start)
	    return;		/* (Done) */
	for (end = start;
	     *end && (*end != ' ') && (*end != ','); end++) ;	/* Find end */
	if (*end)
	    end++;		/* Include comma or space but not NULL */
	c = *end;
	*end = '\0';
	if (*start == '<')
	    write_anchor(start, start + 1);
	else
	    write_anchor(start, start);
	START(HTML_BR);
	*end = c;
	start = end;		/* Point to next one */
    }
}

/*	Abort the connection					abort_socket
 *	--------------------
 */
static void abort_socket(void)
{
    CTRACE((tfp, "HTNews: EOF on read, closing socket %d\n", s));
    NEWS_NETCLOSE(s);		/* End of file, close socket */
    if (rawtext) {
	RAW_PUTS("Network Error: connection lost\n");
    } else {
	PUTS("Network Error: connection lost");
	PUTC('\n');
    }
    s = -1;			/* End of file on response */
}

/*
 *  Determine if a line is a valid header line.			valid_header
 *  -------------------------------------------
 */
static BOOLEAN valid_header(char *line)
{
    char *colon, *space;

    /*
     * Blank or tab in first position implies this is a continuation header.
     */
    if (line[0] == ' ' || line[0] == '\t')
	return (TRUE);

    /*
     * Just check for initial letter, colon, and space to make sure we discard
     * only invalid headers.
     */
    colon = strchr(line, ':');
    space = strchr(line, ' ');
    if (isalpha(UCH(line[0])) && colon && space == colon + 1)
	return (TRUE);

    /*
     * Anything else is a bad header -- it should be ignored.
     */
    return (FALSE);
}

/*	post in an Article					post_article
 *	------------------
 *			(added by FM, modeled on Lynx's previous mini inews)
 *
 *	Note the termination condition of a single dot on a line by itself.
 *
 *  On entry,
 *	s		Global socket number is OK
 *	postfile	file with header and article to post.
 */
static void post_article(char *postfile)
{
    char line[512];
    char buf[512];
    char crlf[3];
    char *cp;
    int status;
    FILE *fd;
    int in_header = 1, seen_header = 0, seen_fromline = 0;
    int blen = 0, llen = 0;

    /*
     * Open the temporary file with the nntp headers and message body.  - FM
     */
    if ((fd = fopen(NonNull(postfile), TXT_R)) == NULL) {
	HTAlert(FAILED_CANNOT_OPEN_POST);
	return;
    }

    /*
     * Read the temporary file and post in maximum 512 byte chunks.  - FM
     */
    buf[0] = '\0';
    sprintf(crlf, "%c%c", CR, LF);
    while (fgets(line, sizeof(line) - 2, fd) != NULL) {
	if ((cp = strchr(line, '\n')) != NULL)
	    *cp = '\0';
	if (line[0] == '.') {
	    /*
	     * A single '.' means end of transmission for nntp.  Lead dots on
	     * lines normally are trimmed and the EOF is not registered if the
	     * dot was not followed by CRLF.  We prepend an extra dot for any
	     * line beginning with one, to retain the one intended, as well as
	     * avoid a false EOF signal.  We know we have room for it in the
	     * buffer, because we normally send when it would exceed 510.  - FM
	     */
	    strcat(buf, ".");
	    blen++;
	}
	llen = strlen(line);
	if (in_header && !strncasecomp(line, "From:", 5)) {
	    seen_header = 1;
	    seen_fromline = 1;
	}
	if (in_header && line[0] == '\0') {
	    if (seen_header) {
		in_header = 0;
		if (!seen_fromline) {
		    if (blen >= (int) sizeof(buf) - 35) {
			NEWS_NETWRITE(s, buf, blen);
			buf[blen = 0] = 0;
		    }
		    strcat(buf, "From: anonymous@nowhere.you.know");
		    strcat(buf, crlf);
		    blen += 34;
		}
	    } else {
		continue;
	    }
	} else if (in_header) {
	    if (valid_header(line)) {
		seen_header = 1;
	    } else {
		continue;
	    }
	}
	strcat(line, crlf);
	llen += 2;
	if ((blen + llen) >= (int) sizeof(buf) - 1) {
	    NEWS_NETWRITE(s, buf, blen);
	    buf[blen = 0] = 0;
	}
	strcat(buf, line);
	blen += llen;
    }
    fclose(fd);
    HTSYS_remove(postfile);

    /*
     * Send the nntp EOF and get the server's response.  - FM
     */
    if (blen >= (int) sizeof(buf) - 4) {
	NEWS_NETWRITE(s, buf, blen);
	buf[blen = 0] = 0;
    }
    strcat(buf, ".");
    strcat(buf, crlf);
    blen += 3;
    NEWS_NETWRITE(s, buf, blen);

    status = response(NULL);
    if (status == 240) {
	/*
	 * Successful post.  - FM
	 */
	HTProgress(response_text);
    } else {
	/*
	 * Shucks, something went wrong.  - FM
	 */
	HTAlert(response_text);
    }
}

#ifdef NEWS_DEBUG
/* for DEBUG 1997/11/07 (Fri) 17:20:16 */
void debug_print(unsigned char *p)
{
    while (*p) {
	if (*p == '\0')
	    break;
	if (*p == 0x1b)
	    printf("[ESC]");
	else if (*p == '\n')
	    printf("[NL]");
	else if (*p < ' ' || *p >= 0x80)
	    printf("(%02x)", *p);
	else
	    putchar(*p);
	p++;
    }
    printf("]\n");
}
#endif

static char *decode_mime(char **str)
{
#ifdef SH_EX
    if (HTCJK != JAPANESE)
	return *str;
#endif
    HTmmdecode(str, *str);
    return HTrjis(str, *str) ? *str : "";
}

/*	Read in an Article					read_article
 *	------------------
 *
 *	Note the termination condition of a single dot on a line by itself.
 *	RFC 977 specifies that the line "folding" of RFC850 is not used, so we
 *	do not handle it here.
 *
 * On entry,
 *	s	Global socket number is OK
 *	HT	Global hypertext object is ready for appending text
 */
static int read_article(HTParentAnchor *thisanchor)
{
    char line[LINE_LENGTH + 1];
    char *full_line = NULL;
    char *subject = NULL;	/* Subject string           */
    char *from = NULL;		/* From string              */
    char *replyto = NULL;	/* Reply-to string          */
    char *date = NULL;		/* Date string              */
    char *organization = NULL;	/* Organization string      */
    char *references = NULL;	/* Hrefs for other articles */
    char *newsgroups = NULL;	/* Newsgroups list          */
    char *followupto = NULL;	/* Followup list            */
    char *href = NULL;
    char *p = line;
    char *cp;
    const char *ccp;
    BOOL done = NO;

    /*
     * Read in the HEADer of the article.
     *
     * The header fields are either ignored, or formatted and put into the
     * text.
     */
    if (!diagnostic && !rawtext) {
	while (!done) {
	    int ich = NEXT_CHAR;

	    *p++ = (char) ich;
	    if (ich == EOF) {
		if (interrupted_in_htgetcharacter) {
		    interrupted_in_htgetcharacter = 0;
		    CTRACE((tfp,
			    "HTNews: Interrupted on read, closing socket %d\n",
			    s));
		    NEWS_NETCLOSE(s);
		    s = -1;
		    return (HT_INTERRUPTED);
		}
		abort_socket();	/* End of file, close socket */
		return (HT_LOADED);	/* End of file on response */
	    }
	    if (((char) ich == LF) || (p == &line[LINE_LENGTH])) {
		*--p = '\0';	/* Terminate the string */
		CTRACE((tfp, "H %s\n", line));

		if (line[0] == '\t' || line[0] == ' ') {
		    int i = 0;

		    while (line[i]) {
			if (line[i] == '\t')
			    line[i] = ' ';
			i++;
		    }
		    if (full_line == NULL) {
			StrAllocCopy(full_line, line);
		    } else {
			StrAllocCat(full_line, line);
		    }
		} else {
		    StrAllocCopy(full_line, line);
		}

		if (full_line[0] == '.') {
		    /*
		     * End of article?
		     */
		    if (UCH(full_line[1]) < ' ') {
			done = YES;
			break;
		    }
		} else if (UCH(full_line[0]) < ' ') {
		    break;	/* End of Header? */

		} else if (match(full_line, "SUBJECT:")) {
		    StrAllocCopy(subject, HTStrip(strchr(full_line, ':') + 1));
		    decode_mime(&subject);
		} else if (match(full_line, "DATE:")) {
		    StrAllocCopy(date, HTStrip(strchr(full_line, ':') + 1));

		} else if (match(full_line, "ORGANIZATION:")) {
		    StrAllocCopy(organization,
				 HTStrip(strchr(full_line, ':') + 1));
		    decode_mime(&organization);

		} else if (match(full_line, "FROM:")) {
		    StrAllocCopy(from, HTStrip(strchr(full_line, ':') + 1));
		    decode_mime(&from);

		} else if (match(full_line, "REPLY-TO:")) {
		    StrAllocCopy(replyto, HTStrip(strchr(full_line, ':') + 1));
		    decode_mime(&replyto);

		} else if (match(full_line, "NEWSGROUPS:")) {
		    StrAllocCopy(newsgroups, HTStrip(strchr(full_line, ':') + 1));

		} else if (match(full_line, "REFERENCES:")) {
		    StrAllocCopy(references, HTStrip(strchr(full_line, ':') + 1));

		} else if (match(full_line, "FOLLOWUP-TO:")) {
		    StrAllocCopy(followupto, HTStrip(strchr(full_line, ':') + 1));

		} else if (match(full_line, "MESSAGE-ID:")) {
		    char *msgid = HTStrip(full_line + 11);

		    if (msgid[0] == '<' && msgid[strlen(msgid) - 1] == '>') {
			msgid[strlen(msgid) - 1] = '\0';	/* Chop > */
			msgid++;	/* Chop < */
			HTAnchor_setMessageID(thisanchor, msgid);
		    }

		}		/* end if match */
		p = line;	/* Restart at beginning */
	    }			/* if end of line */
	}			/* Loop over characters */
	FREE(full_line);

	START(HTML_HEAD);
	PUTC('\n');
	START(HTML_TITLE);
	if (subject && *subject != '\0')
	    PUTS(subject);
	else
	    PUTS("No Subject");
	END(HTML_TITLE);
	PUTC('\n');
	/*
	 * Put in the owner as a link rel.
	 */
	if (from || replyto) {
	    char *temp = NULL;

	    StrAllocCopy(temp, author_address(replyto ? replyto : from));
	    StrAllocCopy(href, STR_MAILTO_URL);
	    if (strchr(temp, '%') || strchr(temp, '?')) {
		cp = HTEscape(temp, URL_XPALPHAS);
		StrAllocCat(href, cp);
		FREE(cp);
	    } else {
		StrAllocCat(href, temp);
	    }
	    start_link(href, "made");
	    PUTC('\n');
	    FREE(temp);
	}
	END(HTML_HEAD);
	PUTC('\n');

	START(HTML_H1);
	if (subject && *subject != '\0')
	    PUTS(subject);
	else
	    PUTS("No Subject");
	END(HTML_H1);
	PUTC('\n');

	if (subject)
	    FREE(subject);

	START(HTML_DLC);
	PUTC('\n');

	if (from || replyto) {
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("From:");
	    END(HTML_B);
	    PUTC(' ');
	    if (from)
		PUTS(from);
	    else
		PUTS(replyto);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');

	    if (!replyto)
		StrAllocCopy(replyto, from);
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Reply to:");
	    END(HTML_B);
	    PUTC(' ');
	    start_anchor(href);
	    if (*replyto != '<')
		PUTS(author_name(replyto));
	    else
		PUTS(author_address(replyto));
	    END(HTML_A);
	    START(HTML_BR);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');

	    FREE(from);
	    FREE(replyto);
	}

	if (date) {
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Date:");
	    END(HTML_B);
	    PUTC(' ');
	    PUTS(date);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');
	    FREE(date);
	}

	if (organization) {
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Organization:");
	    END(HTML_B);
	    PUTC(' ');
	    PUTS(organization);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');
	    FREE(organization);
	}

	/* sanitize some headers - kw */
	if (newsgroups &&
	    ((cp = strchr(newsgroups, '/')) ||
	     (cp = strchr(newsgroups, '(')))) {
	    *cp = '\0';
	}
	if (newsgroups && !*newsgroups) {
	    FREE(newsgroups);
	}
	if (followupto &&
	    ((cp = strchr(followupto, '/')) ||
	     (cp = strchr(followupto, '(')))) {
	    *cp = '\0';
	}
	if (followupto && !*followupto) {
	    FREE(followupto);
	}

	if (newsgroups && HTCanPost) {
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Newsgroups:");
	    END(HTML_B);
	    PUTC('\n');
	    MAYBE_END(HTML_DT);
	    START(HTML_DD);
	    write_anchors(newsgroups);
	    MAYBE_END(HTML_DD);
	    PUTC('\n');
	}

	if (followupto && !strcasecomp(followupto, "poster")) {
	    /*
	     * "Followup-To:  poster" has special meaning.  Don't use it to
	     * construct a newsreply link.  -kw
	     */
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Followup to:");
	    END(HTML_B);
	    PUTC(' ');
	    if (href) {
		start_anchor(href);
		PUTS("poster");
		END(HTML_A);
	    } else {
		PUTS("poster");
	    }
	    MAYBE_END(HTML_DT);
	    PUTC('\n');
	    FREE(followupto);
	}

	if (newsgroups && HTCanPost) {
	    /*
	     * We have permission to POST to this host, so add a link for
	     * posting followups for this article.  - FM
	     */
	    if (!strncasecomp(NewsHREF, STR_SNEWS_URL, 6))
		StrAllocCopy(href, "snewsreply://");
	    else
		StrAllocCopy(href, "newsreply://");
	    StrAllocCat(href, NewsHost);
	    StrAllocCat(href, "/");
	    StrAllocCat(href, (followupto ? followupto : newsgroups));
	    if (*href == 'n' &&
		(ccp = HTAnchor_messageID(thisanchor)) && *ccp) {
		StrAllocCat(href, ";ref=");
		if (strchr(ccp, '<') || strchr(ccp, '&') ||
		    strchr(ccp, ' ') || strchr(ccp, ':') ||
		    strchr(ccp, '/') || strchr(ccp, '%') ||
		    strchr(ccp, ';')) {
		    char *cp1 = HTEscape(ccp, URL_XPALPHAS);

		    StrAllocCat(href, cp1);
		    FREE(cp1);
		} else {
		    StrAllocCat(href, ccp);
		}
	    }

	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("Followup to:");
	    END(HTML_B);
	    PUTC(' ');
	    start_anchor(href);
	    if (strchr((followupto ? followupto : newsgroups), ',')) {
		PUTS("newsgroups");
	    } else {
		PUTS("newsgroup");
	    }
	    END(HTML_A);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');
	}
	FREE(newsgroups);
	FREE(followupto);

	if (references) {
	    START(HTML_DT);
	    START(HTML_B);
	    PUTS("References:");
	    END(HTML_B);
	    MAYBE_END(HTML_DT);
	    PUTC('\n');
	    START(HTML_DD);
	    write_anchors(references);
	    MAYBE_END(HTML_DD);
	    PUTC('\n');
	    FREE(references);
	}

	END(HTML_DLC);
	PUTC('\n');
	FREE(href);
    }

    if (rawtext) {
	/*
	 * No tags, and never do a PUTC.  - kw
	 */
	;
    } else if (diagnostic) {
	/*
	 * Read in the HEAD and BODY of the Article as XMP formatted text.  -
	 * FM
	 */
	START(HTML_XMP);
	PUTC('\n');
    } else {
	/*
	 * Read in the BODY of the Article as PRE formatted text.  - FM
	 */
	START(HTML_PRE);
	PUTC('\n');
    }

    p = line;
    while (!done) {
	int ich = NEXT_CHAR;

	*p++ = (char) ich;
	if (ich == EOF) {
	    if (interrupted_in_htgetcharacter) {
		interrupted_in_htgetcharacter = 0;
		CTRACE((tfp,
			"HTNews: Interrupted on read, closing socket %d\n",
			s));
		NEWS_NETCLOSE(s);
		s = -1;
		return (HT_INTERRUPTED);
	    }
	    abort_socket();	/* End of file, close socket */
	    return (HT_LOADED);	/* End of file on response */
	}
	if (((char) ich == LF) || (p == &line[LINE_LENGTH])) {
	    *p++ = '\0';	/* Terminate the string */
	    CTRACE((tfp, "B %s", line));
#ifdef NEWS_DEBUG		/* 1997/11/09 (Sun) 15:56:11 */
	    debug_print(line);	/* @@@ */
#endif
	    if (line[0] == '.') {
		/*
		 * End of article?
		 */
		if (UCH(line[1]) < ' ') {
		    done = YES;
		    break;
		} else {	/* Line starts with dot */
		    if (rawtext) {
			RAW_PUTS(&line[1]);
		    } else {
			PUTS(&line[1]);		/* Ignore first dot */
		    }
		}
	    } else {
		if (rawtext) {
		    RAW_PUTS(line);
		} else if (diagnostic || !scan_for_buried_news_references) {
		    /*
		     * All lines are passed as unmodified source.  - FM
		     */
		    PUTS(line);
		} else {
		    /*
		     * Normal lines are scanned for buried references to other
		     * articles.  Unfortunately, it could pick up mail
		     * addresses as well!  It also can corrupt uuencoded
		     * messages!  So we don't do this when fetching articles as
		     * WWW_SOURCE or when downloading (diagnostic is TRUE) or
		     * if the client has set scan_for_buried_news_references to
		     * FALSE.  Otherwise, we convert all "<...@...>" strings
		     * preceded by "rticle " to "news:...@..." links, and any
		     * strings that look like URLs to links.  - FM
		     */
		    char *l = line;
		    char *p2;

		    while ((p2 = strstr(l, "rticle <")) != NULL) {
			char *q = strrchr(p2, '>');
			char *at = strrchr(p2, '@');

			if (q && at && at < q) {
			    char c = q[1];

			    q[1] = 0;	/* chop up */
			    p2 += 7;
			    *p2 = 0;
			    while (*l) {
				if (strncmp(l, STR_NEWS_URL, LEN_NEWS_URL) &&
				    strncmp(l, "snews://", 8) &&
				    strncmp(l, "nntp://", 7) &&
				    strncmp(l, "snewspost:", 10) &&
				    strncmp(l, "snewsreply:", 11) &&
				    strncmp(l, "newspost:", 9) &&
				    strncmp(l, "newsreply:", 10) &&
				    strncmp(l, "ftp://", 6) &&
				    strncmp(l, "file:/", 6) &&
				    strncmp(l, "finger://", 9) &&
				    strncmp(l, "http://", 7) &&
				    strncmp(l, "https://", 8) &&
				    strncmp(l, "wais://", 7) &&
				    strncmp(l, STR_MAILTO_URL, LEN_MAILTO_URL) &&
				    strncmp(l, "cso://", 6) &&
				    strncmp(l, "gopher://", 9)) {
				    PUTC(*l++);
				} else {
				    StrAllocCopy(href, l);
				    start_anchor(strtok(href, " \r\n\t,>)\""));
				    while (*l && !strchr(" \r\n\t,>)\"", *l))
					PUTC(*l++);
				    END(HTML_A);
				    FREE(href);
				}
			    }
			    *p2 = '<';	/* again */
			    *q = 0;
			    start_anchor(p2 + 1);
			    *q = '>';	/* again */
			    PUTS(p2);
			    END(HTML_A);
			    q[1] = c;	/* again */
			    l = q + 1;
			} else {
			    break;	/* line has unmatched <> */
			}
		    }
		    while (*l) {	/* Last bit of the line */
			if (strncmp(l, STR_NEWS_URL, LEN_NEWS_URL) &&
			    strncmp(l, "snews://", 8) &&
			    strncmp(l, "nntp://", 7) &&
			    strncmp(l, "snewspost:", 10) &&
			    strncmp(l, "snewsreply:", 11) &&
			    strncmp(l, "newspost:", 9) &&
			    strncmp(l, "newsreply:", 10) &&
			    strncmp(l, "ftp://", 6) &&
			    strncmp(l, "file:/", 6) &&
			    strncmp(l, "finger://", 9) &&
			    strncmp(l, "http://", 7) &&
			    strncmp(l, "https://", 8) &&
			    strncmp(l, "wais://", 7) &&
			    strncmp(l, STR_MAILTO_URL, LEN_MAILTO_URL) &&
			    strncmp(l, "cso://", 6) &&
			    strncmp(l, "gopher://", 9))
			    PUTC(*l++);
			else {
			    StrAllocCopy(href, l);
			    start_anchor(strtok(href, " \r\n\t,>)\""));
			    while (*l && !strchr(" \r\n\t,>)\"", *l))
				PUTC(*l++);
			    END(HTML_A);
			    FREE(href);
			}
		    }
		}		/* if diagnostic or not scan_for_buried_news_references */
	    }			/* if not dot */
	    p = line;		/* Restart at beginning */
	}			/* if end of line */
    }				/* Loop over characters */

    if (rawtext)
	return (HT_LOADED);

    if (diagnostic)
	END(HTML_XMP);
    else
	END(HTML_PRE);
    PUTC('\n');
    return (HT_LOADED);
}

/*	Read in a List of Newsgroups
 *	----------------------------
 *
 *  Note the termination condition of a single dot on a line by itself.
 *  RFC 977 specifies that the line "folding" of RFC850 is not used,
 *  so we do not handle it here.
 */
static int read_list(char *arg)
{
    char line[LINE_LENGTH + 1];
    char *p;
    BOOL done = NO;
    BOOL head = NO;
    BOOL tail = NO;
    BOOL skip_this_line = NO;
    BOOL skip_rest_of_line = NO;
    int listing = 0;
    char *pattern = NULL;
    int len = 0;

    /*
     * Support head or tail matches for groups to list.  - FM
     */
    if (arg && strlen(arg) > 1) {
	if (*arg == '*') {
	    tail = YES;
	    StrAllocCopy(pattern, (arg + 1));
	} else if (arg[strlen(arg) - 1] == '*') {
	    head = YES;
	    StrAllocCopy(pattern, arg);
	    pattern[strlen(pattern) - 1] = '\0';
	}
	if (tail || head) {
	    len = strlen(pattern);
	}

    }

    /*
     * Read the server's reply.
     *
     * The lines are scanned for newsgroup names and descriptions.
     */
    START(HTML_HEAD);
    PUTC('\n');
    START(HTML_TITLE);
    PUTS("Newsgroups");
    END(HTML_TITLE);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');
    START(HTML_H1);
    PUTS("Newsgroups");
    END(HTML_H1);
    PUTC('\n');
    p = line;
    START(HTML_DLC);
    PUTC('\n');
    while (!done) {
	int ich = NEXT_CHAR;
	char ch = (char) ich;

	if (ich == EOF) {
	    if (interrupted_in_htgetcharacter) {
		interrupted_in_htgetcharacter = 0;
		CTRACE((tfp,
			"HTNews: Interrupted on read, closing socket %d\n",
			s));
		NEWS_NETCLOSE(s);
		s = -1;
		return (HT_INTERRUPTED);
	    }
	    abort_socket();	/* End of file, close socket */
	    FREE(pattern);
	    return (HT_LOADED);	/* End of file on response */
	} else if (skip_this_line) {
	    if (ch == LF) {
		skip_this_line = skip_rest_of_line = NO;
		p = line;
	    }
	    continue;
	} else if (skip_rest_of_line) {
	    if (ch != LF) {
		continue;
	    }
	} else if (p == &line[LINE_LENGTH]) {
	    CTRACE((tfp, "b %.*s%c[...]\n", (LINE_LENGTH), line, ch));
	    *p = '\0';
	    if (ch == LF) {
		;		/* Will be dealt with below */
	    } else if (WHITE(ch)) {
		ch = LF;	/* May treat as line without description */
		skip_this_line = YES;	/* ...and ignore until LF */
	    } else if (strchr(line, ' ') == NULL &&
		       strchr(line, '\t') == NULL) {
		/* No separator found */
		CTRACE((tfp, "HTNews..... group name too long, discarding.\n"));
		skip_this_line = YES;	/* ignore whole line */
		continue;
	    } else {
		skip_rest_of_line = YES;	/* skip until ch == LF found */
	    }
	} else {
	    *p++ = ch;
	}
	if (ch == LF) {
	    skip_rest_of_line = NO;	/* done, reset flag */
	    *p = '\0';		/* Terminate the string */
	    CTRACE((tfp, "B %s", line));
	    if (line[0] == '.') {
		/*
		 * End of article?
		 */
		if (UCH(line[1]) < ' ') {
		    done = YES;
		    break;
		} else {	/* Line starts with dot */
		    START(HTML_DT);
		    PUTS(&line[1]);
		    MAYBE_END(HTML_DT);
		}
	    } else if (line[0] == '#') {	/* Comment? */
		p = line;	/* Restart at beginning */
		continue;
	    } else {
		/*
		 * Normal lines are scanned for references to newsgroups.
		 */
		int i = 0;

		/* find whitespace if it exits */
		for (; line[i] != '\0' && !WHITE(line[i]); i++) ;	/* null body */

		if (line[i] != '\0') {
		    line[i] = '\0';
		    if ((head && strncasecomp(line, pattern, len)) ||
			(tail && (i < len ||
				  strcasecomp((line + (i - len)), pattern)))) {
			p = line;	/* Restart at beginning */
			continue;
		    }
		    START(HTML_DT);
		    write_anchor(line, line);
		    listing++;
		    MAYBE_END(HTML_DT);
		    PUTC('\n');
		    START(HTML_DD);
		    PUTS(&line[i + 1]);		/* put description */
		    MAYBE_END(HTML_DD);
		} else {
		    if ((head && strncasecomp(line, pattern, len)) ||
			(tail && (i < len ||
				  strcasecomp((line + (i - len)), pattern)))) {
			p = line;	/* Restart at beginning */
			continue;
		    }
		    START(HTML_DT);
		    write_anchor(line, line);
		    MAYBE_END(HTML_DT);
		    listing++;
		}
	    }			/* if not dot */
	    p = line;		/* Restart at beginning */
	}			/* if end of line */
    }				/* Loop over characters */
    if (!listing) {
	char *msg = NULL;

	START(HTML_DT);
	HTSprintf0(&msg, gettext("No matches for: %s"), arg);
	PUTS(msg);
	MAYBE_END(HTML_DT);
	FREE(msg);
    }
    END(HTML_DLC);
    PUTC('\n');
    FREE(pattern);
    return (HT_LOADED);
}

/*	Read in a Newsgroup
 *	-------------------
 *
 *  Unfortunately, we have to ask for each article one by one if we
 *  want more than one field.
 *
 */
static int read_group(const char *groupName,
		      int first_required,
		      int last_required)
{
    char line[LINE_LENGTH + 1];
    char *author = NULL;
    char *subject = NULL;
    char *date = NULL;
    int i;
    char *p;
    BOOL done;

    char buffer[LINE_LENGTH + 1];
    char *temp = NULL;
    char *reference = NULL;	/* Href for article */
    int art;			/* Article number WITHIN GROUP */
    int status, count, first, last;	/* Response fields */

    START(HTML_HEAD);
    PUTC('\n');
    START(HTML_TITLE);
    PUTS("Newsgroup ");
    PUTS(groupName);
    END(HTML_TITLE);
    PUTC('\n');
    END(HTML_HEAD);
    PUTC('\n');

    sscanf(response_text, " %d %d %d %d", &status, &count, &first, &last);
    CTRACE((tfp, "Newsgroup status=%d, count=%d, (%d-%d) required:(%d-%d)\n",
	    status, count, first, last, first_required, last_required));
    if (last == 0) {
	PUTS(gettext("\nNo articles in this group.\n"));
	goto add_post;
    }
#define FAST_THRESHOLD 100	/* Above this, read IDs fast */
#define CHOP_THRESHOLD 50	/* Above this, chop off the rest */

    if (first_required < first)
	first_required = first;	/* clip */
    if ((last_required == 0) || (last_required > last))
	last_required = last;

    if (last_required < first_required) {
	PUTS(gettext("\nNo articles in this range.\n"));
	goto add_post;
    }

    if (last_required - first_required + 1 > HTNewsMaxChunk) {	/* Trim this block */
	first_required = last_required - HTNewsChunkSize + 1;
    }
    CTRACE((tfp, "    Chunk will be (%d-%d)\n",
	    first_required, last_required));

    /*
     * Set window title.
     */
    HTSprintf0(&temp, gettext("%s,  Articles %d-%d"),
	       groupName, first_required, last_required);
    START(HTML_H1);
    PUTS(temp);
    FREE(temp);
    END(HTML_H1);
    PUTC('\n');

    /*
     * Link to earlier articles.
     */
    if (first_required > first) {
	int before;		/* Start of one before */

	if (first_required - HTNewsMaxChunk <= first)
	    before = first;
	else
	    before = first_required - HTNewsChunkSize;
	HTSprintf0(&dbuf, "%s%s/%d-%d", NewsHREF, groupName,
		   before, first_required - 1);
	CTRACE((tfp, "    Block before is %s\n", dbuf));
	PUTC('(');
	start_anchor(dbuf);
	PUTS(gettext("Earlier articles"));
	END(HTML_A);
	PUTS("...)\n");
	START(HTML_P);
	PUTC('\n');
    }

    done = NO;

/*#define USE_XHDR*/
#ifdef USE_XHDR
    if (count > FAST_THRESHOLD) {
	HTSprintf0(&temp,
		   gettext("\nThere are about %d articles currently available in %s, IDs as follows:\n\n"),
		   count, groupName);
	PUTS(temp);
	FREE(temp);
	sprintf(buffer, "XHDR Message-ID %d-%d%c%c", first, last, CR, LF);
	status = response(buffer);
	if (status == 221) {
	    p = line;
	    while (!done) {
		int ich = NEXT_CHAR;

		*p++ = ich;
		if (ich == EOF) {
		    if (interrupted_in_htgetcharacter) {
			interrupted_in_htgetcharacter = 0;
			CTRACE((tfp,
				"HTNews: Interrupted on read, closing socket %d\n",
				s));
			NEWS_NETCLOSE(s);
			s = -1;
			return (HT_INTERRUPTED);
		    }
		    abort_socket();	/* End of file, close socket */
		    return (HT_LOADED);		/* End of file on response */
		}
		if (((char) ich == '\n') || (p == &line[LINE_LENGTH])) {
		    *p = '\0';	/* Terminate the string */
		    CTRACE((tfp, "X %s", line));
		    if (line[0] == '.') {
			/*
			 * End of article?
			 */
			if (UCH(line[1]) < ' ') {
			    done = YES;
			    break;
			} else {	/* Line starts with dot */
			    /* Ignore strange line */
			}
		    } else {
			/*
			 * Normal lines are scanned for references to articles.
			 */
			char *space = strchr(line, ' ');

			if (space++)
			    write_anchor(space, space);
		    }		/* if not dot */
		    p = line;	/* Restart at beginning */
		}		/* if end of line */
	    }			/* Loop over characters */

	    /* leaving loop with "done" set */
	}			/* Good status */
    }
#endif /* USE_XHDR */

    /*
     * Read newsgroup using individual fields.
     */
    if (!done) {
	START(HTML_B);
	if (first == first_required && last == last_required)
	    PUTS(gettext("All available articles in "));
	else
	    PUTS("Articles in ");
	PUTS(groupName);
	END(HTML_B);
	PUTC('\n');
	if (LYListNewsNumbers)
	    start_list(first_required);
	else
	    START(HTML_UL);
	for (art = first_required; art <= last_required; art++) {
/*#define OVERLAP*/
#ifdef OVERLAP
	    /*
	     * With this code we try to keep the server running flat out by
	     * queuing just one extra command ahead of time.  We assume (1)
	     * that the server won't abort if it gets input during output, and
	     * (2) that TCP buffering is enough for the two commands.  Both
	     * these assumptions seem very reasonable.  However, we HAVE had a
	     * hangup with a loaded server.
	     */
	    if (art == first_required) {
		if (art == last_required) {	/* Only one */
		    sprintf(buffer, "HEAD %d%c%c",
			    art, CR, LF);
		    status = response(buffer);
		} else {	/* First of many */
		    sprintf(buffer, "HEAD %d%c%cHEAD %d%c%c",
			    art, CR, LF, art + 1, CR, LF);
		    status = response(buffer);
		}
	    } else if (art == last_required) {	/* Last of many */
		status = response(NULL);
	    } else {		/* Middle of many */
		sprintf(buffer, "HEAD %d%c%c", art + 1, CR, LF);
		status = response(buffer);
	    }
#else /* Not OVERLAP: */
	    sprintf(buffer, "HEAD %d%c%c", art, CR, LF);
	    status = response(buffer);
#endif /* OVERLAP */
	    /*
	     * Check for a good response (221) for the HEAD request, and if so,
	     * parse it.  Otherwise, indicate the error so that the number of
	     * listings corresponds to what's claimed for the range, and if we
	     * are listing numbers via an ordered list, they stay in synchrony
	     * with the article numbers.  - FM
	     */
	    if (status == 221) {	/* Head follows - parse it: */
		p = line;	/* Write pointer */
		done = NO;
		while (!done) {
		    int ich = NEXT_CHAR;

		    *p++ = (char) ich;
		    if (ich == EOF) {
			if (interrupted_in_htgetcharacter) {
			    interrupted_in_htgetcharacter = 0;
			    CTRACE((tfp,
				    "HTNews: Interrupted on read, closing socket %d\n",
				    s));
			    NEWS_NETCLOSE(s);
			    s = -1;
			    return (HT_INTERRUPTED);
			}
			abort_socket();		/* End of file, close socket */
			return (HT_LOADED);	/* End of file on response */
		    }
		    if (((char) ich == LF) ||
			(p == &line[LINE_LENGTH])) {

			*--p = '\0';	/* Terminate  & chop LF */
			p = line;	/* Restart at beginning */
			CTRACE((tfp, "G %s\n", line));
			switch (line[0]) {

			case '.':
			    /*
			     * End of article?
			     */
			    done = (BOOL) (UCH(line[1]) < ' ');
			    break;

			case 'S':
			case 's':
			    if (match(line, "SUBJECT:")) {
				StrAllocCopy(subject, line + 9);
				decode_mime(&subject);
			    }
			    break;

			case 'M':
			case 'm':
			    if (match(line, "MESSAGE-ID:")) {
				char *addr = HTStrip(line + 11) + 1;	/* Chop < */

				addr[strlen(addr) - 1] = '\0';	/* Chop > */
				StrAllocCopy(reference, addr);
			    }
			    break;

			case 'f':
			case 'F':
			    if (match(line, "FROM:")) {
				char *p2;

				StrAllocCopy(author, strchr(line, ':') + 1);
				decode_mime(&author);
				p2 = author + strlen(author) - 1;
				if (*p2 == LF)
				    *p2 = '\0';		/* Chop off newline */
			    }
			    break;

			case 'd':
			case 'D':
			    if (LYListNewsDates && match(line, "DATE:")) {
				StrAllocCopy(date,
					     HTStrip(strchr(line, ':') + 1));
			    }
			    break;

			}	/* end switch on first character */
		    }		/* if end of line */
		}		/* Loop over characters */

		PUTC('\n');
		START(HTML_LI);
		p = decode_mime(&subject);
		HTSprintf0(&temp, "\"%s\"", NonNull(p));
		if (reference) {
		    write_anchor(temp, reference);
		    FREE(reference);
		} else {
		    PUTS(temp);
		}
		FREE(temp);

		if (author != NULL) {
		    PUTS(" - ");
		    if (LYListNewsDates)
			START(HTML_I);
		    PUTS(decode_mime(&author));
		    if (LYListNewsDates)
			END(HTML_I);
		    FREE(author);
		}
		if (date) {
		    if (!diagnostic) {
			for (i = 0; date[i]; i++) {
			    if (date[i] == ' ') {
				date[i] = HT_NON_BREAK_SPACE;
			    }
			}
		    }
		    sprintf(buffer, " [%.*s]", (int) (sizeof(buffer) - 4), date);
		    PUTS(buffer);
		    FREE(date);
		}
		MAYBE_END(HTML_LI);
		/*
		 * Indicate progress!  @@@@@@
		 */
	    } else if (status == HT_INTERRUPTED) {
		interrupted_in_htgetcharacter = 0;
		CTRACE((tfp,
			"HTNews: Interrupted on read, closing socket %d\n",
			s));
		NEWS_NETCLOSE(s);
		s = -1;
		return (HT_INTERRUPTED);
	    } else {
		/*
		 * Use the response text on error.  - FM
		 */
		PUTC('\n');
		START(HTML_LI);
		START(HTML_I);
		if (LYListNewsNumbers)
		    LYstrncpy(buffer, "Status:", sizeof(buffer) - 1);
		else
		    sprintf(buffer, "Status (ARTICLE %d):", art);
		PUTS(buffer);
		END(HTML_I);
		PUTC(' ');
		PUTS(response_text);
		MAYBE_END(HTML_LI);
	    }			/* Handle response to HEAD request */
	}			/* Loop over article */
	FREE(author);
	FREE(subject);
    }				/* If read headers */
    PUTC('\n');
    if (LYListNewsNumbers)
	END(HTML_OL);
    else
	END(HTML_UL);
    PUTC('\n');

    /*
     * Link to later articles.
     */
    if (last_required < last) {
	int after;		/* End of article after */

	after = last_required + HTNewsChunkSize;
	if (after == last)
	    HTSprintf0(&dbuf, "%s%s", NewsHREF, groupName);	/* original group */
	else
	    HTSprintf0(&dbuf, "%s%s/%d-%d", NewsHREF, groupName,
		       last_required + 1, after);
	CTRACE((tfp, "    Block after is %s\n", dbuf));
	PUTC('(');
	start_anchor(dbuf);
	PUTS(gettext("Later articles"));
	END(HTML_A);
	PUTS("...)\n");
    }

  add_post:
    if (HTCanPost) {
	/*
	 * We have permission to POST to this host, so add a link for posting
	 * messages to this newsgroup.  - FM
	 */
	char *href = NULL;

	START(HTML_HR);
	PUTC('\n');
	if (!strncasecomp(NewsHREF, STR_SNEWS_URL, 6))
	    StrAllocCopy(href, "snewspost://");
	else
	    StrAllocCopy(href, "newspost://");
	StrAllocCat(href, NewsHost);
	StrAllocCat(href, "/");
	StrAllocCat(href, groupName);
	start_anchor(href);
	PUTS(gettext("Post to "));
	PUTS(groupName);
	END(HTML_A);
	FREE(href);
    } else {
	START(HTML_HR);
    }
    PUTC('\n');
    return (HT_LOADED);
}

/*	Load by name.						HTLoadNews
 *	=============
 */
static int HTLoadNews(const char *arg,
		      HTParentAnchor *anAnchor,
		      HTFormat format_out,
		      HTStream *stream)
{
    char command[262];		/* The whole command */
    char proxycmd[260];		/* The proxy command */
    char groupName[GROUP_NAME_LENGTH];	/* Just the group name */
    int status;			/* tcp return */
    int retries;		/* A count of how hard we have tried */
    BOOL normal_url;		/* Flag: "news:" or "nntp:" (physical) URL */
    BOOL group_wanted;		/* Flag: group was asked for, not article */
    BOOL list_wanted;		/* Flag: list was asked for, not article */
    BOOL post_wanted;		/* Flag: new post to group was asked for */
    BOOL reply_wanted;		/* Flag: followup post was asked for */
    BOOL spost_wanted;		/* Flag: new SSL post to group was asked for */
    BOOL sreply_wanted;		/* Flag: followup SSL post was asked for */
    BOOL head_wanted = NO;	/* Flag: want HEAD of single article */
    int first, last;		/* First and last articles asked for */
    char *cp = 0;
    char *ListArg = NULL;
    char *ProxyHost = NULL;
    char *ProxyHREF = NULL;
    char *postfile = NULL;

#ifdef USE_SSL
    char SSLprogress[256];
#endif /* USE_SSL */

    diagnostic = (format_out == WWW_SOURCE ||	/* set global flag */
		  format_out == HTAtom_for("www/download") ||
		  format_out == HTAtom_for("www/dump"));
    rawtext = NO;

    CTRACE((tfp, "HTNews: Looking for %s\n", arg));

    if (!initialized)
	initialized = initialize();
    if (!initialized)
	return -1;		/* FAIL */

    FREE(NewsHREF);
    command[0] = '\0';
    command[sizeof(command) - 1] = '\0';
    proxycmd[0] = '\0';
    proxycmd[sizeof(proxycmd) - 1] = '\0';

    {
	const char *p1 = arg;

	/*
	 * We will ask for the document, omitting the host name & anchor.
	 *
	 * Syntax of address is
	 * xxx@yyy                 Article
	 * <xxx@yyy>               Same article
	 * xxxxx                   News group (no "@")
	 * group/n1-n2             Articles n1 to n2 in group
	 */
	normal_url = (BOOL) (!strncmp(arg, STR_NEWS_URL, LEN_NEWS_URL) ||
			     !strncmp(arg, "nntp:", 5));
	spost_wanted = (BOOL) (!normal_url && strstr(arg, "snewspost:") != NULL);
	sreply_wanted = (BOOL) (!(normal_url || spost_wanted) &&
				strstr(arg, "snewsreply:") != NULL);
	post_wanted = (BOOL) (!(normal_url || spost_wanted || sreply_wanted) &&
			      strstr(arg, "newspost:") != NULL);
	reply_wanted = (BOOL) (!(normal_url || spost_wanted || sreply_wanted ||
				 post_wanted) &&
			       strstr(arg, "newsreply:") != NULL);
	group_wanted = (BOOL) ((!(spost_wanted || sreply_wanted ||
				  post_wanted || reply_wanted) &&
				strchr(arg, '@') == NULL) &&
			       (strchr(arg, '*') == NULL));
	list_wanted = (BOOL) ((!(spost_wanted || sreply_wanted ||
				 post_wanted || reply_wanted ||
				 group_wanted) &&
			       strchr(arg, '@') == NULL) &&
			      (strchr(arg, '*') != NULL));

#ifndef USE_SSL
	if (!strncasecomp(arg, "snewspost:", 10) ||
	    !strncasecomp(arg, "snewsreply:", 11)) {
	    HTAlert(FAILED_CANNOT_POST_SSL);
	    return HT_NOT_LOADED;
	}
#endif /* !USE_SSL */
	if (post_wanted || reply_wanted || spost_wanted || sreply_wanted) {
	    /*
	     * Make sure we have a non-zero path for the newsgroup(s).  - FM
	     */
	    if ((p1 = strrchr(arg, '/')) != NULL) {
		p1++;
	    } else if ((p1 = strrchr(arg, ':')) != NULL) {
		p1++;
	    }
	    if (!(p1 && *p1)) {
		HTAlert(WWW_ILLEGAL_URL_MESSAGE);
		return (HT_NO_DATA);
	    }
	    if (!(cp = HTParse(arg, "", PARSE_HOST)) || *cp == '\0') {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, HTNewsHost)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, HTNewsHost);
	    } else {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, cp)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, cp);
	    }
	    FREE(cp);
	    HTSprintf0(&NewsHREF, "%s://%.*s/",
		       (post_wanted ?
			"newspost" :
			(reply_wanted ?
			 "newreply" :
			 (spost_wanted ?
			  "snewspost" : "snewsreply"))),
		       (int) sizeof(command) - 15, NewsHost);

	    /*
	     * If the SSL daemon is being used as a proxy, reset p1 to the
	     * start of the proxied URL rather than to the start of the
	     * newsgroup(s).  - FM
	     */
	    if (spost_wanted && strncasecomp(arg, "snewspost:", 10))
		p1 = strstr(arg, "snewspost:");
	    if (sreply_wanted && strncasecomp(arg, "snewsreply:", 11))
		p1 = strstr(arg, "snewsreply:");

	    /* p1 = HTParse(arg, "", PARSE_PATH | PARSE_PUNCTUATION); */
	    /*
	     * Don't use HTParse because news:  access doesn't follow
	     * traditional rules.  For instance, if the article reference
	     * contains a '#', the rest of it is lost -- JFG 10/7/92, from a
	     * bug report
	     */
	} else if (isNNTP_URL(arg)) {
	    if (((*(arg + 5) == '\0') ||
		 (!strcmp((arg + 5), "/") ||
		  !strcmp((arg + 5), "//") ||
		  !strcmp((arg + 5), "///"))) ||
		((!strncmp((arg + 5), "//", 2)) &&
		 (!(cp = strchr((arg + 7), '/')) || *(cp + 1) == '\0'))) {
		p1 = "*";
		group_wanted = FALSE;
		list_wanted = TRUE;
	    } else if (*(arg + 5) != '/') {
		p1 = (arg + 5);
	    } else if (*(arg + 5) == '/' && *(arg + 6) != '/') {
		p1 = (arg + 6);
	    } else {
		p1 = (cp + 1);
	    }
	    if (!(cp = HTParse(arg, "", PARSE_HOST)) || *cp == '\0') {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, HTNewsHost)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, HTNewsHost);
	    } else {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, cp)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, cp);
	    }
	    FREE(cp);
	    SnipIn2(command, "%s//%.*s/", STR_NNTP_URL, 9, NewsHost);
	    StrAllocCopy(NewsHREF, command);
	} else if (!strncasecomp(arg, STR_SNEWS_URL, 6)) {
#ifdef USE_SSL
	    if (((*(arg + 6) == '\0') ||
		 (!strcmp((arg + 6), "/") ||
		  !strcmp((arg + 6), "//") ||
		  !strcmp((arg + 6), "///"))) ||
		((!strncmp((arg + 6), "//", 2)) &&
		 (!(cp = strchr((arg + 8), '/')) || *(cp + 1) == '\0'))) {
		p1 = "*";
		group_wanted = FALSE;
		list_wanted = TRUE;
	    } else if (*(arg + 6) != '/') {
		p1 = (arg + 6);
	    } else if (*(arg + 6) == '/' && *(arg + 7) != '/') {
		p1 = (arg + 7);
	    } else {
		p1 = (cp + 1);
	    }
	    if (!(cp = HTParse(arg, "", PARSE_HOST)) || *cp == '\0') {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, HTNewsHost)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, HTNewsHost);
	    } else {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, cp)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, cp);
	    }
	    FREE(cp);
	    sprintf(command, "%s//%.250s/", STR_SNEWS_URL, NewsHost);
	    StrAllocCopy(NewsHREF, command);
#else
	    HTAlert(gettext("This client does not contain support for SNEWS URLs."));
	    return HT_NOT_LOADED;
#endif /* USE_SSL */
	} else if (!strncasecomp(arg, "news:/", 6)) {
	    if (((*(arg + 6) == '\0') ||
		 !strcmp((arg + 6), "/") ||
		 !strcmp((arg + 6), "//")) ||
		((*(arg + 6) == '/') &&
		 (!(cp = strchr((arg + 7), '/')) || *(cp + 1) == '\0'))) {
		p1 = "*";
		group_wanted = FALSE;
		list_wanted = TRUE;
	    } else if (*(arg + 6) != '/') {
		p1 = (arg + 6);
	    } else {
		p1 = (cp + 1);
	    }
	    if (!(cp = HTParse(arg, "", PARSE_HOST)) || *cp == '\0') {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, HTNewsHost)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, HTNewsHost);
	    } else {
		if (s >= 0 && NewsHost && strcasecomp(NewsHost, cp)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		StrAllocCopy(NewsHost, cp);
	    }
	    FREE(cp);
	    SnipIn(command, "news://%.*s/", 9, NewsHost);
	    StrAllocCopy(NewsHREF, command);
	} else {
	    p1 = (arg + 5);	/* Skip "news:" prefix */
	    if (*p1 == '\0') {
		p1 = "*";
		group_wanted = FALSE;
		list_wanted = TRUE;
	    }
	    if (s >= 0 && NewsHost && strcasecomp(NewsHost, HTNewsHost)) {
		NEWS_NETCLOSE(s);
		s = -1;
	    }
	    StrAllocCopy(NewsHost, HTNewsHost);
	    StrAllocCopy(NewsHREF, STR_NEWS_URL);
	}

	/*
	 * Set up any proxy for snews URLs that returns NNTP responses for Lynx
	 * to convert to HTML, instead of doing the conversion itself, and for
	 * handling posts or followups.  - TZ & FM
	 */
	if (!strncasecomp(p1, STR_SNEWS_URL, 6) ||
	    !strncasecomp(p1, "snewspost:", 10) ||
	    !strncasecomp(p1, "snewsreply:", 11)) {
	    StrAllocCopy(ProxyHost, NewsHost);
	    if ((cp = HTParse(p1, "", PARSE_HOST)) != NULL && *cp != '\0') {
		SnipIn2(command, "%s//%.*s", STR_SNEWS_URL, 10, cp);
		StrAllocCopy(NewsHost, cp);
	    } else {
		SnipIn2(command, "%s//%.*s", STR_SNEWS_URL, 10, NewsHost);
	    }
	    command[sizeof(command) - 2] = '\0';
	    FREE(cp);
	    sprintf(proxycmd, "GET %.*s%c%c%c%c",
		    (int) sizeof(proxycmd) - 9, command,
		    CR, LF, CR, LF);
	    CTRACE((tfp, "HTNews: Proxy command is '%.*s'\n",
		    (int) (strlen(proxycmd) - 4), proxycmd));
	    strcat(command, "/");
	    StrAllocCopy(ProxyHREF, NewsHREF);
	    StrAllocCopy(NewsHREF, command);
	    if (spost_wanted || sreply_wanted) {
		/*
		 * Reset p1 so that it points to the newsgroup(s).
		 */
		if ((p1 = strrchr(arg, '/')) != NULL) {
		    p1++;
		} else {
		    p1 = (strrchr(arg, ':') + 1);
		}
	    } else {
		/*
		 * Reset p1 so that it points to the newsgroup (or a wildcard),
		 * or the article.
		 */
		if (!(cp = strrchr((p1 + 6), '/')) || *(cp + 1) == '\0') {
		    p1 = "*";
		    group_wanted = FALSE;
		    list_wanted = TRUE;
		} else {
		    p1 = (cp + 1);
		}
	    }
	}

	/*
	 * Set up command for a post, listing, or article request.  - FM
	 */
	if (post_wanted || reply_wanted || spost_wanted || sreply_wanted) {
	    strcpy(command, "POST");
	} else if (list_wanted) {
	    if (strlen(p1) > 249) {
		FREE(ProxyHost);
		FREE(ProxyHREF);
		HTAlert(URL_TOO_LONG);
		return -400;
	    }
	    SnipIn(command, "XGTITLE %.*s", 11, p1);
	} else if (group_wanted) {
	    char *slash = strchr(p1, '/');

	    first = 0;
	    last = 0;
	    if (slash) {
		*slash = '\0';
		if (strlen(p1) >= sizeof(groupName)) {
		    FREE(ProxyHost);
		    FREE(ProxyHREF);
		    HTAlert(URL_TOO_LONG);
		    return -400;
		}
		LYstrncpy(groupName, p1, sizeof(groupName) - 1);
		*slash = '/';
		(void) sscanf(slash + 1, "%d-%d", &first, &last);
		if ((first > 0) && (isdigit(UCH(*(slash + 1)))) &&
		    (strchr(slash + 1, '-') == NULL || first == last)) {
		    /*
		     * We got a number greater than 0, which will be loaded as
		     * first, and either no range or the range computes to
		     * zero, so make last negative, as a flag to select the
		     * group and then fetch an article by number (first)
		     * instead of by messageID.  - FM
		     */
		    last = -1;
		}
	    } else {
		if (strlen(p1) >= sizeof(groupName)) {
		    FREE(ProxyHost);
		    FREE(ProxyHREF);
		    HTAlert(URL_TOO_LONG);
		    return -400;
		}
		LYstrncpy(groupName, p1, sizeof(groupName) - 1);
	    }
	    SnipIn(command, "GROUP %.*s", 9, groupName);
	} else {
	    int add_open = (strchr(p1, '<') == 0);
	    int add_close = (strchr(p1, '>') == 0);

	    if (strlen(p1) + add_open + add_close >= 252) {
		FREE(ProxyHost);
		FREE(ProxyHREF);
		HTAlert(URL_TOO_LONG);
		return -400;
	    }
	    sprintf(command, "ARTICLE %s%.*s%s",
		    add_open ? "<" : "",
		    (int) (sizeof(command) - (11 + add_open + add_close)),
		    p1,
		    add_close ? ">" : "");
	}

	{
	    char *p = command + strlen(command);

	    /*
	     * Terminate command with CRLF, as in RFC 977.
	     */
	    *p++ = CR;		/* Macros to be correct on Mac */
	    *p++ = LF;
	    *p++ = 0;
	}
	StrAllocCopy(ListArg, p1);
    }				/* scope of p1 */

    if (!*arg) {
	FREE(NewsHREF);
	FREE(ProxyHost);
	FREE(ProxyHREF);
	FREE(ListArg);
	return NO;		/* Ignore if no name */
    }

    if (!(post_wanted || reply_wanted || spost_wanted || sreply_wanted ||
	  (group_wanted && last != -1) || list_wanted)) {
	head_wanted = anAnchor->isHEAD;
	if (head_wanted && !strncmp(command, "ARTICLE ", 8)) {
	    /* overwrite "ARTICLE" - hack... */
	    strcpy(command, "HEAD ");
	    for (cp = command + 5;; cp++)
		if ((*cp = *(cp + 3)) == '\0')
		    break;
	}
	rawtext = (BOOL) (head_wanted || keep_mime_headers);
    }
    if (rawtext) {
	node_anchor = anAnchor;
	rawtarget = HTStreamStack(WWW_PLAINTEXT,
				  format_out,
				  stream, anAnchor);
	if (!rawtarget) {
	    FREE(NewsHost);
	    FREE(NewsHREF);
	    FREE(ProxyHost);
	    FREE(ProxyHREF);
	    FREE(ListArg);
	    HTAlert(gettext("No target for raw text!"));
	    return (HT_NOT_LOADED);
	}			/* Copy routine entry points */
	rawtargetClass = *rawtarget->isa;
    } else
	/*
	 * Make a hypertext object with an anchor list.
	 */
    if (!(post_wanted || reply_wanted || spost_wanted || sreply_wanted)) {
	node_anchor = anAnchor;
	target = HTML_new(anAnchor, format_out, stream);
	targetClass = *target->isa;	/* Copy routine entry points */
    }

    /*
     * Now, let's get a stream setup up from the NewsHost.
     */
    for (retries = 0; retries < 2; retries++) {
	if (s < 0) {
	    /* CONNECTING to news host */
	    char url[260];

	    if (!strcmp(NewsHREF, STR_NEWS_URL)) {
		SnipIn(url, "lose://%.*s/", 9, NewsHost);
	    } else if (ProxyHREF) {
		SnipIn(url, "%.*s", 1, ProxyHREF);
	    } else {
		SnipIn(url, "%.*s", 1, NewsHREF);
	    }
	    CTRACE((tfp, "News: doing HTDoConnect on '%s'\n", url));

	    _HTProgress(gettext("Connecting to NewsHost ..."));

#ifdef USE_SSL
	    if (!using_proxy &&
		(!strncmp(arg, STR_SNEWS_URL, 6) ||
		 !strncmp(arg, "snewspost:", 10) ||
		 !strncmp(arg, "snewsreply:", 11)))
		status = HTDoConnect(url, "NNTPS", SNEWS_PORT, &s);
	    else
		status = HTDoConnect(url, "NNTP", NEWS_PORT, &s);
#else
	    status = HTDoConnect(url, "NNTP", NEWS_PORT, &s);
#endif /* USE_SSL */

	    if (status == HT_INTERRUPTED) {
		/*
		 * Interrupt cleanly.
		 */
		CTRACE((tfp,
			"HTNews: Interrupted on connect; recovering cleanly.\n"));
		_HTProgress(CONNECTION_INTERRUPTED);
		if (!(post_wanted || reply_wanted ||
		      spost_wanted || sreply_wanted)) {
		    ABORT_TARGET;
		}
		FREE(NewsHost);
		FREE(NewsHREF);
		FREE(ProxyHost);
		FREE(ProxyHREF);
		FREE(ListArg);
#ifdef USE_SSL
		if (Handle) {
		    SSL_free(Handle);
		    Handle = NULL;
		}
#endif /* USE_SSL */
		if (postfile) {
		    HTSYS_remove(postfile);
		    FREE(postfile);
		}
		return HT_NOT_LOADED;
	    }
	    if (status < 0) {
		NEWS_NETCLOSE(s);
		s = -1;
		CTRACE((tfp, "HTNews: Unable to connect to news host.\n"));
		if (retries < 1)
		    continue;
		if (!(post_wanted || reply_wanted ||
		      spost_wanted || sreply_wanted)) {
		    ABORT_TARGET;
		}
		HTSprintf0(&dbuf, gettext("Could not access %s."), NewsHost);
		FREE(NewsHost);
		FREE(NewsHREF);
		FREE(ProxyHost);
		FREE(ProxyHREF);
		FREE(ListArg);
		if (postfile) {
		    HTSYS_remove(postfile);
		    FREE(postfile);
		}
		return HTLoadError(stream, 500, dbuf);
	    } else {
		CTRACE((tfp, "HTNews: Connected to news host %s.\n",
			NewsHost));
#ifdef USE_SSL
		/*
		 * If this is an snews url, then do the SSL stuff here
		 */
		if (!using_proxy &&
		    (!strncmp(url, "snews", 5) ||
		     !strncmp(url, "snewspost:", 10) ||
		     !strncmp(url, "snewsreply:", 11))) {
		    Handle = HTGetSSLHandle();
		    SSL_set_fd(Handle, s);
		    HTSSLInitPRNG();
		    status = SSL_connect(Handle);

		    if (status <= 0) {
			unsigned long SSLerror;

			CTRACE((tfp,
				"HTNews: Unable to complete SSL handshake for '%s', SSL_connect=%d, SSL error stack dump follows\n",
				url, status));
			SSL_load_error_strings();
			while ((SSLerror = ERR_get_error()) != 0) {
			    CTRACE((tfp, "HTNews: SSL: %s\n",
				    ERR_error_string(SSLerror, NULL)));
			}
			HTAlert("Unable to make secure connection to remote host.");
			NEWS_NETCLOSE(s);
			s = -1;
			if (!(post_wanted || reply_wanted ||
			      spost_wanted || sreply_wanted))
			    (*targetClass._abort) (target, NULL);
			FREE(NewsHost);
			FREE(NewsHREF);
			FREE(ProxyHost);
			FREE(ProxyHREF);
			FREE(ListArg);
			if (postfile) {
#ifdef VMS
			    while (remove(postfile) == 0) ;	/* loop through all versions */
#else
			    remove(postfile);
#endif /* VMS */
			    FREE(postfile);
			}
			return HT_NOT_LOADED;
		    }
		    sprintf(SSLprogress,
			    "Secure %d-bit %s (%s) NNTP connection",
			    SSL_get_cipher_bits(Handle, NULL),
			    SSL_get_cipher_version(Handle),
			    SSL_get_cipher(Handle));
		    _HTProgress(SSLprogress);
		}
#endif /* USE_SSL */
		HTInitInput(s);	/* set up buffering */
		if (proxycmd[0]) {
		    status = NEWS_NETWRITE(s, proxycmd, strlen(proxycmd));
		    CTRACE((tfp,
			    "HTNews: Proxy command returned status '%d'.\n",
			    status));
		}
		if (((status = response(NULL)) / 100) != 2) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		    if (status == HT_INTERRUPTED) {
			_HTProgress(CONNECTION_INTERRUPTED);
			if (!(post_wanted || reply_wanted ||
			      spost_wanted || sreply_wanted)) {
			    ABORT_TARGET;
			}
			FREE(NewsHost);
			FREE(NewsHREF);
			FREE(ProxyHost);
			FREE(ProxyHREF);
			FREE(ListArg);
			if (postfile) {
			    HTSYS_remove(postfile);
			    FREE(postfile);
			}
			return (HT_NOT_LOADED);
		    }
		    if (retries < 1)
			continue;
		    FREE(ProxyHost);
		    FREE(ProxyHREF);
		    FREE(ListArg);
		    FREE(postfile);
		    if (!(post_wanted || reply_wanted ||
			  spost_wanted || sreply_wanted)) {
			ABORT_TARGET;
		    }
		    if (response_text[0]) {
			HTSprintf0(&dbuf,
				   gettext("Can't read news info.  News host %.20s responded: %.200s"),
				   NewsHost, response_text);
		    } else {
			HTSprintf0(&dbuf,
				   gettext("Can't read news info, empty response from host %s"),
				   NewsHost);
		    }
		    return HTLoadError(stream, 500, dbuf);
		}
		if (status == 200) {
		    HTCanPost = TRUE;
		} else {
		    HTCanPost = FALSE;
		    if (post_wanted || reply_wanted ||
			spost_wanted || sreply_wanted) {
			HTAlert(CANNOT_POST);
			FREE(NewsHREF);
			if (ProxyHREF) {
			    StrAllocCopy(NewsHost, ProxyHost);
			    FREE(ProxyHost);
			    FREE(ProxyHREF);
			}
			FREE(ListArg);
			if (postfile) {
			    HTSYS_remove(postfile);
			    FREE(postfile);
			}
			return (HT_NOT_LOADED);
		    }
		}
	    }
	}
	/* If needed opening */
	if (post_wanted || reply_wanted ||
	    spost_wanted || sreply_wanted) {
	    if (!HTCanPost) {
		HTAlert(CANNOT_POST);
		FREE(NewsHREF);
		if (ProxyHREF) {
		    StrAllocCopy(NewsHost, ProxyHost);
		    FREE(ProxyHost);
		    FREE(ProxyHREF);
		}
		FREE(ListArg);
		if (postfile) {
		    HTSYS_remove(postfile);
		    FREE(postfile);
		}
		return (HT_NOT_LOADED);
	    }
	    if (postfile == NULL) {
		postfile = LYNewsPost(ListArg,
				      (BOOLEAN) (reply_wanted || sreply_wanted));
	    }
	    if (postfile == NULL) {
		HTProgress(CANCELLED);
		FREE(NewsHREF);
		if (ProxyHREF) {
		    StrAllocCopy(NewsHost, ProxyHost);
		    FREE(ProxyHost);
		    FREE(ProxyHREF);
		}
		FREE(ListArg);
		return (HT_NOT_LOADED);
	    }
	} else {
	    /*
	     * Ensure reader mode, but don't bother checking the status for
	     * anything but HT_INTERRUPTED or a 480 Authorization request,
	     * because if the reader mode command is not needed, the server
	     * probably returned a 500, which is irrelevant at this point.  -
	     * FM
	     */
	    char buffer[20];

	    sprintf(buffer, "mode reader%c%c", CR, LF);
	    if ((status = response(buffer)) == HT_INTERRUPTED) {
		_HTProgress(CONNECTION_INTERRUPTED);
		break;
	    }
	    if (status == 480) {
		NNTPAuthResult auth_result = HTHandleAuthInfo(NewsHost);

		if (auth_result == NNTPAUTH_CLOSE) {
		    if (s != -1 && !(ProxyHost || ProxyHREF)) {
			NEWS_NETCLOSE(s);
			s = -1;
		    }
		}
		if (auth_result != NNTPAUTH_OK) {
		    break;
		}
		if ((status = response(buffer)) == HT_INTERRUPTED) {
		    _HTProgress(CONNECTION_INTERRUPTED);
		    break;
		}
	    }
	}

      Send_NNTP_command:
#ifdef NEWS_DEB
	if (postfile)
	    printf("postfile = %s, command = %s", postfile, command);
	else
	    printf("command = %s", command);
#endif
	if ((status = response(command)) == HT_INTERRUPTED) {
	    _HTProgress(CONNECTION_INTERRUPTED);
	    break;
	}
	if (status < 0) {
	    if (retries < 1) {
		continue;
	    } else {
		break;
	    }
	}
	/*
	 * For some well known error responses which are expected to occur in
	 * normal use, break from the loop without retrying and without closing
	 * the connection.  It is unlikely that these are leftovers from a
	 * timed-out connection (but we do some checks to see whether the
	 * response corresponds to the last command), or that they will give
	 * anything else when automatically retried.  - kw
	 */
	if (status == 411 && group_wanted &&
	    !strncmp(command, "GROUP ", 6) &&
	    !strncasecomp(response_text + 3, " No such group ", 15) &&
	    !strcmp(response_text + 18, groupName)) {

	    HTAlert(response_text);
	    break;
	} else if (status == 430 && !group_wanted && !list_wanted &&
		   !strncmp(command, "ARTICLE <", 9) &&
		   !strcasecomp(response_text + 3, " No such article")) {

	    HTAlert(response_text);
	    break;
	}
	if ((status / 100) != 2 &&
	    status != 340 &&
	    status != 480) {
	    if (retries) {
		if (list_wanted && !strncmp(command, "XGTITLE", 7)) {
		    sprintf(command, "LIST NEWSGROUPS%c%c", CR, LF);
		    goto Send_NNTP_command;
		}
		HTAlert(response_text);
	    } else {
		_HTProgress(response_text);
	    }
	    NEWS_NETCLOSE(s);
	    s = -1;
	    /*
	     * Message might be a leftover "Timeout-disconnected", so try again
	     * if the retries maximum has not been reached.
	     */
	    continue;
	}

	/*
	 * Post or load a group, article, etc
	 */
	if (status == 480) {
	    NNTPAuthResult auth_result;

	    /*
	     * Some servers return 480 for a failed XGTITLE.  - FM
	     */
	    if (list_wanted && !strncmp(command, "XGTITLE", 7) &&
		strstr(response_text, "uthenticat") == NULL &&
		strstr(response_text, "uthor") == NULL) {
		sprintf(command, "LIST NEWSGROUPS%c%c", CR, LF);
		goto Send_NNTP_command;
	    }
	    /*
	     * Handle Authorization.  - FM
	     */
	    if ((auth_result = HTHandleAuthInfo(NewsHost)) == NNTPAUTH_OK) {
		goto Send_NNTP_command;
	    } else if (auth_result == NNTPAUTH_CLOSE) {
		if (s != -1 && !(ProxyHost || ProxyHREF)) {
		    NEWS_NETCLOSE(s);
		    s = -1;
		}
		if (retries < 1)
		    continue;
	    }
	    status = HT_NOT_LOADED;
	} else if (post_wanted || reply_wanted ||
		   spost_wanted || sreply_wanted) {
	    /*
	     * Handle posting of an article.  - FM
	     */
	    if (status != 340) {
		HTAlert(CANNOT_POST);
		if (postfile) {
		    HTSYS_remove(postfile);
		}
	    } else {
		post_article(postfile);
	    }
	    FREE(postfile);
	    status = HT_NOT_LOADED;
	} else if (list_wanted) {
	    /*
	     * List available newsgroups.  - FM
	     */
	    _HTProgress(gettext("Reading list of available newsgroups."));
	    status = read_list(ListArg);
	} else if (group_wanted) {
	    /*
	     * List articles in a news group.  - FM
	     */
	    if (last < 0) {
		/*
		 * We got one article number rather than a range following the
		 * slash which followed the group name, or the range was zero,
		 * so now that we have selected that group, load ARTICLE and
		 * the the number (first) as the command and go back to send it
		 * and check the response.  - FM
		 */
		sprintf(command, "%s %d%c%c",
			head_wanted ? "HEAD" : "ARTICLE",
			first, CR, LF);
		group_wanted = FALSE;
		retries = 2;
		goto Send_NNTP_command;
	    }
	    _HTProgress(gettext("Reading list of articles in newsgroup."));
	    status = read_group(groupName, first, last);
	} else {
	    /*
	     * Get an article from a news group.  - FM
	     */
	    _HTProgress(gettext("Reading news article."));
	    status = read_article(anAnchor);
	}
	if (status == HT_INTERRUPTED) {
	    _HTProgress(CONNECTION_INTERRUPTED);
	    status = HT_LOADED;
	}
	if (!(post_wanted || reply_wanted ||
	      spost_wanted || sreply_wanted)) {
	    if (status == HT_NOT_LOADED) {
		ABORT_TARGET;
	    } else {
		FREE_TARGET;
	    }
	}
	FREE(NewsHREF);
	if (ProxyHREF) {
	    StrAllocCopy(NewsHost, ProxyHost);
	    FREE(ProxyHost);
	    FREE(ProxyHREF);
	}
	FREE(ListArg);
	if (postfile) {
	    HTSYS_remove(postfile);
	    FREE(postfile);
	}
	return status;
    }				/* Retry loop */

#if 0
    HTAlert(gettext("Sorry, could not load requested news."));
    NXRunAlertPanel(NULL, "Sorry, could not load `%s'.", NULL, NULL, NULL, arg);
    /* No -- message earlier wil have covered it */
#endif

    if (!(post_wanted || reply_wanted ||
	  spost_wanted || sreply_wanted)) {
	ABORT_TARGET;
    }
    FREE(NewsHREF);
    if (ProxyHREF) {
	StrAllocCopy(NewsHost, ProxyHost);
	FREE(ProxyHost);
	FREE(ProxyHREF);
    }
    FREE(ListArg);
    if (postfile) {
	HTSYS_remove(postfile);
	FREE(postfile);
    }
    return HT_NOT_LOADED;
}

/*
 *  This function clears all authorization information by
 *  invoking the free_NNTP_AuthInfo() function, which normally
 *  is invoked at exit.  It allows a browser command to do
 *  this at any time, for example, if the user is leaving
 *  the terminal for a period of time, but does not want
 *  to end the current session.  - FM
 */
void HTClearNNTPAuthInfo(void)
{
    /*
     * Need code to check cached documents and do something to ensure that any
     * protected documents no longer can be accessed without a new retrieval. 
     * - FM
     */

    /*
     * Now free all of the authorization info.  - FM
     */
    free_NNTP_AuthInfo();
}

#ifdef USE_SSL
static char HTNewsGetCharacter(void)
{
    if (!Handle)
	return HTGetCharacter();
    else
	return HTGetSSLCharacter((void *) Handle);
}

int HTNewsProxyConnect(int sock,
		       const char *url,
		       HTParentAnchor *anAnchor,
		       HTFormat format_out,
		       HTStream *sink)
{
    int status;
    const char *arg = url;
    char SSLprogress[256];

    s = channel_s = sock;
    Handle = HTGetSSLHandle();
    SSL_set_fd(Handle, s);
    HTSSLInitPRNG();
    status = SSL_connect(Handle);

    if (status <= 0) {
	unsigned long SSLerror;

	channel_s = -1;
	CTRACE((tfp,
		"HTNews: Unable to complete SSL handshake for '%s', SSL_connect=%d, SSL error stack dump follows\n",
		url, status));
	SSL_load_error_strings();
	while ((SSLerror = ERR_get_error()) != 0) {
	    CTRACE((tfp, "HTNews: SSL: %s\n", ERR_error_string(SSLerror, NULL)));
	}
	HTAlert("Unable to make secure connection to remote host.");
	NEWS_NETCLOSE(s);
	s = -1;
	return HT_NOT_LOADED;
    }
    sprintf(SSLprogress, "Secure %d-bit %s (%s) NNTP connection",
	    SSL_get_cipher_bits(Handle, NULL),
	    SSL_get_cipher_version(Handle),
	    SSL_get_cipher(Handle));
    _HTProgress(SSLprogress);
    status = HTLoadNews(arg, anAnchor, format_out, sink);
    channel_s = -1;
    return status;
}
#endif /* USE_SSL */

#ifdef GLOBALDEF_IS_MACRO
#define _HTNEWS_C_1_INIT { "news", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTNews, _HTNEWS_C_1_INIT);
#define _HTNEWS_C_2_INIT { "nntp", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTNNTP, _HTNEWS_C_2_INIT);
#define _HTNEWS_C_3_INIT { "newspost", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTNewsPost, _HTNEWS_C_3_INIT);
#define _HTNEWS_C_4_INIT { "newsreply", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTNewsReply, _HTNEWS_C_4_INIT);
#define _HTNEWS_C_5_INIT { "snews", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTSNews, _HTNEWS_C_5_INIT);
#define _HTNEWS_C_6_INIT { "snewspost", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTSNewsPost, _HTNEWS_C_6_INIT);
#define _HTNEWS_C_7_INIT { "snewsreply", HTLoadNews, NULL }
GLOBALDEF(HTProtocol, HTSNewsReply, _HTNEWS_C_7_INIT);
#else
GLOBALDEF HTProtocol HTNews =
{"news", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTNNTP =
{"nntp", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTNewsPost =
{"newspost", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTNewsReply =
{"newsreply", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTSNews =
{"snews", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTSNewsPost =
{"snewspost", HTLoadNews, NULL};
GLOBALDEF HTProtocol HTSNewsReply =
{"snewsreply", HTLoadNews, NULL};
#endif /* GLOBALDEF_IS_MACRO */

#endif /* not DISABLE_NEWS */
