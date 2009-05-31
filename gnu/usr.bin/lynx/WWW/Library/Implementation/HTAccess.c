/*		Access Manager					HTAccess.c
 *		==============
 *
 *  Authors
 *	TBL	Tim Berners-Lee timbl@info.cern.ch
 *	JFG	Jean-Francois Groff jfg@dxcern.cern.ch
 *	DD	Denis DeLaRoca (310) 825-4580  <CSP1DWD@mvs.oac.ucla.edu>
 *	FM	Foteos Macrides macrides@sci.wfeb.edu
 *	PDM	Danny Mayer mayer@ljo.dec.com
 *
 *  History
 *	 8 Jun 92 Telnet hopping prohibited as telnet is not secure TBL
 *	26 Jun 92 When over DECnet, suppressed FTP, Gopher and News. JFG
 *	 6 Oct 92 Moved HTClientHost and logfile into here. TBL
 *	17 Dec 92 Tn3270 added, bug fix. DD
 *	 4 Feb 93 Access registration, Search escapes bad chars TBL
 *		  PARAMETERS TO HTSEARCH AND HTLOADRELATIVE CHANGED
 *	28 May 93 WAIS gateway explicit if no WAIS library linked in.
 *	31 May 94 Added DIRECT_WAIS support for VMS. FM
 *	27 Jan 95 Fixed proxy support to use NNTPSERVER for checking
 *		  whether or not to use the proxy server. PDM
 *	27 Jan 95 Ensured that proxy service will be overridden for files
 *		  on the local host (because HTLoadFile() doesn't try ftp
 *		  for those) and will substitute ftp for remote files. FM
 *	28 Jan 95 Tweaked PDM's proxy override mods to handle port info
 *		  for news and wais URL's. FM
 *
 *  Bugs
 *	This module assumes that that the graphic object is hypertext, as it
 *	needs to select it when it has been loaded.  A superclass needs to be
 *	defined which accepts select and select_anchor.
 */

#ifdef VMS
#define DIRECT_WAIS
#endif /* VMS */

#include <HTUtils.h>
#include <HTTP.h>
#include <HTAlert.h>
/*
 *  Implements:
 */
#include <HTAccess.h>

/*
 *  Uses:
 */
#include <HTParse.h>
#include <HTML.h>		/* SCW */

#ifndef NO_RULES
#include <HTRules.h>
#endif

#include <HTList.h>
#include <HText.h>		/* See bugs above */
#include <HTCJK.h>
#include <UCMap.h>
#include <GridText.h>

#include <LYGlobalDefs.h>
#include <LYexit.h>
#include <LYUtils.h>
#include <LYLeaks.h>

/*
 *  These flags may be set to modify the operation of this module
 */
char *HTClientHost = NULL;	/* Name of remote login host if any */
FILE *HTlogfile = NULL;		/* File to which to output one-liners */
BOOL HTSecure = NO;		/* Disable access for telnet users? */
BOOL HTPermitRedir = NO;	/* Always allow redirection in getfile()? */

BOOL using_proxy = NO;		/* are we using a proxy gateway? */

/*
 *  To generate other things, play with these:
 */
HTFormat HTOutputFormat = NULL;
HTStream *HTOutputStream = NULL;	/* For non-interactive, set this */

static HTList *protocols = NULL;	/* List of registered protocol descriptors */

char *use_this_url_instead = NULL;

static int pushed_assume_LYhndl = -1;	/* see LYUC* functions below - kw */
static char *pushed_assume_MIMEname = NULL;

#ifdef LY_FIND_LEAKS
static void free_protocols(void)
{
    HTList_delete(protocols);
    protocols = NULL;
    FREE(pushed_assume_MIMEname);	/* shouldn't happen, just in case - kw */
}
#endif /* LY_FIND_LEAKS */

/*	Register a Protocol.				HTRegisterProtocol()
 *	--------------------
 */
BOOL HTRegisterProtocol(HTProtocol * protocol)
{
    if (!protocols) {
	protocols = HTList_new();
#ifdef LY_FIND_LEAKS
	atexit(free_protocols);
#endif
    }
    HTList_addObject(protocols, protocol);
    return YES;
}

/*	Register all known protocols.			HTAccessInit()
 *	-----------------------------
 *
 *	Add to or subtract from this list if you add or remove protocol
 *	modules.  This routine is called the first time the protocol list
 *	is needed, unless any protocols are already registered, in which
 *	case it is not called.	Therefore the application can override
 *	this list.
 *
 *	Compiling with NO_INIT prevents all known protocols from being
 *	forced in at link time.
 */
#ifndef NO_INIT
#ifdef GLOBALREF_IS_MACRO
extern GLOBALREF (HTProtocol, HTTP);
extern GLOBALREF (HTProtocol, HTTPS);
extern GLOBALREF (HTProtocol, HTFile);
extern GLOBALREF (HTProtocol, HTTelnet);
extern GLOBALREF (HTProtocol, HTTn3270);
extern GLOBALREF (HTProtocol, HTRlogin);

#ifndef DECNET
#ifndef DISABLE_FTP
extern GLOBALREF (HTProtocol, HTFTP);
#endif /* DISABLE_FTP */
#ifndef DISABLE_NEWS
extern GLOBALREF (HTProtocol, HTNews);
extern GLOBALREF (HTProtocol, HTNNTP);
extern GLOBALREF (HTProtocol, HTNewsPost);
extern GLOBALREF (HTProtocol, HTNewsReply);
extern GLOBALREF (HTProtocol, HTSNews);
extern GLOBALREF (HTProtocol, HTSNewsPost);
extern GLOBALREF (HTProtocol, HTSNewsReply);
#endif /* not DISABLE_NEWS */
#ifndef DISABLE_GOPHER
extern GLOBALREF (HTProtocol, HTGopher);
extern GLOBALREF (HTProtocol, HTCSO);
#endif /* not DISABLE_GOPHER */
#ifndef DISABLE_FINGER
extern GLOBALREF (HTProtocol, HTFinger);
#endif /* not DISABLE_FINGER */
#ifdef DIRECT_WAIS
extern GLOBALREF (HTProtocol, HTWAIS);
#endif /* DIRECT_WAIS */
#endif /* !DECNET */
#else
GLOBALREF HTProtocol HTTP, HTTPS, HTFile, HTTelnet, HTTn3270, HTRlogin;

