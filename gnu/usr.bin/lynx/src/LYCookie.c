/*			       Lynx Cookie Support		   LYCookie.c
**			       ===================
**
**	Author: AMK	A.M. Kuchling (amk@magnet.com)	12/25/96
**
**	Incorporated with mods by FM			01/16/97
**
**  Based on:
**	http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-mgmt-05.txt
**
**	Updated for:
**   http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-man-mec-02.txt
**		- FM					1997-07-09
**
**	Updated for:
**   ftp://ds.internic.net/internet-drafts/draft-ietf-http-state-man-mec-03.txt
**		- FM					1997-08-02
**
**  TO DO: (roughly in order of decreasing priority)
      * A means to specify "always allow" and "never allow" domains via
	a configuration file is needed.
      * Hex escaping isn't considered at all.  Any semi-colons, commas,
	or spaces actually in cookie names or values (i.e., not serving
	as punctuation for the overall Set-Cookie value) should be hex
	escaped if not quoted, but presumeably the server is expecting
	them to be hex escaped in our Cookie request header as well, so
	in theory we need not unescape them.  We'll see how this works
	out in practice.
      * The prompt should show more information about the cookie being
	set in Novice mode.
      * The truncation heuristic in HTConfirmCookie should probably be
	smarter, smart enough to leave a really short name/value string
	alone.
      * We protect against denial-of-service attacks (see section 6.3.1
	of the draft) by limiting a domain to 50 cookies, limiting the
	total number of cookies to 500, and limiting a processed cookie
	to a maximum of 4096 bytes, but we count on the normal garbage
	collections to bring us back down under the limits, rather than
	actively removing cookies and/or domains based on age or frequency
	of use.
      * If a cookie has the secure flag set, we presently treat only SSL
	connections as secure.	This may need to be expanded for other
	secure communication protocols that become standarized.
      * Cookies could be optionally stored in a file from session to session.
*/

#include "HTUtils.h"
#include "tcp.h"
#include "HTAccess.h"
#include "HTParse.h"
#include "HTAlert.h"
#include "LYCurses.h"
#include "LYSignal.h"
#include "LYUtils.h"
#include "LYCharUtils.h"
#include "LYClean.h"
#include "LYGlobalDefs.h"
#include "LYEdit.h"
#include "LYStrings.h"
#include "LYSystem.h"
#include "GridText.h"
#include "LYUtils.h"
#include "LYCharUtils.h"
#include "LYCookie.h"

#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

/*
**  The first level of the cookie list is a list indexed by the domain
**  string; cookies with the same domain will be placed in the same
**  list.  Thus, finding the cookies that apply to a given URL is a
**  two-level scan; first we check each domain to see if it applies,
**  and if so, then we check the paths of all the cookies on that
**  list.   We keep a running total of cookies as we add or delete
**  them
*/
PRIVATE HTList *domain_list = NULL;
PRIVATE HTList *cookie_list = NULL;
PRIVATE int total_cookies = 0;

struct _cookie {
    char *lynxID;  /* Lynx cookie identifier */
    char *name;    /* Name of this cookie */
    char *value;   /* Value of this cookie */
    int version;   /* Cookie protocol version (=1) */
    char *comment; /* Comment to show to user */
    char *commentURL; /* URL for comment to show to user */
    char *domain;  /* Domain for which this cookie is valid */
    int port;	   /* Server port from which this cookie was given (usu. 80) */
    char *PortList;/* List of ports for which cookie can be sent */
    char *path;    /* Path prefix for which this cookie is valid */
    int pathlen;   /* Length of the path */
    int flags;	   /* Various flags */
    time_t expires;/* The time when this cookie expires */
    BOOL quoted;   /* Was a value quoted in the Set-Cookie header? */
};
typedef struct _cookie cookie;

#define COOKIE_FLAG_SECURE 1	   /* If set, cookie requires secure links */
#define COOKIE_FLAG_DISCARD 2	   /* If set, expire at end of session */
#define COOKIE_FLAG_EXPIRES_SET 4  /* If set, an expiry date was set */
#define COOKIE_FLAG_DOMAIN_SET 8   /* If set, an non-default domain was set */
#define COOKIE_FLAG_PATH_SET 16    /* If set, an non-default path was set */
struct _HTStream
{
  HTStreamClass * isa;
};

PRIVATE void MemAllocCopy ARGS3(
	char **,	dest,
	CONST char *,	start,
	CONST char *,	end)
{
    char *temp;

    if (!(start && end) || (end <= start)) {
	HTSACopy(dest, "");
	return;
    }

    temp = (char *)calloc(1, ((end - start) + 1));
    if (temp == NULL)
	outofmem(__FILE__, "MemAllocCopy");
    LYstrncpy(temp, start, (end - start));
    HTSACopy(dest, temp);
    FREE(temp);
}

PRIVATE cookie * newCookie NOARGS
{
    cookie *p = (cookie *)calloc(1, sizeof(cookie));
    char lynxID[64];

    if (p == NULL)
	outofmem(__FILE__, "newCookie");
    sprintf(lynxID, "%p", p);
    StrAllocCopy(p->lynxID, lynxID);
    p->port = 80;
    return p;
}

PRIVATE void freeCookie ARGS1(
	cookie *,	co)
{
    if (co) {
	FREE(co->lynxID);
	FREE(co->name);
	FREE(co->value);
	FREE(co->comment);
	FREE(co->commentURL);
	FREE(co->domain);
	FREE(co->path);
	FREE(co->PortList);
	FREE(co);
    }
}

PRIVATE void LYCookieJar_free NOARGS
{
    HTList *dl = domain_list;
    domain_entry *de = NULL;
    HTList *cl = NULL, *next = NULL;
    cookie *co = NULL;

    while (dl) {
	if ((de = dl->object) != NULL) {
	    cl = de->cookie_list;
	    while (cl) {
		next = cl->next;
		co = cl->object;
		if (co) {
		    HTList_removeObject(de->cookie_list, co);
		    freeCookie(co);
		}
		cl = next;
	    }
	    FREE(de->domain);
	    HTList_delete(de->cookie_list);
	    de->cookie_list = NULL;
	}
	dl = dl->next;
    }
    if (dump_output_immediately) {
	cl = cookie_list;
	while (cl) {
	    next = cl->next;
	    co = cl->object;
	    if (co) {
		HTList_removeObject(cookie_list, co);
		freeCookie(co);
	    }
	    cl = next;
	}
	HTList_delete(cookie_list);
    }
    cookie_list = NULL;
    HTList_delete(domain_list);
    domain_list = NULL;
}

/*
**  Compare two hostnames as specified in Section 2 of:
**   http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-man-mec-02.txt
**	- AK & FM
*/
PRIVATE BOOLEAN host_matches ARGS2(
	CONST char *,	A,
	CONST char *,	B)
{
    /*
     *	The following line will handle both numeric IP addresses and
     *	FQDNs.	Do numeric addresses require special handling?
     */
    if (*B != '.' && !strcmp(A, B))
	return YES;

    /*
     *	The following will pass a "dotted tail" match to "a.b.c.e"
     *	as described in Section 2 of the -05 draft.
     */
    if (*B == '.') {
	int diff = (strlen(A) - strlen(B));
	if (diff > 0) {
	    if (!strcmp((A + diff), B))
		return YES;
	}
    }
    return NO;
}

/*
**  Compare the current port with a port list as specified in Section 4.3 of:
**   http://www.ics.uci.edu/pub/ietf/http/draft-ietf-http-state-man-mec-02.txt
**	- FM
*/
PRIVATE BOOLEAN port_matches ARGS2(
	int,		port,
	CONST char *,	list)
{
    CONST char *number = list;

    if (!(number && isdigit(*number)))
	return(FALSE);

    while (*number != '\0') {
	if (atoi(number) == port) {
	    return(TRUE);
	}
	while (isdigit(*number)) {
	    number++;
	}
	while (*number != '\0' && !isdigit(*number)) {
	    number++;
	}
    }

    return(FALSE);
}