#ifndef DECNET
#ifndef DISABLE_FTP
GLOBALREF HTProtocol HTFTP;
#endif /* DISABLE_FTP */
#ifndef DISABLE_NEWS
GLOBALREF HTProtocol HTNews, HTNNTP, HTNewsPost, HTNewsReply;
GLOBALREF HTProtocol HTSNews, HTSNewsPost, HTSNewsReply;
#endif /* not DISABLE_NEWS */
#ifndef DISABLE_GOPHER
GLOBALREF HTProtocol HTGopher, HTCSO;
#endif /* not DISABLE_GOPHER */
#ifndef DISABLE_FINGER
GLOBALREF HTProtocol HTFinger;
#endif /* not DISABLE_FINGER */
#ifdef DIRECT_WAIS
GLOBALREF HTProtocol HTWAIS;
#endif /* DIRECT_WAIS */
#endif /* !DECNET */
#endif /* GLOBALREF_IS_MACRO */

static void HTAccessInit(void)	/* Call me once */
{
    HTRegisterProtocol(&HTTP);
    HTRegisterProtocol(&HTTPS);
    HTRegisterProtocol(&HTFile);
    HTRegisterProtocol(&HTTelnet);
    HTRegisterProtocol(&HTTn3270);
    HTRegisterProtocol(&HTRlogin);
#ifndef DECNET
#ifndef DISABLE_FTP
    HTRegisterProtocol(&HTFTP);
#endif /* DISABLE_FTP */
#ifndef DISABLE_NEWS
    HTRegisterProtocol(&HTNews);
    HTRegisterProtocol(&HTNNTP);
    HTRegisterProtocol(&HTNewsPost);
    HTRegisterProtocol(&HTNewsReply);
    HTRegisterProtocol(&HTSNews);
    HTRegisterProtocol(&HTSNewsPost);
    HTRegisterProtocol(&HTSNewsReply);
#endif /* not DISABLE_NEWS */
#ifndef DISABLE_GOPHER
    HTRegisterProtocol(&HTGopher);
    HTRegisterProtocol(&HTCSO);
#endif /* not DISABLE_GOPHER */
#ifndef DISABLE_FINGER
    HTRegisterProtocol(&HTFinger);
#endif /* not DISABLE_FINGER */
#ifdef DIRECT_WAIS
    HTRegisterProtocol(&HTWAIS);
#endif /* DIRECT_WAIS */
#endif /* !DECNET */
    LYRegisterLynxProtocols();
}
#endif /* !NO_INIT */

/*	Check for proxy override.			override_proxy()
 *	-------------------------
 *
 *	Check the no_proxy environment variable to get the list
 *	of hosts for which proxy server is not consulted.
 *
 *	no_proxy is a comma- or space-separated list of machine
 *	or domain names, with optional :port part.  If no :port
 *	part is present, it applies to all ports on that domain.
 *
 *  Example:
 *	    no_proxy="cern.ch,some.domain:8001"
 *
 *  Use "*" to override all proxy service:
 *	     no_proxy="*"
 */
BOOL override_proxy(const char *addr)
{
    const char *no_proxy = getenv("no_proxy");
    char *p = NULL;
    char *at = NULL;
    char *host = NULL;
    char *Host = NULL;
    char *acc_method = NULL;
    int port = 0;
    int h_len = 0;

    /*
     * Check for global override.
     */
    if (no_proxy) {
	if (!strcmp(no_proxy, "*"))
	    return YES;
    }

    /*
     * Never proxy file:// URLs if they are on the local host.  HTLoadFile()
     * will not attempt ftp for those if direct access fails.  We'll check that
     * first, in case no_proxy hasn't been defined.  - FM
     */
    if (!addr)
	return NO;
    if (!(host = HTParse(addr, "", PARSE_HOST)))
	return NO;
    if (!*host) {
	FREE(host);
	return NO;
    }
    Host = (((at = strchr(host, '@')) != NULL) ? (at + 1) : host);

    if ((acc_method = HTParse(addr, "", PARSE_ACCESS))) {
	if (!strcmp("file", acc_method) &&
	    (LYSameHostname(Host, "localhost") ||
	     LYSameHostname(Host, HTHostName()))) {
	    FREE(host);
	    FREE(acc_method);
	    return YES;
	}
	FREE(acc_method);
    }

    if (!no_proxy) {
	FREE(host);
	return NO;
    }

    if (NULL != (p = strrchr(Host, ':'))) {	/* Port specified */
	*p++ = 0;		/* Chop off port */
	port = atoi(p);
    } else {			/* Use default port */
	acc_method = HTParse(addr, "", PARSE_ACCESS);
	if (acc_method != NULL) {
	    /* *INDENT-OFF* */
	    if	    (!strcmp(acc_method, "http"))	port = 80;
	    else if (!strcmp(acc_method, "https"))	port = 443;
	    else if (!strcmp(acc_method, "ftp"))	port = 21;
#ifndef DISABLE_GOPHER
	    else if (!strcmp(acc_method, "gopher"))	port = 70;
#endif
	    else if (!strcmp(acc_method, "cso"))	port = 105;
#ifndef DISABLE_NEWS
	    else if (!strcmp(acc_method, "news"))	port = 119;
	    else if (!strcmp(acc_method, "nntp"))	port = 119;
	    else if (!strcmp(acc_method, "newspost"))	port = 119;
	    else if (!strcmp(acc_method, "newsreply"))	port = 119;
	    else if (!strcmp(acc_method, "snews"))	port = 563;
	    else if (!strcmp(acc_method, "snewspost"))	port = 563;
	    else if (!strcmp(acc_method, "snewsreply")) port = 563;
#endif
	    else if (!strcmp(acc_method, "wais"))	port = 210;
#ifndef DISABLE_FINGER
	    else if (!strcmp(acc_method, "finger"))	port = 79;
#endif
	    else if (!strcmp(acc_method, "telnet"))	port = 23;
	    else if (!strcmp(acc_method, "tn3270"))	port = 23;
	    else if (!strcmp(acc_method, "rlogin"))	port = 513;
	    /* *INDENT-ON* */

	    FREE(acc_method);
	}
    }
    if (!port)
	port = 80;		/* Default */
    h_len = strlen(Host);

    while (*no_proxy) {
	const char *end;
	const char *colon = NULL;
	int templ_port = 0;
	int t_len;

	while (*no_proxy && (WHITE(*no_proxy) || *no_proxy == ','))
	    no_proxy++;		/* Skip whitespace and separators */

	end = no_proxy;
	while (*end && !WHITE(*end) && *end != ',') {	/* Find separator */
	    if (*end == ':')
		colon = end;	/* Port number given */
	    end++;
	}

	if (colon) {
	    templ_port = atoi(colon + 1);
	    t_len = colon - no_proxy;
	} else {
	    t_len = end - no_proxy;
	}

	if ((!templ_port || templ_port == port) &&
	    (t_len > 0 && t_len <= h_len &&
	     !strncasecomp(Host + h_len - t_len, no_proxy, t_len))) {
	    FREE(host);
	    return YES;
	}
#ifdef CJK_EX			/* ASATAKU PROXY HACK */
	if ((!templ_port || templ_port == port) &&
	    (t_len > 0 && t_len <= h_len &&
	     isdigit(UCH(*no_proxy)) && !strncmp(host, no_proxy, t_len))) {
	    FREE(host);
	    return YES;
	}
#endif /* ASATAKU PROXY HACK */

	if (*end)
	    no_proxy = (end + 1);
	else
	    break;
    }

    FREE(host);
    return NO;
}

/*	Find physical name and access protocol		get_physical()
 *	--------------------------------------
 *
 *  On entry,
 *	addr		must point to the fully qualified hypertext reference.
 *	anchor		a parent anchor with whose address is addr
 *
 *  On exit,
 *	returns		HT_NO_ACCESS		Error has occurred.
 *			HT_OK			Success
 */
static int get_physical(const char *addr,
			HTParentAnchor *anchor)
{
    int result;
    char *acc_method = NULL;	/* Name of access method */
    char *physical = NULL;
    char *Server_addr = NULL;
    BOOL override_flag = NO;

    CTRACE((tfp, "get_physical %s\n", addr));

    /*
     * Make sure the using_proxy variable is FALSE.
     */
    using_proxy = NO;

#ifndef NO_RULES
    if ((physical = HTTranslate(addr)) == 0) {
	if (redirecting_url) {
	    return HT_REDIRECTING;
	}
	return HT_FORBIDDEN;
    }
    if (anchor->isISMAPScript == TRUE) {
	StrAllocCat(physical, "?0,0");
	CTRACE((tfp, "HTAccess: Appending '?0,0' coordinate pair.\n"));
    }
    if (!strncmp(physical, "Proxied=", 8)) {
	HTAnchor_setPhysical(anchor, physical + 8);
	using_proxy = YES;
    } else if (!strncmp(physical, "NoProxy=", 8)) {
	HTAnchor_setPhysical(anchor, physical + 8);
	override_flag = YES;
    } else {
	HTAnchor_setPhysical(anchor, physical);
    }
    FREE(physical);		/* free our copy */
#else
    if (anchor->isISMAPScript == TRUE) {
	StrAllocCopy(physical, addr);
	StrAllocCat(physical, "?0,0");
	CTRACE((tfp, "HTAccess: Appending '?0,0' coordinate pair.\n"));
	HTAnchor_setPhysical(anchor, physical);
	FREE(physical);		/* free our copy */
    } else {
	HTAnchor_setPhysical(anchor, addr);
    }
#endif /* NO_RULES */

    acc_method = HTParse(HTAnchor_physical(anchor), STR_FILE_URL, PARSE_ACCESS);

    /*
     * Check whether gateway access has been set up for this.
     *
     * This function can be replaced by the rule system above.
     *
     * If the rule system has already determined that we should use a proxy, or
     * that we shouldn't, ignore proxy-related settings, don't use no_proxy
     * either.
     */
#define USE_GATEWAYS
#ifdef USE_GATEWAYS

    if (!override_flag && !using_proxy) {	/* else ignore no_proxy env var */
	if (!strcasecomp(acc_method, "news")) {
	    /*
	     * News is different, so we need to check the name of the server,
	     * as well as the default port for selective exclusions.
	     */
	    char *host = NULL;

	    if ((host = HTParse(addr, "", PARSE_HOST))) {
		if (strchr(host, ':') == NULL) {
		    StrAllocCopy(Server_addr, "news://");
		    StrAllocCat(Server_addr, host);
		    StrAllocCat(Server_addr, ":119/");
		}
		FREE(host);
	    } else if (LYGetEnv("NNTPSERVER") != NULL) {
		StrAllocCopy(Server_addr, "news://");
		StrAllocCat(Server_addr, LYGetEnv("NNTPSERVER"));
		StrAllocCat(Server_addr, ":119/");
	    }
	} else if (!strcasecomp(acc_method, "wais")) {
	    /*
	     * Wais also needs checking of the default port for selective
	     * exclusions.
	     */
	    char *host = NULL;

	    if ((host = HTParse(addr, "", PARSE_HOST))) {
		if (!(strchr(host, ':'))) {
		    StrAllocCopy(Server_addr, "wais://");
		    StrAllocCat(Server_addr, host);
		    StrAllocCat(Server_addr, ":210/");
		}
		FREE(host);
	    } else
		StrAllocCopy(Server_addr, addr);
	} else {
	    StrAllocCopy(Server_addr, addr);
	}
	override_flag = override_proxy(Server_addr);
    }

    if (!override_flag && !using_proxy) {
	char *gateway_parameter = NULL, *gateway, *proxy;

	/*
	 * Search for gateways.
	 */
	HTSprintf0(&gateway_parameter, "WWW_%s_GATEWAY", acc_method);
	gateway = LYGetEnv(gateway_parameter);	/* coerce for decstation */

	/*
	 * Search for proxy servers.
	 */
	if (!strcmp(acc_method, "file"))
	    /*
	     * If we got to here, a file URL is for ftp on a remote host. - FM
	     */
	    strcpy(gateway_parameter, "ftp_proxy");
	else
	    sprintf(gateway_parameter, "%s_proxy", acc_method);
	proxy = LYGetEnv(gateway_parameter);
	FREE(gateway_parameter);

	if (gateway)
	    CTRACE((tfp, "Gateway found: %s\n", gateway));
	if (proxy)
	    CTRACE((tfp, "proxy server found: %s\n", proxy));

	/*
	 * Proxy servers have precedence over gateway servers.
	 */
	if (proxy) {
	    char *gatewayed = NULL;

	    StrAllocCopy(gatewayed, proxy);
	    if (!strncmp(gatewayed, "http", 4)) {
		char *cp = strrchr(gatewayed, '/');

		/* Append a slash to the proxy specification if it doesn't
		 * end in one but otherwise looks normal (starts with "http",
		 * has no '/' other than ones before the hostname). - kw */
		if (cp && (cp - gatewayed) <= 7)
		    LYAddHtmlSep(&gatewayed);
	    }
	    /*
	     * Ensure that the proxy server uses ftp for file URLs. - FM
	     */
	    if (!strncmp(addr, "file", 4)) {
		StrAllocCat(gatewayed, "ftp");
		StrAllocCat(gatewayed, (addr + 4));
	    } else
		StrAllocCat(gatewayed, addr);
	    using_proxy = YES;
	    if (anchor->isISMAPScript == TRUE)
		StrAllocCat(gatewayed, "?0,0");
	    HTAnchor_setPhysical(anchor, gatewayed);
	    FREE(gatewayed);
	    FREE(acc_method);

	    acc_method = HTParse(HTAnchor_physical(anchor),
				 STR_HTTP_URL, PARSE_ACCESS);

	} else if (gateway) {
	    char *path = HTParse(addr, "",
				 PARSE_HOST + PARSE_PATH + PARSE_PUNCTUATION);

	    /* Chop leading / off to make host into part of path */
	    char *gatewayed = HTParse(path + 1, gateway, PARSE_ALL);

	    FREE(path);
	    HTAnchor_setPhysical(anchor, gatewayed);
	    FREE(gatewayed);
	    FREE(acc_method);

	    acc_method = HTParse(HTAnchor_physical(anchor),
				 STR_HTTP_URL, PARSE_ACCESS);
	}
    }
    FREE(Server_addr);
#endif /* use gateways */

    /*
     * Search registered protocols to find suitable one.
     */
    result = HT_NO_ACCESS;
    {
	int i, n;

#ifndef NO_INIT
	if (!protocols)
	    HTAccessInit();
#endif
	n = HTList_count(protocols);
	for (i = 0; i < n; i++) {
	    HTProtocol *p = (HTProtocol *) HTList_objectAt(protocols, i);

	    if (!strcmp(p->name, acc_method)) {
		HTAnchor_setProtocol(anchor, p);
		FREE(acc_method);
		result = HT_OK;
		break;
	    }
	}
    }

    FREE(acc_method);
    return result;
}