/*
**  Store a cookie somewhere in the domain list. - AK & FM
*/
PRIVATE void store_cookie ARGS3(
	cookie *,	co,
	CONST char *,	hostname,
	CONST char *,	path)
{
    HTList *hl, *next;
    cookie *c2;
    time_t now = time(NULL);
    int pos;
    CONST char *ptr;
    domain_entry *de = NULL;
    BOOL Replacement = FALSE;

    if (co == NULL)
	return;

    /*
     *	Apply sanity checks.
     *
     *	Section 4.3.2, condition 1: The value for the Path attribute is
     *	not a prefix of the request-URI.
     */
    if (strncmp(co->path, path, co->pathlen) != 0) {
	if (TRACE)
	    fprintf(stderr,
	    "store_cookie: Rejecting because '%s' is not a prefix of '%s'.\n",
		    co->path, path);
	freeCookie(co);
	co = NULL;
	return;
    }
    /*
     *	The next 4 conditions do NOT apply if the domain is still
     *	the default of request-host.
     */
    if (strcmp(co->domain, hostname) != 0) {
	/*
	 *  The hostname does not contain a dot.
	 */
	if (strchr(hostname, '.') == NULL) {
	    if (TRACE)
		fprintf(stderr,
			"store_cookie: Rejecting because '%s' has no dot.\n",
			hostname);
	    freeCookie(co);
	    co = NULL;
	    return;
	}

	/*
	 *  Section 4.3.2, condition 2: The value for the Domain attribute
	 *  contains no embedded dots or does not start with a dot.
	 *  (A dot is embedded if it's neither the first nor last character.)
	 *  Note that we added a lead dot ourselves if a domain attribute
	 *  value otherwise qualified. - FM
	 */
	if (co->domain[0] != '.' || co->domain[1] == '\0') {
	    if (TRACE)
		fprintf(stderr,
			"store_cookie: Rejecting domain '%s'.\n",
			co->domain);
	    freeCookie(co);
	    co = NULL;
	    return;
	}
	ptr = strchr((co->domain + 1), '.');
	if (ptr == NULL || ptr[1] == '\0') {
	    if (TRACE)
		fprintf(stderr,
			"store_cookie: Rejecting domain '%s'.\n",
			co->domain);
	    freeCookie(co);
	    co = NULL;
	    return;
	}

	/*
	 *  Section 4.3.2, condition 3: The value for the request-host does
	 *  not domain-match the Domain attribute.
	 */
	if (!host_matches(hostname, co->domain)) {
	    if (TRACE)
		fprintf(stderr,
			"store_cookie: Rejecting domain '%s' for host '%s'.\n",
			co->domain, hostname);
	    freeCookie(co);
	    co = NULL;
	    return;
	}

	/*
	 *  Section 4.3.2, condition 4: The request-host is an HDN (not IP
	 *  address) and has the form HD, where D is the value of the Domain
	 *  attribute, and H is a string that contains one or more dots.
	 */
	ptr = ((hostname + strlen(hostname)) - strlen(co->domain));
	if (strchr(hostname, '.') < ptr) {
	    char *msg = calloc(1,
			       (strlen(co->domain) +
				strlen(hostname) +
				strlen(INVALID_COOKIE_DOMAIN_CONFIRMATION) +
				1));

	    sprintf(msg,
		    INVALID_COOKIE_DOMAIN_CONFIRMATION,
		    co->domain,
		    hostname);
	    if (!HTConfirm(msg)) {
		if (TRACE) {
		    fprintf(stderr,
		       "store_cookie: Rejecting domain '%s' for host '%s'.\n",
			    co->domain,
			    hostname);
		}
		freeCookie(co);
		co = NULL;
		FREE(msg);
		return;
	    }
	    FREE(msg);
	}
    }

    /*
     *	Ensure that the domain list exists.
     */
    if (domain_list == NULL) {
	atexit(LYCookieJar_free);
	domain_list = HTList_new();
	total_cookies = 0;
    }

    /*
     *	Look through domain_list to see if the cookie's domain
     *	is already listed.
     */
    if (dump_output_immediately) { /* Non-interactive, can't respond */
	if (cookie_list == NULL)
	    cookie_list = HTList_new();
    } else {
	cookie_list = NULL;
	for (hl = domain_list; hl != NULL; hl = hl->next) {
	    de = (domain_entry *)hl->object;
	    if ((de != NULL && de->domain != NULL) &&
		!strcmp(co->domain, de->domain)) {
		cookie_list = de->cookie_list;
		break;
	    }
	}
	if (hl == NULL) {
	    /*
	     *	Domain not found; add a new entry for this domain.
	     */
	    de = (domain_entry *)calloc(1, sizeof(domain_entry));
	    if (de == NULL)
		outofmem(__FILE__, "store_cookie");
	    de->bv = QUERY_USER;
	    cookie_list = de->cookie_list = HTList_new();
	    StrAllocCopy(de->domain, co->domain);
	    HTList_addObject(domain_list, de);
	}
    }

    /*
     *	Loop over the cookie list, deleting expired and matching cookies.
     */
    hl = cookie_list;
    pos = 0;
    while (hl) {
	c2 = (cookie *)hl->object;
	next = hl->next;
	/*
	 *  Check if this cookie has expired.
	 */
	if ((c2 != NULL) &&
	    (c2->flags & COOKIE_FLAG_EXPIRES_SET) &&
	    c2->expires < now) {
	    HTList_removeObject(cookie_list, c2);
	    freeCookie(c2);
	    c2 = NULL;
	    total_cookies--;

	/*
	 *  Check if this cookie matches the one we're inserting.
	 */
	} else if ((c2) &&
		   !strcmp(co->domain, c2->domain) &&
		   !strcmp(co->path, c2->path) &&
		   !strcmp(co->name, c2->name)) {
	    HTList_removeObject(cookie_list, c2);
	    freeCookie(c2);
	    c2 = NULL;
	    total_cookies--;
	    Replacement = TRUE;

	} else if ((c2) && (c2->pathlen) > (co->pathlen)) {
	    pos++;
	}
	hl = next;
    }

    /*
     *	Don't bother to add the cookie if it's already expired.
     */
    if ((co->flags & COOKIE_FLAG_EXPIRES_SET) && co->expires < now) {
	freeCookie(co);
	co = NULL;

    /*
     *	Don't add the cookie if we're over the domain's limit. - FM
     */
    } else if (HTList_count(cookie_list) > 50) {
	if (TRACE)
	    fprintf(stderr,
	"store_cookie: Domain's cookie limit exceeded!  Rejecting cookie.\n");
	freeCookie(co);
	co = NULL;

    /*
     *	Don't add the cookie if we're over the total cookie limit. - FM
     */
    } else if (total_cookies > 500) {
	if (TRACE)
	    fprintf(stderr,
	"store_cookie: Total cookie limit exceeded!  Rejecting cookie.\n");
	freeCookie(co);
	co = NULL;

    /*
     *	If it's a replacement for a cookie that had not expired,
     *	and never allow has not been set, add it again without
     *	confirmation. - FM
     */
    } else if ((Replacement == TRUE && de) && de->bv != REJECT_ALWAYS) {
	HTList_insertObjectAt(cookie_list, co, pos);
	total_cookies++;

    /*
     *	Get confirmation if we need it, and add cookie
     *	if confirmed or 'allow' is set to always. - FM
     */
    } else if (HTConfirmCookie(de, hostname,
			       co->domain, co->path, co->name, co->value)) {
	HTList_insertObjectAt(cookie_list, co, pos);
	total_cookies++;
    } else {
	freeCookie(co);
	co = NULL;
    }
}

/*
**  Scan a domain's cookie_list for any cookies we should
**  include in a Cookie: request header. - AK & FM
*/
PRIVATE char * scan_cookie_sublist ARGS6(
	CONST char *,	hostname,
	CONST char *,	path,
	int,		port,
	HTList *,	sublist,
	char *, 	header,
	BOOL,		secure)
{
    HTList *hl = sublist, *next = NULL;
    cookie *co;
    time_t now = time(NULL);
    int len = 0;
    char crlftab[8];

    sprintf(crlftab, "%c%c%c", CR, LF, '\t');
    while (hl) {
	co = (cookie *)hl->object;
	next = hl->next;

	if (TRACE && co) {
	    fprintf(stderr, "Checking cookie %lx %s=%s\n",
			    (long)hl,
			    (co->name ? co->name : "(no name)"),
			    (co->value ? co->value : "(no value)"));
	    fprintf(stderr, "%s %s %d %s %s %d%s\n",
			    hostname,
			    (co->domain ? co->domain : "(no domain)"),
			    host_matches(hostname, co->domain),
			    path, co->path, ((co->pathlen > 0) ?
			  strncmp(path, co->path, co->pathlen) : 0),
			    ((co->flags & COOKIE_FLAG_SECURE) ?
						   " secure" : ""));
	}
	/*
	 *  Check if this cookie has expired, and if so, delete it.
	 */
	if (((co) && (co->flags & COOKIE_FLAG_EXPIRES_SET)) &&
	    co->expires < now) {
	    HTList_removeObject(sublist, co);
	    freeCookie(co);
	    co = NULL;
	    total_cookies--;
	}

	/*
	 *  Check if we have a unexpired match, and handle if we do.
	 */
	if (((co != NULL) &&
	     host_matches(hostname, co->domain)) &&
	    (co->pathlen == 0 || !strncmp(path, co->path, co->pathlen))) {
	    /*
	     *	Skip if the secure flag is set and we don't have
	     *	a secure connection.  HTTP.c presently treats only
	     *	SSL connections as secure. - FM
	     */
	    if ((co->flags & COOKIE_FLAG_SECURE) && secure == FALSE) {
		hl = next;
		continue;
	    }

	    /*
	     *	Skip if we have a port list and the
	     *	current port is not listed. - FM
	     */
	    if (co->PortList && !port_matches(port, co->PortList)) {
		hl = next;
		continue;
	    }

	    /*
	     *	Start or append to the request header.
	     */
	    if (header == NULL) {
		if (co->version > 0) {
		    /*
		     *	For Version 1 (or greater) cookies,
		     *	the version number goes before the
		     *	first cookie.
		     */
		    char version[16];
		    sprintf(version, "$Version=\"%d\"; ", co->version);
		    StrAllocCopy(header, version);
		    len += strlen(header);
		}
	    } else {
		/*
		 *  There's already cookie data there, so add
		 *  a separator (always use a semi-colon for
		 *  "backward compatibility"). - FM
		 */
		StrAllocCat(header, "; ");
		/*
		 *  Check if we should fold the header. - FM
		 */
		if (len > 800) {
		    StrAllocCat(header, crlftab);
		    len = 0;
		}
	    }
	    /*
	     *	Include the cookie name=value pair.
	     */
	    StrAllocCat(header, co->name);
	    StrAllocCat(header, "=");
	    if (co->quoted) {
		StrAllocCat(header, "\"");
		len++;
	    }
	    StrAllocCat(header, co->value);
	    if (co->quoted) {
		StrAllocCat(header, "\"");
		len++;
	    }
	    len += (strlen(co->name) + strlen(co->value) + 1);
	    /*
	     *	For Version 1 (or greater) cookies, add
	     *	$PATH, $PORT and/or $DOMAIN attributes for
	     *	the cookie if they were specified via a
	     *	server reply header. - FM
	     */
	    if (co->version > 0) {
		if (co->path && (co->flags & COOKIE_FLAG_PATH_SET)) {
		    /*
		     *	Append the path attribute. - FM
		     */
		    StrAllocCat(header, "; $Path=\"");
		    StrAllocCat(header, co->path);
		    StrAllocCat(header, "\"");
		    len += (strlen(co->path) + 10);
		}
		if (co->PortList && isdigit((unsigned char)*co->PortList)) {
		    /*
		     *	Append the port attribute. - FM
		     */
		    StrAllocCat(header, "; $Port=\"");
		    StrAllocCat(header, co->PortList);
		    StrAllocCat(header, "\"");
		    len += (strlen(co->PortList) + 10);
		}
		if (co->domain && (co->flags & COOKIE_FLAG_DOMAIN_SET)) {
		    /*
		     *	Append the domain attribute. - FM
		     */
		    StrAllocCat(header, "; $Domain=\"");
		    StrAllocCat(header, co->domain);
		    StrAllocCat(header, "\"");
		    len += (strlen(co->domain) + 12);
		}
	    }
	}
	hl = next;
    }

    return(header);
}