/*
 * Temporarily set the int UCLYhndl_for_unspec and string UCLYhndl_for_unspec
 * used for charset "assuming" to the values implied by a HTParentAnchor's
 * UCStages, after saving the current values for later restoration.  - kw @@@
 * These functions may not really belong here, but where else?  I want the
 * "pop" to occur as soon as possible after loading has finished.  - kw @@@
 */
void LYUCPushAssumed(HTParentAnchor *anchor)
{
    int anchor_LYhndl = -1;
    LYUCcharset *anchor_UCI = NULL;

    if (anchor) {
	anchor_LYhndl = HTAnchor_getUCLYhndl(anchor, UCT_STAGE_PARSER);
	if (anchor_LYhndl >= 0)
	    anchor_UCI = HTAnchor_getUCInfoStage(anchor,
						 UCT_STAGE_PARSER);
	if (anchor_UCI && anchor_UCI->MIMEname) {
	    pushed_assume_MIMEname = UCAssume_MIMEcharset;
	    UCAssume_MIMEcharset = NULL;
	    if (HTCJK == JAPANESE)
		StrAllocCopy(UCAssume_MIMEcharset, pushed_assume_MIMEname);
	    else
		StrAllocCopy(UCAssume_MIMEcharset, anchor_UCI->MIMEname);
	    pushed_assume_LYhndl = anchor_LYhndl;
	    /* some diagnostics */
	    if (UCLYhndl_for_unspec != anchor_LYhndl)
		CTRACE((tfp,
			"LYUCPushAssumed: UCLYhndl_for_unspec changed %d -> %d\n",
			UCLYhndl_for_unspec,
			anchor_LYhndl));
	    UCLYhndl_for_unspec = anchor_LYhndl;
	    return;
	}
    }
    pushed_assume_LYhndl = -1;
    FREE(pushed_assume_MIMEname);
}

/*
 * Restore the int UCLYhndl_for_unspec and string UCLYhndl_for_unspec used for
 * charset "assuming" from the values saved by LYUCPushAssumed, if any.  - kw
 */
int LYUCPopAssumed(void)
{
    if (pushed_assume_LYhndl >= 0) {
	/* some diagnostics */
	if (UCLYhndl_for_unspec != pushed_assume_LYhndl)
	    CTRACE((tfp,
		    "LYUCPopAssumed: UCLYhndl_for_unspec changed %d -> %d\n",
		    UCLYhndl_for_unspec,
		    pushed_assume_LYhndl));
	UCLYhndl_for_unspec = pushed_assume_LYhndl;
	pushed_assume_LYhndl = -1;
	FREE(UCAssume_MIMEcharset);
	UCAssume_MIMEcharset = pushed_assume_MIMEname;
	pushed_assume_MIMEname = NULL;
	return UCLYhndl_for_unspec;
    }
    return -1;
}

/*	Load a document					HTLoad()
 *	---------------
 *
 *	This is an internal routine, which has an address AND a matching
 *	anchor.  (The public routines are called with one OR the other.)
 *
 *  On entry,
 *	addr		must point to the fully qualified hypertext reference.
 *	anchor		a parent anchor with whose address is addr
 *
 *  On exit,
 *	returns		<0		Error has occurred.
 *			HT_LOADED	Success
 *			HT_NO_DATA	Success, but no document loaded.
 *					(telnet session started etc)
 */
static int HTLoad(const char *addr,
		  HTParentAnchor *anchor,
		  HTFormat format_out,
		  HTStream *sink)
{
    HTProtocol *p;
    int status = get_physical(addr, anchor);

    if (status == HT_FORBIDDEN) {
	/* prevent crash if telnet or similar was forbidden by rule. - kw */
	LYFixCursesOn("show alert:");
	return HTLoadError(sink, 500, gettext("Access forbidden by rule"));
    } else if (status == HT_REDIRECTING) {
	return status;		/* fake redirection by rule, to redirecting_url */
    }
    if (status < 0)
	return status;		/* Can't resolve or forbidden */

    /* prevent crash if telnet or similar mapped or proxied by rule. - kw */
    LYFixCursesOnForAccess(addr, HTAnchor_physical(anchor));
    p = (HTProtocol *) HTAnchor_protocol(anchor);
    anchor->parent->underway = TRUE;	/* Hack to deal with caching */
    status = p->load(HTAnchor_physical(anchor),
		     anchor, format_out, sink);
    anchor->parent->underway = FALSE;
    LYUCPopAssumed();
    return status;
}

/*	Get a save stream for a document		HTSaveStream()
 *	--------------------------------
 */
HTStream *HTSaveStream(HTParentAnchor *anchor)
{
    HTProtocol *p = (HTProtocol *) HTAnchor_protocol(anchor);

    if (!p)
	return NULL;

    return p->saveStream(anchor);
}

int redirection_attempts = 0;	/* counter in HTLoadDocument */

/*	Load a document - with logging etc		HTLoadDocument()
 *	----------------------------------
 *
 *	- Checks or documents already loaded
 *	- Logs the access
 *	- Allows stdin filter option
 *	- Trace output and error messages
 *
 *  On Entry,
 *	  anchor	    is the node_anchor for the document
 *	  full_address	    The address of the document to be accessed.
 *	  filter	    if YES, treat stdin as HTML
 *
 *  On Exit,
 *	  returns    YES     Success in opening document
 *		     NO      Failure
 */
static BOOL HTLoadDocument(const char *full_address,	/* may include #fragment */
			   HTParentAnchor *anchor,
			   HTFormat format_out,
			   HTStream *sink)
{
    int status;
    HText *text;
    const char *address_to_load = full_address;
    char *cp;
    BOOL ForcingNoCache = LYforce_no_cache;

    CTRACE((tfp, "HTAccess: loading document %s\n", address_to_load));

    /*
     * Free use_this_url_instead and reset permanent_redirection if not done
     * elsewhere.  - FM
     */
    FREE(use_this_url_instead);
    permanent_redirection = FALSE;

    /*
     * Make sure some yoyo doesn't send us 'round in circles with redirecting
     * URLs that point back to themselves.  We'll set the original Lynx limit
     * of 10 redirections per requested URL from a user, because the HTTP/1.1
     * will no longer specify a restriction to 5, but will leave it up to the
     * browser's discretion, in deference to Microsoft.  - FM
     */
    if (redirection_attempts > 10) {
	redirection_attempts = 0;
	HTAlert(TOO_MANY_REDIRECTIONS);
	return NO;
    }

    /*
     * If this is marked as an internal link but we don't have the document
     * loaded any more, and we haven't explicitly flagged that we want to
     * reload with LYforce_no_cache, then something has disappeared from the
     * cache when we expected it to be still there.  The user probably doesn't
     * expect a new network access.  So if we have POST data and safe is not
     * set in the anchor, ask for confirmation, and fail if not granted.  The
     * exception are LYNXIMGMAP documents, for which we defer to LYLoadIMGmap
     * for prompting if necessary.  - kw
     */
    text = (HText *) HTAnchor_document(anchor);
    if (LYinternal_flag && !text && !LYforce_no_cache &&
	anchor->post_data && !anchor->safe &&
	!isLYNXIMGMAP(full_address) &&
	HTConfirm(gettext("Document with POST content not found in cache.  Resubmit?"))
	!= TRUE) {
	return NO;
    }

    /*
     * If we don't have POST content, check whether this is a previous
     * redirecting URL, and keep re-checking until we get to the final
     * destination or redirection limit.  If we do have POST content, we didn't
     * allow permanent redirection, and an interactive user will be deciding
     * whether to keep redirecting.  - FM
     */
    if (!anchor->post_data) {
	while ((cp = HTAnchor_physical(anchor)) != NULL &&
	       !strncmp(cp, "Location=", 9)) {
	    DocAddress NewDoc;

	    CTRACE((tfp, "HTAccess: '%s' is a redirection URL.\n",
		    anchor->address));
	    CTRACE((tfp, "HTAccess: Redirecting to '%s'\n", cp + 9));

	    /*
	     * Don't exceed the redirection_attempts limit.  - FM
	     */
	    if (++redirection_attempts > 10) {
		HTAlert(TOO_MANY_REDIRECTIONS);
		redirection_attempts = 0;
		FREE(use_this_url_instead);
		return NO;
	    }

	    /*
	     * Set up the redirection. - FM
	     */
	    StrAllocCopy(use_this_url_instead, cp + 9);
	    NewDoc.address = use_this_url_instead;
	    NewDoc.post_data = NULL;
	    NewDoc.post_content_type = NULL;
	    NewDoc.bookmark = anchor->bookmark;
	    NewDoc.isHEAD = anchor->isHEAD;
	    NewDoc.safe = anchor->safe;
	    anchor = HTAnchor_findAddress(&NewDoc);
	}
    }
    /*
     * If we had previous redirection, go back and check out that the URL under
     * the current restrictions.  - FM
     */
    if (use_this_url_instead) {
	FREE(redirecting_url);
	return (NO);
    }

    /*
     * See if we can use an already loaded document.
     */
    text = (HText *) HTAnchor_document(anchor);
    if (text && !LYforce_no_cache) {
	/*
	 * We have a cached rendition of the target document.  Check if it's OK
	 * to re-use it.  We consider it OK if:
	 * (1) the anchor does not have the no_cache element set, or
	 * (2) we've overridden it, e.g., because we are acting on a PREV_DOC
	 * command or a link in the History Page and it's not a reply from a
	 * POST with the LYresubmit_posts flag set, or
	 * (3) we are repositioning within the currently loaded document based
	 * on the target anchor's address (URL_Reference).
	 *
	 * If DONT_TRACK_INTERNAL_LINKS is defined, HText_AreDifferent() is
	 * used to determine whether (3) applies.  If the target address
	 * differs from that of the current document only by a fragment and the
	 * target address has an appended fragment, repositioning without
	 * reloading is always assumed.  Note that HText_AreDifferent()
	 * currently always returns TRUE if the target has a LYNXIMGMAP URL, so
	 * that an internally generated pseudo-document will normally not be
	 * re-used unless condition (2) applies.  (Condition (1) cannot apply
	 * since in LYMap.c, no_cache is always set in the anchor object). 
	 * This doesn't guarantee that the resource from which the MAP element
	 * is taken will be read again (reloaded) when the list of links for a
	 * client-side image map is regenerated, when in some cases it should
	 * (e.g., user requested RELOAD, or HTTP response with no-cache header
	 * and we are not overriding).
	 *
	 * If DONT_TRACK_INTERNAL_LINKS is undefined, a target address that
	 * points to the same URL as the current document may still result in
	 * reloading, depending on whether the original URL-Reference was given
	 * as an internal link in the context of the previously loaded
	 * document.  HText_AreDifferent() is not used here for testing whether
	 * we are just repositioning.  For an internal link, the potential
	 * callers of this function from mainloop() down will either avoid
	 * making the call (and do the repositioning differently) or set
	 * LYinternal_flag (or LYoverride_no_cache).  Note that (a) LYNXIMGMAP
	 * pseudo-documents and (b) The "List Page" document are treated
	 * logically as being part of the document on which they are based, for
	 * the purpose of whether to treat a link as internal, but the logic
	 * for this (by setting LYinternal_flag as necessary) is implemented
	 * elsewhere.  There is a specific test for LYNXIMGMAP here so that the
	 * generated pseudo-document will not be re-used unless
	 * LYoverride_no_cache is set.  The same caveat as above applies w.r.t. 
	 * reloading of the underlying resource.
	 *
	 * We also should be checking other aspects of cache regulation (e.g.,
	 * based on an If-Modified-Since check, etc.) but the code for doing
	 * those other things isn't available yet.
	 */
	if (LYoverride_no_cache ||
#ifdef DONT_TRACK_INTERNAL_LINKS
	    !HText_hasNoCacheSet(text) ||
	    !HText_AreDifferent(anchor, full_address)
#else
	    ((LYinternal_flag || !HText_hasNoCacheSet(text)) &&
	     !isLYNXIMGMAP(full_address))
#endif /* TRACK_INTERNAL_LINKS */
	    ) {
	    CTRACE((tfp, "HTAccess: Document already in memory.\n"));
	    HText_select(text);

#ifdef DIRED_SUPPORT
	    if (HTAnchor_format(anchor) == WWW_DIRED)
		lynx_edit_mode = TRUE;
#endif
	    redirection_attempts = 0;
	    return YES;
	} else {
	    ForcingNoCache = YES;
	    CTRACE((tfp, "HTAccess: Auto-reloading document.\n"));
	}
    }

    if (text && HText_HaveUserChangedForms(text)) {
	/*
	 * Issue a warning.  User forms content will be lost.
	 * Will not restore changed forms, currently.
	 */
	HTAlert(RELOADING_FORM);
    }

    /*
     * Get the document from the net.  If we are auto-reloading, the mutable
     * anchor elements from the previous rendition should be freed in
     * conjunction with loading of the new rendition.  - FM
     */
    LYforce_no_cache = NO;	/* reset after each time through */
    if (ForcingNoCache) {
	FREE(anchor->title);	/* ??? */
    }
    status = HTLoad(address_to_load, anchor, format_out, sink);
    CTRACE((tfp, "HTAccess:  status=%d\n", status));

    /*
     * RECOVERY:  if the loading failed, and we had a cached HText copy, and no
     * new HText created - use a previous copy, issue a warning.
     */
    if (text && status < 0 && (HText *) HTAnchor_document(anchor) == text) {
	HTAlert(gettext("Loading failed, use a previous copy."));
	CTRACE((tfp, "HTAccess: Loading failed, use a previous copy.\n"));
	HText_select(text);

#ifdef DIRED_SUPPORT
	if (HTAnchor_format(anchor) == WWW_DIRED)
	    lynx_edit_mode = TRUE;
#endif
	redirection_attempts = 0;
	return YES;
    }

    /*
     * Log the access if necessary.
     */
    if (HTlogfile) {
	time_t theTime;

	time(&theTime);
	fprintf(HTlogfile, "%24.24s %s %s %s\n",
		ctime(&theTime),
		HTClientHost ? HTClientHost : "local",
		status < 0 ? "FAIL" : "GET",
		full_address);
	fflush(HTlogfile);	/* Actually update it on disk */
	CTRACE((tfp, "Log: %24.24s %s %s %s\n",
		ctime(&theTime),
		HTClientHost ? HTClientHost : "local",
		status < 0 ? "FAIL" : "GET",
		full_address));
    }

    /*
     * Check out what we received from the net.
     */
    if (status == HT_REDIRECTING) {
	/* Exported from HTMIME.c, of all places.  */
	/* NO!! - FM */
	/*
	 * Doing this via HTMIME.c meant that the redirection cover page was
	 * already loaded before we learned that we want a different URL. 
	 * Also, changing anchor->address, as Lynx was doing, meant we could
	 * never again access its hash table entry, creating an insolvable
	 * memory leak.  Instead, if we had a 301 status and set
	 * permanent_redirection, we'll load the new URL in anchor->physical,
	 * preceded by a token, which we can check to make replacements on
	 * subsequent access attempts.  We'll check recursively, and retrieve
	 * the final URL if we had multiple redirections to it.  If we just
	 * went to HTLoad now, as Lou originally had this, we couldn't do
	 * Lynx's security checks and alternate handling of some URL types. 
	 * So, instead, we'll go all the way back to the top of getfile in
	 * LYGetFile.c when the status is HT_REDIRECTING.  This may seem
	 * bizarre, but it works like a charm!  - FM
	 *
	 * Actually, the location header for redirections is now again picked
	 * up in HTMIME.c.  But that's an internal matter between HTTP.c and
	 * HTMIME.c, is still under control of HTLoadHTTP for http URLs, is
	 * done in a way that doesn't load the redirection response's body
	 * (except when wanted as an error fallback), and thus need not concern
	 * us here.  - kw 1999-12-02
	 */
	CTRACE((tfp, "HTAccess: '%s' is a redirection URL.\n",
		address_to_load));
	CTRACE((tfp, "HTAccess: Redirecting to '%s'\n",
		redirecting_url));
	/*
	 * Prevent circular references.
	 */
	if (strcmp(address_to_load, redirecting_url)) {		/* if different */
	    /*
	     * Load token and redirecting url into anchor->physical if we had
	     * 301 Permanent redirection.  HTTP.c does not allow this if we
	     * have POST content.  - FM
	     */
	    if (permanent_redirection) {
		StrAllocCopy(anchor->physical, "Location=");
		StrAllocCat(anchor->physical, redirecting_url);
	    }

	    /*
	     * Set up flags before return to getfile.  - FM
	     */
	    StrAllocCopy(use_this_url_instead, redirecting_url);
	    if (ForcingNoCache)
		LYforce_no_cache = YES;
	    ++redirection_attempts;
	    FREE(redirecting_url);
	    permanent_redirection = FALSE;
	    return (NO);
	}
	++redirection_attempts;
	FREE(redirecting_url);
	permanent_redirection = FALSE;
	return (YES);
    }

    /*
     * We did not receive a redirecting URL.  - FM
     */
    redirection_attempts = 0;
    FREE(redirecting_url);
    permanent_redirection = FALSE;

    if (status == HT_LOADED) {
	CTRACE((tfp, "HTAccess: `%s' has been accessed.\n",
		full_address));
	return YES;
    }
    if (status == HT_PARTIAL_CONTENT) {
	HTAlert(gettext("Loading incomplete."));
	CTRACE((tfp, "HTAccess: `%s' has been accessed, partial content.\n",
		full_address));
	return YES;
    }

    if (status == HT_NO_DATA) {
	CTRACE((tfp, "HTAccess: `%s' has been accessed, No data left.\n",
		full_address));
	return NO;
    }

    if (status == HT_NOT_LOADED) {
	CTRACE((tfp, "HTAccess: `%s' has been accessed, No data loaded.\n",
		full_address));
	return NO;
    }

    if (status == HT_INTERRUPTED) {
	CTRACE((tfp,
		"HTAccess: `%s' has been accessed, transfer interrupted.\n",
		full_address));
	return NO;
    }

    if (status > 0) {
	/*
	 * If you get this, then please find which routine is returning a
	 * positive unrecognized error code!
	 */
	fprintf(stderr,
		gettext("**** HTAccess: socket or file number returned by obsolete load routine!\n"));
	fprintf(stderr,
		gettext("**** HTAccess: Internal software error.  Please mail lynx-dev@nongnu.org!\n"));
	fprintf(stderr, gettext("**** HTAccess: Status returned was: %d\n"), status);
	exit_immediately(EXIT_FAILURE);
    }

    /* Failure in accessing a document */
    cp = NULL;
    StrAllocCopy(cp, gettext("Can't Access"));
    StrAllocCat(cp, " `");
    StrAllocCat(cp, full_address);
    StrAllocCat(cp, "'");
    _HTProgress(cp);
    FREE(cp);

    CTRACE((tfp, "HTAccess: Can't access `%s'\n", full_address));
    HTLoadError(sink, 500, gettext("Unable to access document."));
    return NO;
}				/* HTLoadDocument */