/*
**  Process potentially concatenated Set-Cookie2 and/or Set-Cookie
**  headers. - FM
*/
PRIVATE void LYProcessSetCookies ARGS6(
	CONST char *,	SetCookie,
	CONST char *,	SetCookie2,
	CONST char *,	address,
	char *, 	hostname,
	char *, 	path,
	int,		port)
{
    CONST char *p, *attr_start, *attr_end, *value_start, *value_end;
    HTList *CombinedCookies = NULL, *cl = NULL;
    cookie *cur_cookie = NULL, *co = NULL;
    int length = 0, url_type = 0;
    int NumCookies = 0;
    BOOL MaxAgeAttrSet = FALSE;
    BOOL Quoted = FALSE;

    if (!(SetCookie && *SetCookie) &&
	!(SetCookie2 && *SetCookie2)) {
	/*
	 *  Yuk!  Garbage in, so nothing out. - FM
	 */
	return;
    }

    /*
     *	If we have both Set-Cookie and Set-Cookie2 headers.
     *	process the Set-Cookie2 header.  Otherwise, process
     *	whichever of the two headers we do have.  Note that
     *	if more than one instance of a valued attribute for
     *	the same cookie is encountered, the value for the
     *	first instance is retained.  We only accept up to 50
     *	cookies from the header, and only if a cookie's values
     *	do not exceed the 4096 byte limit on overall size. - FM
     */
    CombinedCookies = HTList_new();

    /*
     *	Process the Set-Cookie2 header, if present and not zero-length,
     *	adding each cookie to the CombinedCookies list. - FM
     */
    p = (SetCookie2 ? SetCookie2 : "");
    if (TRACE && SetCookie && *p) {
	fprintf(stderr, "LYProcessSetCookies: Using Set-Cookie2 header.\n");
    }
    while (NumCookies <= 50 && *p) {
	attr_start = attr_end = value_start = value_end = NULL;
	while (*p != '\0' && isspace((unsigned char)*p)) {
	    p++;
	}
	/*
	 *  Get the attribute name.
	 */
	attr_start = p;
	while (*p != '\0' && !isspace((unsigned char)*p) &&
	       *p != '=' && *p != ';' && *p != ',')
	    p++;
	attr_end = p;
	while (*p != '\0' && isspace((unsigned char)*p)) {
	    p++;
	}

	/*
	 *  Check for an '=' delimiter, or an 'expires' name followed
	 *  by white, since Netscape's bogus parser doesn't require
	 *  an '=' delimiter, and 'expires' attributes are being
	 *  encountered without them.  These shouldn't be in a
	 *  Set-Cookie2 header, but we'll assume it's an expires
	 *  attribute rather a cookie with that name, since the
	 *  attribute mistake rather than name mistake seems more
	 *  likely to be made by providers. - FM
	 */
	if (*p == '=' ||
	     !strncasecomp(attr_start, "Expires", 7)) {
	    /*
	     *	Get the value string.
	     */
	    if (*p == '=') {
		p++;
	    }
	    while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	    }
	    /*
	     *	Hack alert!  We must handle Netscape-style cookies with
	     *		"Expires=Mon, 01-Jan-96 13:45:35 GMT" or
	     *		"Expires=Mon,  1 Jan 1996 13:45:35 GMT".
	     *	No quotes, but there are spaces.  Argh...
	     *	Anyway, we know it will have at least 3 space separators
	     *	within it, and two dashes or two more spaces, so this code
	     *	looks for a space after the 5th space separator or dash to
	     *	mark the end of the value. - FM
	     */
	    if ((attr_end - attr_start) == 7 &&
		!strncasecomp(attr_start, "Expires", 7)) {
		int spaces = 6;
		value_start = p;
		if (isdigit((unsigned char)*p)) {
		    /*
		     *	No alphabetic day field. - FM
		     */
		    spaces--;
		} else {
		    /*
		     *	Skip the alphabetic day field. - FM
		     */
		    while (*p != '\0' && isalpha((unsigned char)*p)) {
			p++;
		    }
		    while (*p == ',' || isspace((unsigned char)*p)) {
			p++;
		    }
		    spaces--;
		}
		while (*p != '\0' && *p != ';' && *p != ',' && spaces) {
		    p++;
		    if (isspace((unsigned char)*p)) {
			while (isspace((unsigned char)*(p + 1)))
			    p++;
			spaces--;
		    } else if (*p == '-') {
			spaces--;
		    }
		}
		value_end = p;
	    /*
	     *	Hack Alert!  The port attribute can take a
	     *	comma separated list of numbers as a value,
	     *	and such values should be quoted, but if
	     *	not, make sure we don't treat a number in
	     *	the list as the start of a new cookie. - FM
	     */
	    } else if ((attr_end - attr_start) == 4 &&
		       !strncasecomp(attr_start, "port", 4) &&
		       isdigit((unsigned char)*p)) {
		/*
		 *  The value starts as an unquoted number.
		 */
		CONST char *cp, *cp1;
		value_start = p;
		while (1) {
		    while (isdigit((unsigned char)*p))
			p++;
		    value_end = p;
		    while (isspace((unsigned char)*p))
			p++;
		    if (*p == '\0' || *p == ';')
			break;
		    if (*p == ',') {
			cp = (p + 1);
			while (*cp != '\0' && isspace((unsigned char)*cp))
			    cp++;
			if (*cp != '\0' && isdigit((unsigned char)*cp)) {
			    cp1 = cp;
			    while (isdigit((unsigned char)*cp1))
				cp1++;
			    while (*cp != '\0' && isspace((unsigned char)*cp))
				cp1++;
			    if (*cp1 == '\0' || *cp1 == ',' || *cp1 == ';') {
				p = cp;
				continue;
			    }
			}
		    }
		    while (*p != '\0' && *p != ';' && *p != ',')
			p++;
		    value_end = p;
		    /*
		     *	Trim trailing spaces.
		     */
		    if ((value_end > value_start) &&
			isspace((unsigned char)*(value_end - 1))) {
			value_end--;
			while ((value_end > (value_start + 1)) &&
			       isspace((unsigned char)*value_end) &&
			       isspace((unsigned char)*(value_end - 1))) {
			    value_end--;
			}
		    }
		    break;
		}
	    } else if (*p == '"') {
		/*
		 *  It's a quoted string.
		 */
		p++;
		value_start = p;
		while (*p != '\0' && *p != '"')
		    p++;
		value_end = p;
		if (*p == '"')
		    p++;
	    } else {
		/*
		 *  Otherwise, it's an unquoted string.
		 */
		value_start = p;
		while (*p != '\0' && *p != ';' && *p != ',')
		    p++;
		value_end = p;
		/*
		 *  Trim trailing spaces.
		 */
		if ((value_end > value_start) &&
		    isspace((unsigned char)*(value_end - 1))) {
		    value_end--;
		    while ((value_end > (value_start + 1)) &&
			   isspace((unsigned char)*value_end) &&
			   isspace((unsigned char)*(value_end - 1))) {
			value_end--;
		    }
		}
	    }
	}

	/*
	 *  Check for a separator character, and skip it.
	 */
	if (*p == ';' || *p == ',')
	    p++;

	/*
	 *  Now, we can handle this attribute/value pair.
	 */
	if (attr_end > attr_start) {
	    int len = (attr_end - attr_start);
	    BOOLEAN known_attr = NO;
	    char *value = NULL;

	    if (value_end > value_start) {
		int value_len = (value_end - value_start);

		if (value_len > 4096) {
		    value_len = 4096;
		}
		value = (char *)calloc(1, value_len + 1);
		if (value == NULL)
		    outofmem(__FILE__, "LYProcessSetCookies");
		LYstrncpy(value, value_start, value_len);
	    }
	    if (len == 6 && !strncasecomp(attr_start, "secure", 6)) {
		if (value == NULL) {
		    known_attr = YES;
		    if (cur_cookie != NULL) {
			cur_cookie->flags |= COOKIE_FLAG_SECURE;
		    }
		} else {
		    /*
		     *	If secure has a value, assume someone
		     *	misused it as cookie name. - FM
		     */
		    known_attr = NO;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "discard", 7)) {
		if (value == NULL) {
		    known_attr = YES;
		    if (cur_cookie != NULL) {
			cur_cookie->flags |= COOKIE_FLAG_DISCARD;
		    }
		} else {
		    /*
		     *	If discard has a value, assume someone
		     *	used it as a cookie name. - FM
		     */
		    known_attr = NO;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "comment", 7)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat comment. - FM
		     */
		    cur_cookie->comment == NULL) {
		    StrAllocCopy(cur_cookie->comment, value);
		    length += strlen(cur_cookie->comment);
		}
	    } else if (len == 10 && !strncasecomp(attr_start,
						  "commentURL", 10)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat commentURL. - FM
		     */
		    cur_cookie->commentURL == NULL) {
		    /*
		     *	We should get only absolute URLs as
		     *	values, but will resolve versus the
		     *	request's URL just in case. - FM
		     */
		    cur_cookie->commentURL = HTParse(value,
						     address,
						     PARSE_ALL);
		    /*
		     *	Accept only URLs for http or https servers. - FM
		     */
		    if ((url_type = is_url(cur_cookie->commentURL)) &&
			(url_type == HTTP_URL_TYPE ||
			 url_type == HTTPS_URL_TYPE)) {
			length += strlen(cur_cookie->commentURL);
		    } else {
			if (TRACE)
			    fprintf(stderr,
		     "LYProcessSetCookies: Rejecting commentURL value '%s'\n",
				    cur_cookie->commentURL);
			FREE(cur_cookie->commentURL);
		    }
		}
	    } else if (len == 6 && !strncasecomp(attr_start, "domain", 6)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat domain. - FM
		     */
		    !(cur_cookie->flags & COOKIE_FLAG_DOMAIN_SET)) {
		    length -= strlen(cur_cookie->domain);
		    /*
		     *	If the value does not have a lead dot,
		     *	but does have an embedded dot, and is
		     *	not an exact match to the hostname, nor
		     *	is a numeric IP address, add a lead dot.
		     *	Otherwise, use the value as is. - FM
		     */
		    if (value[0] != '.' && value[0] != '\0' &&
			value[1] != '\0' && strcmp(value, hostname)) {
			char *ptr = strchr(value, '.');
			if (ptr != NULL && ptr[1] != '\0') {
			    ptr = value;
			    while (*ptr == '.' ||
				   isdigit((unsigned char)*ptr))
				ptr++;
			    if (*ptr != '\0') {
				if (TRACE) {
				    fprintf(stderr,
	       "LYProcessSetCookies: Adding lead dot for domain value '%s'\n",
					    value);
				}
				StrAllocCopy(cur_cookie->domain, ".");
				StrAllocCat(cur_cookie->domain, value);
			    } else {
				StrAllocCopy(cur_cookie->domain, value);
			    }
			} else {
			    StrAllocCopy(cur_cookie->domain, value);
			}
		    } else {
			StrAllocCopy(cur_cookie->domain, value);
		    }
		    length += strlen(cur_cookie->domain);
		    cur_cookie->flags |= COOKIE_FLAG_DOMAIN_SET;
		}
	    } else if (len == 4 && !strncasecomp(attr_start, "path", 4)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat path. - FM
		     */
		    !(cur_cookie->flags & COOKIE_FLAG_PATH_SET)) {
		    length -= strlen(cur_cookie->path);
		    StrAllocCopy(cur_cookie->path, value);
		    length += (cur_cookie->pathlen = strlen(cur_cookie->path));
		    cur_cookie->flags |= COOKIE_FLAG_PATH_SET;
		}
	    } else if (len == 4 && !strncasecomp(attr_start, "port", 4)) {
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat port. - FM
		     */
		    cur_cookie->PortList == NULL) {
		    char *cp = value;
		    while ((*cp != '\0') &&
			   (isdigit((unsigned char)*cp) ||
			    *cp == ',' || *cp == ' ')) {
			cp++;
		    }
		    if (*cp == '\0') {
			StrAllocCopy(cur_cookie->PortList, value);
			length += strlen(cur_cookie->PortList);
			known_attr = YES;
		    } else {
			known_attr = NO;
		    }
		} else if (cur_cookie != NULL) {
		    /*
		     *	Don't process a repeat port. - FM
		     */
		    if (cur_cookie->PortList == NULL) {
			char temp[256];
			sprintf(temp, "%d", port);
			StrAllocCopy(cur_cookie->PortList, temp);
			length += strlen(cur_cookie->PortList);
		    }
		    known_attr = YES;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "version", 7)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat version. - FM
		     */
		    cur_cookie->version < 1) {
		    int temp = strtol(value, NULL, 10);
		    if (errno != -ERANGE) {
			cur_cookie->version = temp;
		    }
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "max-age", 7)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat max-age. - FM
		     */
		    !MaxAgeAttrSet) {
		    int temp = strtol(value, NULL, 10);
		    cur_cookie->flags |= COOKIE_FLAG_EXPIRES_SET;
		    if (errno == -ERANGE) {
			cur_cookie->expires = (time_t)0;
		    } else {
			cur_cookie->expires = (time(NULL) + temp);
			if (TRACE)
			    fprintf(stderr,
				    "LYSetCookie: expires %ld, %s",
				    (long) cur_cookie->expires,
				    ctime(&cur_cookie->expires));
		    }
		    MaxAgeAttrSet = TRUE;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "expires", 7)) {
		/*
		 *  Convert an 'expires' attribute value if we haven't
		 *  received a 'max-age'.  Note that 'expires' should not
		 *  be used in Version 1 cookies, but it might be used for
		 *  "backward compatibility", and, in turn, ill-informed
		 *  people surely would start using it instead of, rather
		 *  than in addition to, 'max-age'. - FM
		 */
		known_attr = YES;
		if ((cur_cookie != NULL && !MaxAgeAttrSet) &&
		     !(cur_cookie->flags & COOKIE_FLAG_EXPIRES_SET)) {
		    known_attr = YES;
		    if (value) {
			cur_cookie->flags |= COOKIE_FLAG_EXPIRES_SET;
			cur_cookie->expires = LYmktime(value, FALSE);
			if (cur_cookie->expires > 0) {
			    if (TRACE)
				fprintf(stderr,
					"LYSetCookie: expires %ld, %s",
					(long) cur_cookie->expires,
					ctime(&cur_cookie->expires));
			}
		    }
		}
	    }

	    /*
	     *	If none of the above comparisons succeeded, and we have
	     *	a value, then we have an unknown pair of the form 'foo=bar',
	     *	which means it's time to create a new cookie.  If we don't
	     *	have a non-zero-length value, assume it's an error or a
	     *	new, unknown attribute which doesn't take a value, and
	     *	ignore it. - FM
	     */
	    if (!known_attr && value_end > value_start) {
		/*
		 *  If we've started a cookie, and it's not too big,
		 *  save it in the CombinedCookies list. - FM
		 */
		if (length <= 4096 && cur_cookie != NULL) {
		    /*
		     *	Assume version 1 if not set to that or higher. - FM
		     */
		    if (cur_cookie->version < 1) {
			cur_cookie->version = 1;
		    }
		    HTList_appendObject(CombinedCookies, cur_cookie);
		} else if (cur_cookie != NULL) {
		    if (TRACE) {
			fprintf(stderr,
			"LYProcessSetCookies: Rejecting Set-Cookie2: %s=%s\n",
				(cur_cookie->name ?
				 cur_cookie->name : "[no name]"),
				(cur_cookie->value ?
				 cur_cookie->value : "[no value]"));
			fprintf(stderr,
			"                     due to excessive length!\n");
		    }
		    freeCookie(cur_cookie);
		    cur_cookie = NULL;
		}
		/*
		 *  Start a new cookie. - FM
		 */
		cur_cookie = newCookie();
		length = 0;
		NumCookies++;
		MemAllocCopy(&(cur_cookie->name), attr_start, attr_end);
		length += strlen(cur_cookie->name);
		MemAllocCopy(&(cur_cookie->value), value_start, value_end);
		length += strlen(cur_cookie->value);
		StrAllocCopy(cur_cookie->domain, hostname);
		length += strlen(cur_cookie->domain);
		StrAllocCopy(cur_cookie->path, path);
		length += (cur_cookie->pathlen = strlen(cur_cookie->path));
		cur_cookie->port = port;
		MaxAgeAttrSet = FALSE;
		cur_cookie->quoted = TRUE;
	    }
	    FREE(value);
	}
    }

    /*
     *	Add any final SetCookie2 cookie to the CombinedCookie list
     *	if we are within the length limit. - FM
     */
    if (NumCookies <= 50 && length <= 4096 && cur_cookie != NULL) {
	if (cur_cookie->version < 1) {
	    cur_cookie->version = 1;
	}
	HTList_appendObject(CombinedCookies, cur_cookie);
    } else if (cur_cookie != NULL) {
	if (TRACE) {
	    fprintf(stderr,
	 "LYProcessSetCookies: Rejecting Set-Cookie2: %s=%s\n",
		    (cur_cookie->name ? cur_cookie->name : "[no name]"),
		    (cur_cookie->value ? cur_cookie->value : "[no value]"));
	    fprintf(stderr,
	 "                     due to excessive %s%s%s\n",
		    (length > 4096 ? "length" : ""),
		    (length > 4096 && NumCookies > 50 ? " and " : ""),
		    (NumCookies > 50 ? "number!\n" : "!\n"));
	}
	freeCookie(cur_cookie);
	cur_cookie = NULL;
    }

    /*
     *	Process the Set-Cookie header, if no non-zero-length Set-Cookie2
     *	header was present. - FM
     */
    length = 0;
    NumCookies = 0;
    cur_cookie = NULL;
    p = ((SetCookie && !(SetCookie2 && *SetCookie2)) ? SetCookie : "");
    if (TRACE && SetCookie2 && *p) {
	fprintf(stderr, "LYProcessSetCookies: Using Set-Cookie header.\n");
    }
    while (NumCookies <= 50 && *p) {
	attr_start = attr_end = value_start = value_end = NULL;
	while (*p != '\0' && isspace((unsigned char)*p)) {
	    p++;
	}
	/*
	 *  Get the attribute name.
	 */
	attr_start = p;
	while (*p != '\0' && !isspace((unsigned char)*p) &&
	       *p != '=' && *p != ';' && *p != ',')
	    p++;
	attr_end = p;
	while (*p != '\0' && isspace((unsigned char)*p)) {
	    p++;
	}

	/*
	 *  Check for an '=' delimiter, or an 'expires' name followed
	 *  by white, since Netscape's bogus parser doesn't require
	 *  an '=' delimiter, and 'expires' attributes are being
	 *  encountered without them. - FM
	 */
	if (*p == '=' ||
	     !strncasecomp(attr_start, "Expires", 7)) {
	    /*
	     *	Get the value string.
	     */
	    if (*p == '=') {
		p++;
	    }
	    while (*p != '\0' && isspace((unsigned char)*p)) {
		p++;
	    }
	    /*
	     *	Hack alert!  We must handle Netscape-style cookies with
	     *		"Expires=Mon, 01-Jan-96 13:45:35 GMT" or
	     *		"Expires=Mon,  1 Jan 1996 13:45:35 GMT".
	     *	No quotes, but there are spaces.  Argh...
	     *	Anyway, we know it will have at least 3 space separators
	     *	within it, and two dashes or two more spaces, so this code
	     *	looks for a space after the 5th space separator or dash to
	     *	mark the end of the value. - FM
	     */
	    if ((attr_end - attr_start) == 7 &&
		!strncasecomp(attr_start, "Expires", 7)) {
		int spaces = 6;
		value_start = p;
		if (isdigit((unsigned char)*p)) {
		    /*
		     *	No alphabetic day field. - FM
		     */
		    spaces--;
		} else {
		    /*
		     *	Skip the alphabetic day field. - FM
		     */
		    while (*p != '\0' && isalpha((unsigned char)*p)) {
			p++;
		    }
		    while (*p == ',' || isspace((unsigned char)*p)) {
			p++;
		    }
		    spaces--;
		}
		while (*p != '\0' && *p != ';' && *p != ',' && spaces) {
		    p++;
		    if (isspace((unsigned char)*p)) {
			while (isspace((unsigned char)*(p + 1)))
			    p++;
			spaces--;
		    } else if (*p == '-') {
			spaces--;
		    }
		}
		value_end = p;
	    /*
	     *	Hack Alert!  The port attribute can take a
	     *	comma separated list of numbers as a value,
	     *	and such values should be quoted, but if
	     *	not, make sure we don't treat a number in
	     *	the list as the start of a new cookie. - FM
	     */
	    } else if ((attr_end - attr_start) == 4 &&
		       !strncasecomp(attr_start, "port", 4) &&
		       isdigit((unsigned char)*p)) {
		/*
		 *  The value starts as an unquoted number.
		 */
		CONST char *cp, *cp1;
		value_start = p;
		while (1) {
		    while (isdigit((unsigned char)*p))
			p++;
		    value_end = p;
		    while (isspace((unsigned char)*p))
			p++;
		    if (*p == '\0' || *p == ';')
			break;
		    if (*p == ',') {
			cp = (p + 1);
			while (*cp != '\0' && isspace((unsigned char)*cp))
			    cp++;
			if (*cp != '\0' && isdigit((unsigned char)*cp)) {
			    cp1 = cp;
			    while (isdigit((unsigned char)*cp1))
				cp1++;
			    while (*cp != '\0' && isspace((unsigned char)*cp))
				cp1++;
			    if (*cp1 == '\0' || *cp1 == ',' || *cp1 == ';') {
				p = cp;
				continue;
			    }
			}
		    }
		    while (*p != '\0' && *p != ';' && *p != ',')
			p++;
		    value_end = p;
		    /*
		     *	Trim trailing spaces.
		     */
		    if ((value_end > value_start) &&
			isspace((unsigned char)*(value_end - 1))) {
			value_end--;
			while ((value_end > (value_start + 1)) &&
			       isspace((unsigned char)*value_end) &&
			       isspace((unsigned char)*(value_end - 1))) {
			    value_end--;
			}
		    }
		    break;
		}
	    } else if (*p == '"') {
		/*
		 *  It's a quoted string.
		 */
		p++;
		value_start = p;
		while (*p != '\0' && *p != '"')
		    p++;
		value_end = p;
		if (*p == '"')
		    p++;
		Quoted = TRUE;
	    } else {
		/*
		 *  Otherwise, it's an unquoted string.
		 */
		value_start = p;
		while (*p != '\0' && *p != ';' && *p != ',')
		    p++;
		value_end = p;
		/*
		 *  Trim trailing spaces.
		 */
		if ((value_end > value_start) &&
		    isspace((unsigned char)*(value_end - 1))) {
		    value_end--;
		    while ((value_end > (value_start + 1)) &&
			   isspace((unsigned char)*value_end) &&
			   isspace((unsigned char)*(value_end - 1))) {
			value_end--;
		    }
		}
	    }
	}

	/*
	 *  Check for a separator character, and skip it.
	 */
	if (*p == ';' || *p == ',')
	    p++;

	/*
	 *  Now, we can handle this attribute/value pair.
	 */
	if (attr_end > attr_start) {
	    int len = (attr_end - attr_start);
	    BOOLEAN known_attr = NO;
	    char *value = NULL;

	    if (value_end > value_start) {
		int value_len = (value_end - value_start);

		if (value_len > 4096) {
		    value_len = 4096;
		}
		value = (char *)calloc(1, value_len + 1);
		if (value == NULL)
		    outofmem(__FILE__, "LYProcessSetCookie");
		LYstrncpy(value, value_start, value_len);
	    }
	    if (len == 6 && !strncasecomp(attr_start, "secure", 6)) {
		if (value == NULL) {
		    known_attr = YES;
		    if (cur_cookie != NULL) {
			cur_cookie->flags |= COOKIE_FLAG_SECURE;
		    }
		} else {
		    /*
		     *	If secure has a value, assume someone
		     *	misused it as cookie name. - FM
		     */
		    known_attr = NO;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "discard", 7)) {
		if (value == NULL) {
		    known_attr = YES;
		    if (cur_cookie != NULL) {
			cur_cookie->flags |= COOKIE_FLAG_DISCARD;
		    }
		} else {
		    /*
		     *	If discard has a value, assume someone
		     *	used it as a cookie name. - FM
		     */
		    known_attr = NO;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "comment", 7)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat comment. - FM
		     */
		    cur_cookie->comment == NULL) {
		    StrAllocCopy(cur_cookie->comment, value);
		    length += strlen(cur_cookie->comment);
		}
	    } else if (len == 10 && !strncasecomp(attr_start,
						  "commentURL", 10)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat commentURL. - FM
		     */
		    cur_cookie->commentURL == NULL) {
		    /*
		     *	We should get only absolute URLs as
		     *	values, but will resolve versus the
		     *	request's URL just in case. - FM
		     */
		    cur_cookie->commentURL = HTParse(value,
						     address,
						     PARSE_ALL);
		    /*
		     *	Accept only URLs for http or https servers. - FM
		     */
		    if ((url_type = is_url(cur_cookie->commentURL)) &&
			(url_type == HTTP_URL_TYPE ||
			 url_type == HTTPS_URL_TYPE)) {
			length += strlen(cur_cookie->commentURL);
		    } else {
			if (TRACE)
			    fprintf(stderr,
		     "LYProcessSetCookies: Rejecting commentURL value '%s'\n",
				    cur_cookie->commentURL);
			FREE(cur_cookie->commentURL);
		    }
		}
	    } else if (len == 6 && !strncasecomp(attr_start, "domain", 6)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat domain. - FM
		     */
		    !(cur_cookie->flags & COOKIE_FLAG_DOMAIN_SET)) {
		    length -= strlen(cur_cookie->domain);
		    /*
		     *	If the value does not have a lead dot,
		     *	but does have an embedded dot, and is
		     *	not an exact match to the hostname, nor
		     *	is a numeric IP address, add a lead dot.
		     *	Otherwise, use the value as is. - FM
		     */
		    if (value[0] != '.' && value[0] != '\0' &&
			value[1] != '\0' && strcmp(value, hostname)) {
			char *ptr = strchr(value, '.');
			if (ptr != NULL && ptr[1] != '\0') {
			    ptr = value;
			    while (*ptr == '.' ||
				   isdigit((unsigned char)*ptr))
				ptr++;
			    if (*ptr != '\0') {
				if (TRACE) {
				    fprintf(stderr,
	       "LYProcessSetCookies: Adding lead dot for domain value '%s'\n",
					    value);
				}
				StrAllocCopy(cur_cookie->domain, ".");
				StrAllocCat(cur_cookie->domain, value);
			    } else {
				StrAllocCopy(cur_cookie->domain, value);
			    }
			} else {
			    StrAllocCopy(cur_cookie->domain, value);
			}
		    } else {
			StrAllocCopy(cur_cookie->domain, value);
		    }
		    length += strlen(cur_cookie->domain);
		    cur_cookie->flags |= COOKIE_FLAG_DOMAIN_SET;
		}
	    } else if (len == 4 && !strncasecomp(attr_start, "path", 4)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat path. - FM
		     */
		    !(cur_cookie->flags & COOKIE_FLAG_PATH_SET)) {
		    length -= strlen(cur_cookie->path);
		    StrAllocCopy(cur_cookie->path, value);
		    length += (cur_cookie->pathlen = strlen(cur_cookie->path));
		    cur_cookie->flags |= COOKIE_FLAG_PATH_SET;
		}
	    } else if (len == 4 && !strncasecomp(attr_start, "port", 4)) {
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat port. - FM
		     */
		    cur_cookie->PortList == NULL) {
		    char *cp = value;
		    while ((*cp != '\0') &&
			   (isdigit((unsigned char)*cp) ||
			    *cp == ',' || *cp == ' ')) {
			cp++;
		    }
		    if (*cp == '\0') {
			StrAllocCopy(cur_cookie->PortList, value);
			length += strlen(cur_cookie->PortList);
			known_attr = YES;
		    } else {
			known_attr = NO;
		    }
		} else if (cur_cookie != NULL) {
		    /*
		     *	Don't process a repeat port. - FM
		     */
		    if (cur_cookie->PortList == NULL) {
			char temp[256];
			sprintf(temp, "%d", port);
			StrAllocCopy(cur_cookie->PortList, temp);
			length += strlen(cur_cookie->PortList);
		    }
		    known_attr = YES;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "version", 7)) {
		known_attr = YES;
		if (cur_cookie != NULL && value &&
		    /*
		     *	Don't process a repeat version. - FM
		     */
		    cur_cookie->version < 0) {
		    int temp = strtol(value, NULL, 10);
		    if (errno != -ERANGE) {
			cur_cookie->version = temp;
		    }
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "max-age", 7)) {
		known_attr = YES;
		if ((cur_cookie != NULL) && !MaxAgeAttrSet && value) {
		    int temp = strtol(value, NULL, 10);
		    cur_cookie->flags |= COOKIE_FLAG_EXPIRES_SET;
		    if (errno == -ERANGE) {
			cur_cookie->expires = (time_t)0;
		    } else {
			cur_cookie->expires = (time(NULL) + temp);
		    }
		    MaxAgeAttrSet = TRUE;
		}
	    } else if (len == 7 && !strncasecomp(attr_start, "expires", 7)) {
		/*
		 *  Convert an 'expires' attribute value if we haven't
		 *  received a 'max-age'.  Note that 'expires' should not
		 *  be used in Version 1 cookies, but it might be used for
		 *  "backward compatibility", and, in turn, ill-informed
		 *  people surely would start using it instead of, rather
		 *  than in addition to, 'max-age'. - FM
		 */
		known_attr = YES;
		if ((cur_cookie != NULL) && !(MaxAgeAttrSet) &&
		    !(cur_cookie->flags & COOKIE_FLAG_EXPIRES_SET)) {
		    if (value) {
			cur_cookie->flags |= COOKIE_FLAG_EXPIRES_SET;
			cur_cookie->expires = LYmktime(value, FALSE);
		    }
		}
	    }

	    /*
	     *	If none of the above comparisons succeeded, and we have
	     *	a value, then we have an unknown pair of the form 'foo=bar',
	     *	which means it's time to create a new cookie.  If we don't
	     *	have a non-zero-length value, assume it's an error or a
	     *	new, unknown attribute which doesn't take a value, and
	     *	ignore it. - FM
	     */
	    if (!known_attr && value_end > value_start) {
		/*
		 *  If we've started a cookie, and it's not too big,
		 *  save it in the CombinedCookies list. - FM
		 */
		if (length <= 4096 && cur_cookie != NULL) {
		    /*
		     *	If we had a Set-Cookie2 header, make sure
		     *	the version is at least 1, and mark it for
		     *	quoting. - FM
		     */
		    if (SetCookie2 != NULL) {
			if (cur_cookie->version < 1) {
			    cur_cookie->version = 1;
			}
			cur_cookie->quoted = TRUE;
		    }
		    HTList_appendObject(CombinedCookies, cur_cookie);
		} else if (cur_cookie != NULL) {
		    if (TRACE) {
			fprintf(stderr,
			"LYProcessSetCookies: Rejecting Set-Cookie: %s=%s\n",
				(cur_cookie->name ?
				 cur_cookie->name : "[no name]"),
				(cur_cookie->value ?
				 cur_cookie->value : "[no value]"));
			fprintf(stderr,
			"                     due to excessive length!\n");
		    }
		    freeCookie(cur_cookie);
		    cur_cookie = NULL;
		}
		/*
		 *  Start a new cookie. - FM
		 */
		cur_cookie = newCookie();
		length = 0;
		MemAllocCopy(&(cur_cookie->name), attr_start, attr_end);
		length += strlen(cur_cookie->name);
		MemAllocCopy(&(cur_cookie->value), value_start, value_end);
		length += strlen(cur_cookie->value);
		StrAllocCopy(cur_cookie->domain, hostname);
		length += strlen(cur_cookie->domain);
		StrAllocCopy(cur_cookie->path, path);
		length += (cur_cookie->pathlen = strlen(cur_cookie->path));
		cur_cookie->port = port;
		MaxAgeAttrSet = FALSE;
		cur_cookie->quoted = Quoted;
		Quoted = FALSE;
	    }
	    FREE(value);
	}
    }

    /*
     *	Handle the final Set-Cookie cookie if within length limit. - FM
     */
    if (NumCookies <= 50 && length <= 4096 && cur_cookie != NULL) {
	if (SetCookie2 != NULL) {
	    if (cur_cookie->version < 1) {
		cur_cookie->version = 1;
	    }
	    cur_cookie->quoted = TRUE;
	}
	HTList_appendObject(CombinedCookies, cur_cookie);
    } else if (cur_cookie != NULL) {
	if (TRACE) {
	    fprintf(stderr,
	  "LYProcessSetCookies: Rejecting Set-Cookie: %s=%s\n",
		    (cur_cookie->name ? cur_cookie->name : "[no name]"),
		    (cur_cookie->value ? cur_cookie->value : "[no value]"));
	    fprintf(stderr,
	  "                     due to excessive %s%s%s\n",
		    (length > 4096 ? "length" : ""),
		    (length > 4096 && NumCookies > 50 ? " and " : ""),
		    (NumCookies > 50 ? "number!\n" : "!\n"));
	}
	freeCookie(cur_cookie);
	cur_cookie = NULL;
    }

    /*
     *	OK, now we can actually store any cookies
     *	in the CombinedCookies list. - FM
     */
    cl = CombinedCookies;
    while (NULL != (co = (cookie *)HTList_nextObject(cl))) {
	if (TRACE) {
	    fprintf(stderr, "LYProcessSetCookie: attr=value pair: '%s=%s'\n",
			    (co->name ? co->name : "[no name]"),
			    (co->value ? co->value : "[no value]"));
	    if (co->expires > 0) {
		fprintf(stderr, "                    expires: %ld, %s\n",
				 (long)co->expires,
				 ctime(&co->expires));
	    }
	}
	if (!strncasecomp(address, "https:", 6) &&
	    LYForceSSLCookiesSecure == TRUE &&
	    !(co->flags & COOKIE_FLAG_SECURE)) {
	    co->flags |= COOKIE_FLAG_SECURE;
	    if (TRACE) {
		fprintf(stderr,
			"                    Forced the 'secure' flag on.\n");
	    }
	}
	store_cookie(co, hostname, path);
    }
    HTList_delete(CombinedCookies);
    CombinedCookies = NULL;

    return;
}

/*
**  Entry function for handling Set-Cookie: and/or Set-Cookie2:
**  reply headers.   They may have been concatenated as comma
**  separated lists in HTTP.c or HTMIME.c. - FM
*/
PUBLIC void LYSetCookie ARGS3(
	CONST char *,	SetCookie,
	CONST char *,	SetCookie2,
	CONST char *,	address)
{
    BOOL BadHeaders = FALSE;
    char *hostname = NULL, *path = NULL, *ptr;
    int port = 80;

    /*
     *	Get the hostname, port and path of the address, and report
     *	the Set-Cookie and/or Set-Cookie2 header(s) if trace mode is
     *	on, but set the cookie(s) only if LYSetCookies is TRUE. - FM
     */
    if (((hostname = HTParse(address, "", PARSE_HOST)) != NULL) &&
	(ptr = strchr(hostname, ':')) != NULL)	{
	/*
	 *  Replace default port number.
	 */
	*ptr = '\0';
	ptr++;
	port = atoi(ptr);
    } else if (!strncasecomp(address, "https:", 6)) {
	port = 443;
    }
    if (((path = HTParse(address, "",
			 PARSE_PATH|PARSE_PUNCTUATION)) != NULL) &&
	(ptr = strrchr(path, '/')) != NULL) {
	if (ptr == path) {
	    *(ptr+1) = '\0';	/* Leave a single '/' alone */
	} else {
	    *ptr = '\0';
	}
    }
    if (!(SetCookie && *SetCookie) &&
	!(SetCookie2 && *SetCookie2)) {
	/*
	 *  Yuk, something must have gone wrong in
	 *  HTMIME.c or HTTP.c because both SetCookie
	 *  and SetCookie2 are NULL or zero-length. - FM
	 */
	BadHeaders = TRUE;
    }
    if (TRACE) {
	fprintf(stderr,
		"LYSetCookie called with host '%s', path '%s',\n",
		(hostname ? hostname : ""),
		(path ? path : ""));
	if (SetCookie) {
	    fprintf(stderr, "    and Set-Cookie: '%s'\n",
			    (SetCookie ? SetCookie : ""));
	}
	if (SetCookie2) {
	    fprintf(stderr, "    and Set-Cookie2: '%s'\n",
			    (SetCookie2 ? SetCookie2 : ""));
	}
	if (LYSetCookies == FALSE || BadHeaders == TRUE) {
	    fprintf(stderr,
		    "    Ignoring this Set-Cookie/Set-Cookie2 request.\n");
	}
    }

    /*
     *	We're done if LYSetCookies is off or we have bad headers. - FM
     */
    if (LYSetCookies == FALSE || BadHeaders == TRUE) {
	FREE(hostname);
	FREE(path);
	return;
    }

    /*
     *	Process the header(s).
     */
    LYProcessSetCookies(SetCookie, SetCookie2, address, hostname, path, port);
    FREE(hostname);
    FREE(path);
    return;
}