/*	Load a document from absolute name.		HTLoadAbsolute()
 *	-----------------------------------
 *
 *  On Entry,
 *	  addr	   The absolute address of the document to be accessed.
 *	  filter   if YES, treat document as HTML
 *
 *  On Exit,
 *	  returns    YES     Success in opening document
 *		     NO      Failure
 */
BOOL HTLoadAbsolute(const DocAddress *docaddr)
{
    return HTLoadDocument(docaddr->address,
			  HTAnchor_findAddress(docaddr),
			  (HTOutputFormat ? HTOutputFormat : WWW_PRESENT),
			  HTOutputStream);
}

#ifdef NOT_USED_CODE
/*	Load a document from absolute name to stream.	HTLoadToStream()
 *	---------------------------------------------
 *
 *  On Entry,
 *	  addr	   The absolute address of the document to be accessed.
 *	  sink	   if non-NULL, send data down this stream
 *
 *  On Exit,
 *	  returns    YES     Success in opening document
 *		     NO      Failure
 */
BOOL HTLoadToStream(const char *addr,
		    BOOL filter,
		    HTStream *sink)
{
    return HTLoadDocument(addr,
			  HTAnchor_findSimpleAddress(addr),
			  (HTOutputFormat ? HTOutputFormat : WWW_PRESENT),
			  sink);
}
#endif /* NOT_USED_CODE */

/*	Load a document from relative name.		HTLoadRelative()
 *	-----------------------------------
 *
 *  On Entry,
 *	  relative_name     The relative address of the document
 *			    to be accessed.
 *
 *  On Exit,
 *	  returns    YES     Success in opening document
 *		     NO      Failure
 */
BOOL HTLoadRelative(const char *relative_name,
		    HTParentAnchor *here)
{
    DocAddress full_address;
    BOOL result;
    char *mycopy = NULL;
    char *stripped = NULL;

    full_address.address = NULL;
    full_address.post_data = NULL;
    full_address.post_content_type = NULL;
    full_address.bookmark = NULL;
    full_address.isHEAD = FALSE;
    full_address.safe = FALSE;

    StrAllocCopy(mycopy, relative_name);

    stripped = HTStrip(mycopy);
    full_address.address =
	HTParse(stripped,
		here->address,
		PARSE_ALL_WITHOUT_ANCHOR);
    result = HTLoadAbsolute(&full_address);
    /*
     * If we got redirection, result will be NO, but use_this_url_instead will
     * be set.  The calling routine should check both and do whatever is
     * appropriate.  - FM
     */
    FREE(full_address.address);
    FREE(mycopy);		/* Memory leak fixed 10/7/92 -- JFG */
    return result;
}

/*	Load if necessary, and select an anchor.	HTLoadAnchor()
 *	----------------------------------------
 *
 *  On Entry,
 *	  destination		    The child or parent anchor to be loaded.
 *
 *  On Exit,
 *	  returns    YES     Success
 *		     NO      Failure
 */
BOOL HTLoadAnchor(HTAnchor * destination)
{
    HTParentAnchor *parent;
    BOOL loaded = NO;

    if (!destination)
	return NO;		/* No link */

    parent = HTAnchor_parent(destination);

    if (HTAnchor_document(parent) == NULL) {	/* If not already loaded */
	/* TBL 921202 */
	BOOL result;

	result = HTLoadDocument(parent->address,
				parent,
				HTOutputFormat ?
				HTOutputFormat : WWW_PRESENT,
				HTOutputStream);
	if (!result)
	    return NO;
	loaded = YES;
    } {
	HText *text = (HText *) HTAnchor_document(parent);

	if ((destination != (HTAnchor *) parent) &&
	    (destination != (HTAnchor *) (parent->parent))) {
	    /* If child anchor */
	    HText_selectAnchor(text,	/* Double display? @@ */
			       (HTChildAnchor *) destination);
	} else {
	    if (!loaded)
		HText_select(text);
	}
    }
    return YES;

}				/* HTLoadAnchor */

/*	Search.						HTSearch()
 *	-------
 *
 *	Performs a keyword search on word given by the user.  Adds the
 *	keyword to the end of the current address and attempts to open
 *	the new address.
 *
 *  On Entry,
 *	 *keywords	space-separated keyword list or similar search list
 *	here		is anchor search is to be done on.
 */
static char hex(int i)
{
    const char *hexchars = "0123456789ABCDEF";

    return hexchars[i];
}