/*
**  Entry function from creating a Cookie: request header
**  if needed. - AK & FM
*/
PUBLIC char * LYCookie ARGS4(
	CONST char *,	hostname,
	CONST char *,	path,
	int,		port,
	BOOL,		secure)
{
    char *header = NULL;
    HTList *hl = domain_list, *next = NULL;
    domain_entry *de;

    if (TRACE) {
	fprintf(stderr,
		"LYCookie: Searching for '%s:%d', '%s'.\n",
		(hostname ? hostname : "(null)"),
		port,
		(path ? path : "(null)"));
    }

    /*
     *	Search the cookie_list elements in the domain_list
     *	for any cookies associated with the //hostname:port/path
     */
    while (hl) {
	de = (domain_entry *)hl->object;
	next = hl->next;

	if (de != NULL) {
	    if (!HTList_isEmpty(de->cookie_list)) {
		/*
		 *  Scan the domain's cookie_list for
		 *  any cookies we should include in
		 *  our request header.
		 */
		header = scan_cookie_sublist(hostname, path, port,
					     de->cookie_list, header, secure);
	    } else if (de->bv == QUERY_USER) {
		/*
		 *  No cookies in this domain, and no default
		 *  accept/reject choice was set by the user,
		 *  so delete the domain. - FM
		 */
		FREE(de->domain);
		HTList_delete(de->cookie_list);
		de->cookie_list = NULL;
		HTList_removeObject(domain_list, de);
		de = NULL;
	    }
	}
	hl = next;
    }
    if (header)
	return(header);

    /*
     *	If we didn't set a header, perhaps all the cookies have
     *	expired and we deleted the last of them above, so check
     *	if we should delete and NULL the domain_list. - FM
     */
    if (domain_list) {
	if (HTList_isEmpty(domain_list)) {
	    HTList_delete(domain_list);
	    domain_list = NULL;
	}
    }
    return(NULL);
}