BOOL HTSearch(const char *keywords,
	      HTParentAnchor *here)
{
#define acceptable \
"1234567890abcdefghijlkmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ.-_"

    char *q, *u;
    const char *p, *s, *e;	/* Pointers into keywords */
    char *address = NULL;
    BOOL result;
    char *escaped = typecallocn(char, (strlen(keywords) * 3) + 1);
    static const BOOL isAcceptable[96] =
    /* *INDENT-OFF* */
    /*	 0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {	 0,0,0,0,0,0,0,0,0,0,1,0,0,1,1,0,	/* 2x	!"#$%&'()*+,-./  */
	 1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?  */
	 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* 4x  @ABCDEFGHIJKLMNO  */
	 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,	/* 5X  PQRSTUVWXYZ[\]^_  */
	 0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,	/* 6x  `abcdefghijklmno  */
	 1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0 };	/* 7X  pqrstuvwxyz{\}~	DEL */
    /* *INDENT-ON* */

    if (escaped == NULL)
	outofmem(__FILE__, "HTSearch");

    StrAllocCopy(address, here->isIndexAction);

    /*
     * Convert spaces to + and hex escape unacceptable characters.
     */
    for (s = keywords; *s && WHITE(*s); s++)	/* Scan */
	;			/* Skip white space */
    for (e = s + strlen(s); e > s && WHITE(*(e - 1)); e--)	/* Scan */
	;			/* Skip trailers */
    for (q = escaped, p = s; p < e; p++) {	/* Scan stripped field */
	unsigned char c = UCH(TOASCII(*p));

	if (WHITE(*p)) {
	    *q++ = '+';
	} else if (HTCJK != NOCJK) {
	    *q++ = *p;
	} else if (c >= 32 && c <= UCH(127) && isAcceptable[c - 32]) {
	    *q++ = *p;		/* 930706 TBL for MVS bug */
	} else {
	    *q++ = '%';
	    *q++ = hex((int) (c >> 4));
	    *q++ = hex((int) (c & 15));
	}
    }				/* Loop over string */
    *q = '\0';			/* Terminate escaped string */
    u = strchr(address, '?');	/* Find old search string */
    if (u != NULL)
	*u = '\0';		/* Chop old search off */

    StrAllocCat(address, "?");
    StrAllocCat(address, escaped);
    FREE(escaped);
    result = HTLoadRelative(address, here);
    FREE(address);

    /*
     * If we got redirection, result will be NO, but use_this_url_instead will
     * be set.  The calling routine should check both and do whatever is
     * appropriate.  Only an http server (not a gopher or wais server) could
     * return redirection.  Lynx will go all the way back to its mainloop() and
     * subject a redirecting URL to all of its security and restrictions
     * checks.  - FM
     */
    return result;
}

/*	Search Given Indexname.			HTSearchAbsolute()
 *	-----------------------
 *
 *	Performs a keyword search on word given by the user.  Adds the
 *	keyword to the end of the current address and attempts to open
 *	the new address.
 *
 *  On Entry,
 *	*keywords	space-separated keyword list or similar search list
 *	*indexname	is name of object search is to be done on.
 */
BOOL HTSearchAbsolute(const char *keywords,
		      char *indexname)
{
    DocAddress abs_doc;
    HTParentAnchor *anchor;

    abs_doc.address = indexname;
    abs_doc.post_data = NULL;
    abs_doc.post_content_type = NULL;
    abs_doc.bookmark = NULL;
    abs_doc.isHEAD = FALSE;
    abs_doc.safe = FALSE;

    anchor = HTAnchor_findAddress(&abs_doc);
    return HTSearch(keywords, anchor);
}

#ifdef NOT_USED_CODE
/*	Generate the anchor for the home page.		HTHomeAnchor()
 *	--------------------------------------
 *
 *	As it involves file access, this should only be done once
 *	when the program first runs.
 *	This is a default algorithm -- browser don't HAVE to use this.
 *	But consistency between browsers is STRONGLY recommended!
 *
 *  Priority order is:
 *		1	WWW_HOME environment variable (logical name, etc)
 *		2	~/WWW/default.html
 *		3	/usr/local/bin/default.html
 *		4	http://www.w3.org/default.html
 */
HTParentAnchor *HTHomeAnchor(void)
{
    char *my_home_document = NULL;
    char *home = LYGetEnv(LOGICAL_DEFAULT);
    char *ref;
    HTParentAnchor *anchor;

    if (home) {
	StrAllocCopy(my_home_document, home);
#define MAX_FILE_NAME 1024	/* @@@ */
    } else if (HTClientHost) {	/* Telnet server */
	/*
	 * Someone telnets in, they get a special home.
	 */
	FILE *fp = fopen(REMOTE_POINTER, "r");
	char *status;

	if (fp) {
	    my_home_document = typecallocn(char, MAX_FILE_NAME);

	    if (my_home_document == NULL)
		outofmem(__FILE__, "HTHomeAnchor");
	    status = fgets(my_home_document, MAX_FILE_NAME, fp);
	    if (!status) {
		FREE(my_home_document);
	    }
	    fclose(fp);
	}
	if (my_home_document == NULL)
	    StrAllocCopy(my_home_document, REMOTE_ADDRESS);
    }
#ifdef UNIX
    if (my_home_document == NULL) {
	FILE *fp = NULL;
	char *home = LYGetEnv("HOME");

	if (home != 0) {
	    HTSprintf0(&my_home_document, "%s/%s", home, PERSONAL_DEFAULT);
	    fp = fopen(my_home_document, "r");
	}

	if (!fp) {
	    StrAllocCopy(my_home_document, LOCAL_DEFAULT_FILE);
	    fp = fopen(my_home_document, "r");
	}
	if (fp) {
	    fclose(fp);
	} else {
	    CTRACE((tfp, "HTBrowse: No local home document ~/%s or %s\n",
		    PERSONAL_DEFAULT, LOCAL_DEFAULT_FILE));
	    FREE(my_home_document);
	}
    }
#endif /* UNIX */
    ref = HTParse((my_home_document ?
		   my_home_document : (HTClientHost ?
				       REMOTE_ADDRESS : LAST_RESORT)),
		  STR_FILE_URL,
		  PARSE_ALL_WITHOUT_ANCHOR);
    if (my_home_document) {
	CTRACE((tfp, "HTAccess: Using custom home page %s i.e., address %s\n",
		my_home_document, ref));
	FREE(my_home_document);
    }
    anchor = HTAnchor_findSimpleAddress(ref);
    FREE(ref);
    return anchor;
}
#endif /* NOT_USED_CODE */