/*	LYHandleCookies - F.Macrides (macrides@sci.wfeb.edu)
**	---------------
**
**  Lists all cookies by domain, and allows deletions of
**  individual cookies or entire domains, and changes of
**  'allow' settings.  The list is invoked via the COOKIE_JAR
**  command (Ctrl-K), and deletions or changes of 'allow'
**  settings are done by activating links in that list.
**  The procedure uses a LYNXCOOKIE: internal URL scheme.
**
**  Semantics:
**	LYNXCOOKIE:/			Create and load the Cookie Jar Page.
**	LYNXCOOKIE://domain		Manipulate the domain.
**	LYNXCOOKIE://domain/lynxID	Delete cookie with lynxID in domain.
**
**	New functions can be added as extensions to the path, and/or by
**	assigning meanings to ;parameters, a ?searchpart, and/or #fragments.
*/
PRIVATE int LYHandleCookies ARGS4 (
	CONST char *,		arg,
	HTParentAnchor *,	anAnchor,
	HTFormat,		format_out,
	HTStream*,		sink)
{
    HTFormat format_in = WWW_HTML;
    HTStream *target = NULL;
    char buf[1024];
    char *domain = NULL;
    char *lynxID = NULL;
    HTList *dl, *cl, *next;
    domain_entry *de;
    cookie *co;
    char *name = NULL, *value = NULL, *path = NULL;
    char *comment = NULL, *Address = NULL, *Title = NULL;
    int ch;
#ifdef VMS
    extern BOOLEAN HadVMSInterrupt;
#endif /* VMS */

    /*
     *	Check whether we have something to do. - FM
     */
    if (domain_list == NULL) {
	HTProgress(COOKIE_JAR_IS_EMPTY);
	sleep(MessageSecs);
	return(HT_NO_DATA);
    }

    /*
     *	If there's a domain string in the "host" field of the
     *	LYNXCOOKIE: URL, this is a request to delete something
     *	or change and 'allow' setting. - FM
     */
    if ((domain = HTParse(arg, "", PARSE_HOST)) != NULL) {
	if (*domain == '\0') {
	    FREE(domain);
	} else {
	    /*
	     *	If there is a path string (not just a slash) in the
	     *	LYNXCOOKIE: URL, that's a cookie's lynxID and this
	     *	is a request to delete it from the Cookie Jar. - FM
	     */
	    if ((lynxID = HTParse(arg, "", PARSE_PATH)) != NULL) {
		if (*lynxID == '\0') {
		    FREE(lynxID);
		}
	    }
	}
    }
    if (domain) {
	/*
	 *  Seek the domain in the domain_list structure. - FM
	 */
	for (dl = domain_list; dl != NULL; dl = dl->next) {
	    de = dl->object;
	    if (!(de && de->domain))
		/*
		 *  First object in the list always is empty. - FM
		 */
		continue;
	    if (!strcmp(domain, de->domain)) {
		/*
		 *  We found the domain.  Check
		 *  whether a lynxID is present. - FM
		 */
		if (lynxID) {
		    /*
		     *	Seek and delete the cookie with this lynxID
		     *	in the domain's cookie list. - FM
		     */
		    for (cl = de->cookie_list; cl != NULL; cl = cl->next) {
			if ((co = (cookie *)cl->object) == NULL)
			    /*
			     *	First object is always empty. - FM
			     */
			    continue;
			if (!strcmp(lynxID, co->lynxID)) {
			    /*
			     *	We found the cookie.
			     *	Delete it if confirmed. - FM
			     */
			    if (HTConfirm(DELETE_COOKIE_CONFIRMATION) == FALSE)
				return(HT_NO_DATA);
			    HTList_removeObject(de->cookie_list, co);
			    freeCookie(co);
			    co = NULL;
			    total_cookies--;
			    if ((de->bv == QUERY_USER &&
				 HTList_isEmpty(de->cookie_list)) &&
				HTConfirm(DELETE_EMPTY_DOMAIN_CONFIRMATION)) {
				/*
				 *  No more cookies in this domain, no
				 *  default accept/reject choice was set
				 *  by the user, and got confirmation on
				 *  deleting the domain, so do it. - FM
				 */
				FREE(de->domain);
				HTList_delete(de->cookie_list);
				de->cookie_list = NULL;
				HTList_removeObject(domain_list, de);
				de = NULL;
				HTProgress(DOMAIN_EATEN);
			    } else {
				HTProgress(COOKIE_EATEN);
			    }
			    sleep(MessageSecs);
			    break;
			}
		    }
		} else {
		    /*
		     *	Prompt whether to delete all of the cookies in
		     *	this domain, or the domain if no cookies in it,
		     *	or to change its 'allow' setting, or to cancel,
		     *	and then act on the user's response. - FM
		     */
		    if (HTList_isEmpty(de->cookie_list)) {
			_statusline(DELETE_DOMAIN_SET_ALLOW_OR_CANCEL);
		    } else {
			_statusline(DELETE_COOKIES_SET_ALLOW_OR_CANCEL);
		    }
		    while (1) {
			ch = LYgetch();
#ifdef VMS
			if (HadVMSInterrupt) {
			    HadVMSInterrupt = FALSE;
			    ch = 'C';
			}
#endif /* VMS */
			switch(TOUPPER(ch)) {
			    case 'A':
				/*
				 *  Set to accept all cookies
				 *  from this domain. - FM
				 */
				de->bv = QUERY_USER;
				_user_message(ALWAYS_ALLOWING_COOKIES,
					      de->domain);
				sleep(MessageSecs);
				return(HT_NO_DATA);

			    case 'C':
			    case 7:	/* Ctrl-G */
			    case 3:	/* Ctrl-C */
				/*
				 *  Cancelled. - FM
				 */
				_statusline(CANCELLED);
				sleep(MessageSecs);
				return(HT_NO_DATA);

			    case 'D':
				if (HTList_isEmpty(de->cookie_list)) {
				    /*
				     *	We had an empty domain, so we
				     *	were asked to delete it. - FM
				     */
				    FREE(de->domain);
				    HTList_delete(de->cookie_list);
				    de->cookie_list = NULL;
				    HTList_removeObject(domain_list, de);
				    de = NULL;
				    HTProgress(DOMAIN_EATEN);
				    sleep(MessageSecs);
				    break;
				}
Delete_all_cookies_in_domain:
				/*
				 *  Delete all cookies in this domain. - FM
				 */
				cl = de->cookie_list;
				while (cl) {
				    next = cl->next;
				    co = cl->object;
				    if (co) {
					HTList_removeObject(de->cookie_list,
							    co);
					freeCookie(co);
					co = NULL;
					total_cookies--;
				    }
				    cl = next;
				}
				HTProgress(DOMAIN_COOKIES_EATEN);
				sleep(MessageSecs);
				/*
				 *  If a default accept/reject
				 *  choice is set, we're done. - FM
				 */
				if (de->bv != QUERY_USER)
				    return(HT_NO_DATA);
				/*
				 *  Check whether to delete
				 *  the empty domain. - FM
				 */
				if(HTConfirm(
					DELETE_EMPTY_DOMAIN_CONFIRMATION)) {
				    FREE(de->domain);
				    HTList_delete(de->cookie_list);
				    de->cookie_list = NULL;
				    HTList_removeObject(domain_list, de);
				    de = NULL;
				    HTProgress(DOMAIN_EATEN);
				    sleep(MessageSecs);
				}
				break;

			    case 'P':
				/*
				 *  Set to prompt for cookie acceptence
				 *  from this domain. - FM
				 */
				de->bv = QUERY_USER;
				_user_message(PROMTING_TO_ALLOW_COOKIES,
					      de->domain);
				sleep(MessageSecs);
				return(HT_NO_DATA);

			    case 'V':
				/*
				 *  Set to reject all cookies
				 *  from this domain. - FM
				 */
				de->bv = REJECT_ALWAYS;
				_user_message(NEVER_ALLOWING_COOKIES,
					      de->domain);
				sleep(MessageSecs);
				if ((!HTList_isEmpty(de->cookie_list)) &&
				    HTConfirm(DELETE_ALL_COOKIES_IN_DOMAIN))
				    goto Delete_all_cookies_in_domain;
				return(HT_NO_DATA);

			    default:
				continue;
			}
			break;
		    }
		}
		break;
	    }
	}
	if (HTList_isEmpty(domain_list)) {
	    /*
	     *	There are no more domains left,
	     *	so delete the domain_list. - FM
	     */
	    HTList_delete(domain_list);
	    domain_list = NULL;
	    HTProgress(ALL_COOKIES_EATEN);
	    sleep(MessageSecs);
	}
	return(HT_NO_DATA);
    }

    /*
     *	If we get to here, it was a LYNXCOOKIE:/ URL
     *	for creating and displaying the Cookie Jar Page,
     *	or we didn't find the domain or cookie in a
     *	deletion request.  Set up an HTML stream and
     *	return an updated Cookie Jar Page. - FM
     */
    target = HTStreamStack(format_in,
			   format_out,
			   sink, anAnchor);
    if (!target || target == NULL) {
	sprintf(buf, CANNOT_CONVERT_I_TO_O,
		HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	return(HT_NOT_LOADED);
    }

    /*
     *	Load HTML strings into buf and pass buf
     *	to the target for parsing and rendering. - FM
     */
    sprintf(buf, "<HEAD>\n<TITLE>%s</title>\n</HEAD>\n<BODY>\n",
		 COOKIE_JAR_TITLE);
    (*target->isa->put_block)(target, buf, strlen(buf));

    sprintf(buf, "<H1>%s</H1>\n", REACHED_COOKIE_JAR_PAGE);
    (*target->isa->put_block)(target, buf, strlen(buf));
    sprintf(buf, "<H2>%s Version %s</H2>\n", LYNX_NAME, LYNX_VERSION);
    (*target->isa->put_block)(target, buf, strlen(buf));

    sprintf(buf, "<NOTE>%s\n", ACTIVATE_TO_GOBBLE);
    (*target->isa->put_block)(target, buf, strlen(buf));
    sprintf(buf, "%s</NOTE>\n", OR_CHANGE_ALLOW);
    (*target->isa->put_block)(target, buf, strlen(buf));

    sprintf(buf, "<DL COMPACT>\n");
    (*target->isa->put_block)(target, buf, strlen(buf));
    for (dl = domain_list; dl != NULL; dl = dl->next) {
	de = dl->object;
	if (de == NULL)
	    /*
	     *	First object always is NULL. - FM
	     */
	    continue;

	/*
	 *  Show the domain link and 'allow' setting. - FM
	 */
	sprintf(buf, "<DT><A HREF=\"LYNXCOOKIE://%s/\">Domain=%s</A>\n",
		      de->domain, de->domain);
	(*target->isa->put_block)(target, buf, strlen(buf));
	switch (de->bv) {
	    case (ACCEPT_ALWAYS):
		sprintf(buf, COOKIES_ALWAYS_ALLOWED);
		break;
	    case (REJECT_ALWAYS):
		sprintf(buf, COOKIES_NEVER_ALLOWED);
		break;
	    case (QUERY_USER):
		sprintf(buf, COOKIES_ALLOWED_VIA_PROMPT);
	    break;
	}
	(*target->isa->put_block)(target, buf, strlen(buf));

	/*
	 *  Show the domain's cookies. - FM
	 */
	for (cl = de->cookie_list; cl != NULL; cl = cl->next) {
	    if ((co = (cookie *)cl->object) == NULL)
		/*
		 *  First object is always NULL. - FM
		 */
		continue;

	    /*
	     *	Show the name=value pair. - FM
	     */
	    if (co->name) {
		StrAllocCopy(name, co->name);
		LYEntify(&name, TRUE);
	    } else {
		StrAllocCopy(name, NO_NAME);
	    }
	    if (co->value) {
		StrAllocCopy(value, co->value);
		LYEntify(&value, TRUE);
	    } else {
		StrAllocCopy(value, NO_VALUE);
	    }
	    sprintf(buf, "<DD><A HREF=\"LYNXCOOKIE://%s/%s\">%s=%s</A>\n",
			 de->domain, co->lynxID, name, value);
	    FREE(name);
	    FREE(value);
	    (*target->isa->put_block)(target, buf, strlen(buf));

	    /*
	     *	Show the path, port, secure and discard setting. - FM
	     */
	    if (co->path) {
		StrAllocCopy(path, co->path);
		LYEntify(&path, TRUE);
	    } else {
		StrAllocCopy(path, "/");
	    }
	    sprintf(buf, "<DD>Path=%s\n<DD>Port: %d Secure: %s Discard: %s\n",
			 path, co->port,
			 ((co->flags & COOKIE_FLAG_SECURE) ? "YES" : "NO"),
			 ((co->flags & COOKIE_FLAG_DISCARD) ? "YES" : "NO"));
	    FREE(path);
	    (*target->isa->put_block)(target, buf, strlen(buf));

	    /*
	     *	Show the list of acceptable ports, if present. - FM
	     */
	    if (co->PortList) {
		sprintf(buf, "<DD>PortList=\"%s\"\n", co->PortList);
		(*target->isa->put_block)(target, buf, strlen(buf));
	    }

	    /*
	     *	Show the commentURL, if we have one. - FM
	     */
	    if (co->commentURL) {
		StrAllocCopy(Address, co->commentURL);
		LYEntify(&Address, FALSE);
		StrAllocCopy(Title, co->commentURL);
		LYEntify(&Title, TRUE);
		sprintf(buf,
			"<DD>CommentURL: <A href=\"%s\">%s</A>\n",
			Address,
			Title);
		FREE(Address);
		FREE(Title);
		(*target->isa->put_block)(target, buf, strlen(buf));
	    }

	    /*
	     *	Show the comment, if we have one. - FM
	     */
	    if (co->comment) {
		StrAllocCopy(comment, co->comment);
		LYEntify(&comment, TRUE);
		sprintf(buf, "<DD>Comment: %s\n", comment);
		FREE(comment);
		(*target->isa->put_block)(target, buf, strlen(buf));
	    }

	    /*
	     *	Show the Maximum Gobble Date. - FM
	     */
	    sprintf(buf, "<DD><EM>Maximum Gobble Date:</EM> %s%s",
			 ((co->expires > 0 &&
			   !(co->flags & COOKIE_FLAG_DISCARD))
					    ?
			ctime(&co->expires) : END_OF_SESSION),
			 ((co->expires > 0 &&
			   !(co->flags & COOKIE_FLAG_DISCARD))
					    ?
					 "" : "\n"));
	    (*target->isa->put_block)(target, buf, strlen(buf));
	}
	sprintf(buf, "</DT>\n");
	(*target->isa->put_block)(target, buf, strlen(buf));
    }
    sprintf(buf, "</DL>\n</BODY>\n");
    (*target->isa->put_block)(target, buf, strlen(buf));

    /*
     *	Free the target to complete loading of the
     *	Cookie Jar Page, and report a successful load. - FM
     */
    (*target->isa->_free)(target);
    return(HT_LOADED);
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYCOOKIE_C_GLOBALDEF_1_INIT { "LYNXCOOKIE",LYHandleCookies,0}
GLOBALDEF (HTProtocol,LYLynxCookies,_LYCOOKIE_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxCookies = {"LYNXCOOKIE",LYHandleCookies,0};
#endif /* GLOBALDEF_IS_MACRO */
