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
**	Partially checked against:
**   http://www.ietf.org/internet-drafts/draft-ietf-http-state-man-mec-10.txt
**		- kw					1998-12-11
**
**  TO DO: (roughly in order of decreasing priority)
      * Persistent cookies are still experimental.  Presently cookies
	lose many pieces of information that distinguish
	version 1 from version 0 cookies.  There is no easy way around
	that with the current cookie file format.  Ports are currently
	not stored persistently at all which is clearly wrong.
      * We currently don't do anything special for unverifiable
	transactions to third-party hosts.
      * We currently don't use effective host names or check for
	Domain=.local.
      * Hex escaping isn't considered at all.  Any semi-colons, commas,
	or spaces actually in cookie names or values (i.e., not serving
	as punctuation for the overall Set-Cookie value) should be hex
	escaped if not quoted, but presumably the server is expecting
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
	connections as secure.  This may need to be expanded for other
	secure communication protocols that become standardized.
*/

#include <HTUtils.h>
#include <HTAccess.h>
#include <HTParse.h>
#include <HTAlert.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYCharUtils.h>
#include <LYClean.h>
#include <LYGlobalDefs.h>
#include <LYEdit.h>
#include <LYStrings.h>
#include <GridText.h>
#include <LYCookie.h>

#include <LYLeaks.h>

#define max_cookies_domain 50
#define max_cookies_global 500
#define max_cookies_buffer 4096

/* default for new domains, one of the invcheck_behaviour_t values: */
#define DEFAULT_INVCHECK_BV INVCHECK_QUERY

/*
**  The first level of the cookie list is a list indexed by the domain
**  string; cookies with the same domain will be placed in the same
**  list.  Thus, finding the cookies that apply to a given URL is a
**  two-level scan; first we check each domain to see if it applies,
**  and if so, then we check the paths of all the cookies on that
**  list.  We keep a running total of cookies as we add or delete
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
#define COOKIE_FLAG_FROM_FILE 32  /* If set, this cookie was persistent */

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

    temp = typecallocn(char, (end - start) + 1);
    if (temp == NULL)
	outofmem(__FILE__, "MemAllocCopy");
    LYstrncpy(temp, start, (end - start));
    HTSACopy(dest, temp);
    FREE(temp);
}

PRIVATE cookie * newCookie NOARGS
{
    cookie *p = typecalloc(cookie);

    if (p == NULL)
	outofmem(__FILE__, "newCookie");
    HTSprintf0(&(p->lynxID), "%p", p);
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

#ifdef LY_FIND_LEAKS
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
	    FREE(dl->object);
	}
	dl = dl->next;
    }
    cookie_list = NULL;
    HTList_delete(domain_list);
    domain_list = NULL;
}
#endif /* LY_FIND_LEAKS */

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
     *	FQDNs.  Do numeric addresses require special handling?
     */
    if (*B != '.' && !strcasecomp(A, B))
	return YES;

    /*
     *	The following will pass a "dotted tail" match to "a.b.c.e"
     *	as described in Section 2 of draft-ietf-http-state-man-mec-10.txt.
     */
    if (*B == '.' && B[1] != '\0' && B[1] != '.' && *A != '.') {
	int diff = (strlen(A) - strlen(B));
	if (diff > 0) {
	    if (!strcasecomp((A + diff), B))
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

    if (!(number && isdigit(UCH(*number))))
	return(FALSE);

    while (*number != '\0') {
	if (atoi(number) == port) {
	    return(TRUE);
	}
	while (isdigit(UCH(*number))) {
	    number++;
	}
	while (*number != '\0' && !isdigit(UCH(*number))) {
	    number++;
	}
    }

    return(FALSE);
}

/*
 * Returns the length of the given path ignoring trailing slashes.
 */
PRIVATE int ignore_trailing_slash ARGS1(CONST char *, a)
{
    int len = strlen(a);
    while (len > 1 && a[len-1] == '/')
	--len;
    return len;
}

/*
 * Check if the path 'a' is a prefix of path 'b', ignoring trailing slashes
 * in either, since they denote an empty component.
 */
PRIVATE BOOL is_prefix ARGS2(CONST char *, a, CONST char *, b)
{
    int len_a = ignore_trailing_slash(a);
    int len_b = ignore_trailing_slash(b);

    if (len_a > len_b) {
	return FALSE;
    } else {
	if (strncmp(a, b, len_a) != 0) {
	    return FALSE;
	}
	if (len_a < len_b && (len_a > 1 || a[0] != '/')) {
	    if (b[len_a] != '\0'
	     && b[len_a] != '/') {
		return FALSE;
	     }
	}
    }
    return TRUE;
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
    int invprompt_reasons = 0;	/* what is wrong with this cookie - kw */
#define FAILS_COND1 0x01
#define FAILS_COND4 0x02

    if (co == NULL)
	return;

    /*
     *	Ensure that the domain list exists.
     */
    if (domain_list == NULL) {
#ifdef LY_FIND_LEAKS
	atexit(LYCookieJar_free);
#endif
	domain_list = HTList_new();
	total_cookies = 0;
    }

    /*
     *	Look through domain_list to see if the cookie's domain
     *	is already listed.
     */
    cookie_list = NULL;
    for (hl = domain_list; hl != NULL; hl = hl->next) {
	de = (domain_entry *)hl->object;
	if ((de != NULL && de->domain != NULL) &&
	    !strcasecomp(co->domain, de->domain)) {
		cookie_list = de->cookie_list;
		break;
	}
    }

    if(hl == NULL) {
	de = NULL;
	cookie_list = NULL;
    }

    /*
     * Apply sanity checks.
     *
     * Section 4.3.2, condition 1:  The value for the Path attribute is
     * not a prefix of the request-URI.
     *
     * If cookie checking for this domain is set to INVCHECK_LOOSE,
     * then we want to bypass this check.  The user should be queried
     * if set to INVCHECK_QUERY.
     */
    if (!is_prefix(co->path, path)) {
	invcheck_behaviour_t invcheck_bv = (de ? de->invcheck_bv
	    				       : DEFAULT_INVCHECK_BV);
	switch (invcheck_bv) {
	case INVCHECK_LOOSE:
	    break;		/* continue as if nothing were wrong */

	case INVCHECK_QUERY:
	    invprompt_reasons |= FAILS_COND1;
	    break;		/* will prompt later if we get that far */

	case INVCHECK_STRICT:
	    CTRACE((tfp, "store_cookie: Rejecting because '%s' is not a prefix of '%s'.\n",
		   co->path, path));
	    freeCookie(co);
	    return;
	}
    }
    /*
     * The next 4 conditions do NOT apply if the domain is still
     * the default of request-host. (domains - case insensitive).
     */
    if (strcasecomp(co->domain, hostname) != 0) {
	/*
	 *  The hostname does not contain a dot.
	 */
	if (strchr(hostname, '.') == NULL) {
	    CTRACE((tfp, "store_cookie: Rejecting because '%s' has no dot.\n",
		    hostname));
	    freeCookie(co);
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
	    CTRACE((tfp, "store_cookie: Rejecting domain '%s'.\n",
		    co->domain));
	    freeCookie(co);
	    return;
	}
	ptr = strchr((co->domain + 1), '.');
	if (ptr == NULL || ptr[1] == '\0') {
	    CTRACE((tfp, "store_cookie: Rejecting domain '%s'.\n",
		    co->domain));
	    freeCookie(co);
	    return;
	}

	/*
	 *  Section 4.3.2, condition 3: The value for the request-host does
	 *  not domain-match the Domain attribute.
	 */
	if (!host_matches(hostname, co->domain)) {
	    CTRACE((tfp, "store_cookie: Rejecting domain '%s' for host '%s'.\n",
		    co->domain, hostname));
	    freeCookie(co);
	    return;
	}

	/*
	 *  Section 4.3.2, condition 4: The request-host is an HDN (not IP
	 *  address) and has the form HD, where D is the value of the Domain
	 *  attribute, and H is a string that contains one or more dots.
	 *
	 *  If cookie checking for this domain is set to INVCHECK_LOOSE,
	 *  then we want to bypass this check.  The user should be queried
	 *  if set to INVCHECK_QUERY.
	 */
	ptr = ((hostname + strlen(hostname)) - strlen(co->domain));
	if (strchr(hostname, '.') < ptr) {
	    invcheck_behaviour_t invcheck_bv = (de ? de->invcheck_bv
						   : DEFAULT_INVCHECK_BV);
	    switch (invcheck_bv) {
	    case INVCHECK_LOOSE:
		break;		/* continue as if nothing were wrong */

	    case INVCHECK_QUERY:
		invprompt_reasons |= FAILS_COND4;
		break;		/* will prompt later if we get that far */

	    case INVCHECK_STRICT:
		CTRACE((tfp, "store_cookie: Rejecting because '%s' is not a prefix of '%s'.\n",
		       co->path, path));
		freeCookie(co);
		return;
	    }
	}
    }

    /*
     *  If we found reasons for issuing an invalid cookie confirmation
     *  prompt, do that now.  Rejection by the user here is the last
     *  chance to completely ignore this cookie; after it passes this
     *  hurdle, it may at least supersede a previous cookie (even if
     *  it finally gets rejected). - kw
     */
    if (invprompt_reasons) {
	char *msg = 0;
	if (invprompt_reasons & FAILS_COND4) {
	    HTSprintf0(&msg,
		       INVALID_COOKIE_DOMAIN_CONFIRMATION,
		       co->domain,
		       hostname);
	    if (!HTConfirmDefault(msg, NO)) {
		CTRACE((tfp, "store_cookie: Rejecting domain '%s' for host '%s'.\n",
					co->domain,
					hostname));
		freeCookie(co);
		FREE(msg);
		return;
	    }
	}
	if (invprompt_reasons & FAILS_COND1) {
	    HTSprintf0(&msg,
		       INVALID_COOKIE_PATH_CONFIRMATION,
		       co->path, path);
	    if (!HTConfirmDefault(msg, NO)) {
		CTRACE((tfp, "store_cookie: Rejecting because '%s' is not a prefix of '%s'.\n",
		       co->path, path));
		freeCookie(co);
		FREE(msg);
		return;
	    }
	}
	FREE(msg);
    }

    if (hl == NULL) {
	/*
	 *	Domain not found; add a new entry for this domain.
	 */
	de = typecalloc(domain_entry);
	if (de == NULL)
	    outofmem(__FILE__, "store_cookie");
#if 0	/* was: ifdef EXP_PERSISTENT_COOKIES */
	/*
	 * The default behavior for this new domain could be set
	 * differently if the cookie comes from a file, as the
	 * code had it originally, but there doesn't seem to be
	 * a good reason for it any more; setting more permissive
	 * behavior for individual domains is now possible via
	 * configuration options. - kw
	 */
	if (persistent_cookies
	 && (co->flags & COOKIE_FLAG_FROM_FILE))
	    de->bv = ACCEPT_ALWAYS; /* ?? */
	else
#endif
	    de->bv = QUERY_USER;
	de->invcheck_bv = DEFAULT_INVCHECK_BV; /* should this go here? */
	cookie_list = de->cookie_list = HTList_new();
	StrAllocCopy(de->domain, co->domain);
	HTList_appendObject(domain_list, de);
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
	    c2->expires <= now) {
	    HTList_removeObject(cookie_list, c2);
	    freeCookie(c2);
	    c2 = NULL;
	    total_cookies--;

	/*
	 *  Check if this cookie matches the one we're inserting.
	 */
	} else if ((c2) &&
		   !strcasecomp(co->domain, c2->domain) &&
		   !strcmp(co->path, c2->path) &&
		   !strcmp(co->name, c2->name)) {
	    HTList_removeObject(cookie_list, c2);
	    freeCookie(c2);
	    c2 = NULL;
	    total_cookies--;
	    Replacement = TRUE;

	} else if ((c2) && (c2->pathlen) >= (co->pathlen)) {
	    /*
	     *  This comparison determines the (tentative) position
	     *  of the new cookie in the list such that it comes
	     *  before existing cookies with a less specific path,
	     *  but after existing cookies of equal (or greater)
	     *  path length.  Thus it should normally preserve
	     *  the order of new cookies with the same path as
	     *  they are received, although this is not required.
	     *  From RFC 2109 4.3.4:

   If multiple cookies satisfy the criteria above, they are ordered in
   the Cookie header such that those with more specific Path
   attributes precede those with less specific.  Ordering with respect
   to other attributes (e.g., Domain) is unspecified.

	     */
	    pos++;
	}
	hl = next;
    }

    /*
     *	Don't bother to add the cookie if it's already expired.
     */
    if ((co->flags & COOKIE_FLAG_EXPIRES_SET) && co->expires <= now) {
	freeCookie(co);
	co = NULL;

    /*
     *	Don't add the cookie if we're over the domain's limit. - FM
     */
    } else if (HTList_count(cookie_list) > max_cookies_domain) {
	CTRACE((tfp, "store_cookie: Domain's cookie limit exceeded!  Rejecting cookie.\n"));
	freeCookie(co);
	co = NULL;

    /*
     *	Don't add the cookie if we're over the total cookie limit. - FM
     */
    } else if (total_cookies > max_cookies_global) {
	CTRACE((tfp, "store_cookie: Total cookie limit exceeded!  Rejecting cookie.\n"));
	freeCookie(co);
	co = NULL;

    /*
     * Don't add the cookie if the value is NULL. - BJP
     */
	/*
	 * Presence of value is now needed (indicated normally by '='),
	 * but it can now be an empty string.
	 * - kw 1999-06-24
	 */
    } else if (co->value == NULL) { /* should not happen - kw */
	CTRACE((tfp, "store_cookie: Value is NULL! Not storing cookie.\n"));
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
     *
     *  Cookies read from file are accepted without confirmation
     *  prompting.  (Prompting may actually not be possible if
     *  LYLoadCookies is called before curses is setup.)  Maybe
     *  this should instead depend on LYSetCookies and/or
     *  LYCookieAcceptDomains and/or LYCookieRejectDomains and/or
     *  LYAcceptAllCookies and/or some other settings. -kw
     */
    } else if ((co->flags & COOKIE_FLAG_FROM_FILE)
	       || HTConfirmCookie(de, hostname, co->name, co->value)) {
	/*
	 * Insert the new cookie so that more specific paths (longer
	 * pathlen) come first in the list. - kw
	 */
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
	char *,		hostname,
	char *,		path,
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

       if ((co) && /* speed-up host_matches() and limit trace output */
	   (LYstrstr(hostname, co->domain) != NULL))
       {
	    CTRACE((tfp, "Checking cookie %p %s=%s\n",
			hl,
			(co->name ? co->name : "(no name)"),
			(co->value ? co->value : "(no value)")));
	    CTRACE((tfp, "\t%s %s %d %s %s %d%s\n",
			hostname,
			(co->domain ? co->domain : "(no domain)"),
			host_matches(hostname, co->domain),
			path, co->path,
			(co->pathlen > 0)
			    ? !is_prefix(co->path, path)
			    : 0,
			(co->flags & COOKIE_FLAG_SECURE)
			    ? " secure"
			    : ""));
	}
	/*
	 *  Check if this cookie has expired, and if so, delete it.
	 */
	if (((co) && (co->flags & COOKIE_FLAG_EXPIRES_SET)) &&
	    co->expires <= now) {
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
	    (co->pathlen == 0 || is_prefix(co->path, path))) {
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
		    HTSprintf0(&header, "$Version=\"%d\"; ", co->version);
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

		/*
		 * Section 2.2 of RFC1945 says:
		 *
		 *  HTTP/1.0 headers may be folded onto multiple lines
		 *  if each continuation line begins with a space or
		 *  horizontal tab.  All linear whitespace, including
		 *  folding, has the same semantics as SP.
		 *  [...]
		 *  However, folding of header lines is not expected by
		 *  some applications, and should not be generated by
		 *  HTTP/1.0 applications.
		 *
		 * This code was causing problems.  Let's not use it. -BJP
		 */

		/* if (len > 800) { */
		/*    StrAllocCat(header, crlftab); */
		/*    len = 0; */
		/* } */

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
		if (co->PortList && isdigit(UCH(*co->PortList))) {
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
    BOOLEAN invalidport = FALSE;

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
    if (SetCookie && *p) {
	CTRACE((tfp, "LYProcessSetCookies: Using Set-Cookie2 header.\n"));
    }
    while (NumCookies <= max_cookies_domain && *p) {
	attr_start = attr_end = value_start = value_end = NULL;
	p = LYSkipCBlanks(p);
	/*
	 *  Get the attribute name.
	 */
	attr_start = p;
	while (*p != '\0' && !isspace(UCH(*p)) &&
	       *p != '=' && *p != ';' && *p != ',')
	    p++;
	attr_end = p;
	p = LYSkipCBlanks(p);

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
	    p = LYSkipCBlanks(p);
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
		if (isdigit(UCH(*p))) {
		    /*
		     *	No alphabetic day field. - FM
		     */
		    spaces--;
		} else {
		    /*
		     *	Skip the alphabetic day field. - FM
		     */
		    while (*p != '\0' && isalpha(UCH(*p))) {
			p++;
		    }
		    while (*p == ',' || isspace(UCH(*p))) {
			p++;
		    }
		    spaces--;
		}
		while (*p != '\0' && *p != ';' && *p != ',' && spaces) {
		    p++;
		    if (isspace(UCH(*p))) {
			while (isspace(UCH(*(p + 1))))
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
		       isdigit(UCH(*p))) {
		/*
		 *  The value starts as an unquoted number.
		 */
		CONST char *cp, *cp1;
		value_start = p;
		while (1) {
		    while (isdigit(UCH(*p)))
			p++;
		    value_end = p;
		    p = LYSkipCBlanks(p);
		    if (*p == '\0' || *p == ';')
			break;
		    if (*p == ',') {
			cp = LYSkipCBlanks(p + 1);
			if (*cp != '\0' && isdigit(UCH(*cp))) {
			    cp1 = cp;
			    while (isdigit(UCH(*cp1)))
				cp1++;
			    cp1 = LYSkipCBlanks(cp1);
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
			isspace(UCH(*(value_end - 1)))) {
			value_end--;
			while ((value_end > (value_start + 1)) &&
			       isspace(UCH(*value_end)) &&
			       isspace(UCH(*(value_end - 1)))) {
			    value_end--;
			}
		    }
		    break;
		}
	    } else if (*p == '"') {
		BOOLEAN escaped = FALSE;
		/*
		 *  It looks like quoted string.
		 */
		p++;
		value_start = p;
		while (*p != '\0' && (*p != '"' || escaped)) {
		    escaped = (BOOL) (!escaped && *p == '\\');
		    p++;
		}
		if (p != value_start && *p == '"' && !escaped) {
		    value_end = p;
		    p++;
		    Quoted = TRUE;
		} else {
		    value_start--;
		    value_end = p;
		    if (*p)
			p++;
		    Quoted = FALSE;
		}
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
		    isspace(UCH(*(value_end - 1)))) {
		    value_end--;
		    while ((value_end > (value_start + 1)) &&
			   isspace(UCH(*value_end)) &&
			   isspace(UCH(*(value_end - 1)))) {
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

	    if (value_start && value_end >= value_start) {
		/*
		 * Presence of value is now needed (indicated normally by '=')
		 * to start a cookie, but it can now be an empty string.
		 * - kw 1999-06-24
		 */
		int value_len = (value_end - value_start);

		if (value_len > max_cookies_buffer) {
		    value_len = max_cookies_buffer;
		}
		value = typecallocn(char, value_len + 1);
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
			CTRACE((tfp, "LYProcessSetCookies: Rejecting commentURL value '%s'\n",
				    cur_cookie->commentURL));
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
		     *	(domains - case insensitive).
		     */
		    if (value[0] != '.' && value[0] != '\0' &&
			value[1] != '\0' && strcasecomp(value, hostname)) {
			char *ptr = strchr(value, '.');
			if (ptr != NULL && ptr[1] != '\0') {
			    ptr = value;
			    while (*ptr == '.' ||
				   isdigit(UCH(*ptr)))
				ptr++;
			    if (*ptr != '\0') {
				CTRACE((tfp,
	       "LYProcessSetCookies: Adding lead dot for domain value '%s'\n",
					    value));
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
			   (isdigit(UCH(*cp)) ||
			    *cp == ',' || *cp == ' ')) {
			cp++;
		    }
		    if (*cp == '\0' && !port_matches(port, value)) {
			invalidport = TRUE;
			known_attr = YES;
		    } else if (*cp == '\0') {
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
			HTSprintf0(&(cur_cookie->PortList), "%d", port);
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
			CTRACE((tfp, "LYSetCookie: expires %ld, %s",
				    (long) cur_cookie->expires,
				    ctime(&cur_cookie->expires)));
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
		    if (value) {
			cur_cookie->flags |= COOKIE_FLAG_EXPIRES_SET;
			cur_cookie->expires = LYmktime(value, FALSE);
			if (cur_cookie->expires > 0) {
			    CTRACE((tfp, "LYSetCookie: expires %ld, %s",
					(long) cur_cookie->expires,
					ctime(&cur_cookie->expires)));
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
	    /* if (!known_attr && value_end > value_start) */

	    /* Is there any reason we don't want to accept cookies with
	     * no value?  This seems to be needed for sites that reset a
	     * cookie by nulling out the value.  If this causes problems,
	     * we can go back to the original behavior above.  - BJP
	     *
	     * Presence of value is now needed (indicated normally by '='),
	     * but it can now be an empty string. - kw 1999-06-24
	     */
	    if (!known_attr && value && value_end >= value_start) {
		/*
		 *  If we've started a cookie, and it's not too big,
		 *  save it in the CombinedCookies list. - FM
		 */
		if (length <= max_cookies_buffer && cur_cookie != NULL &&
		    !invalidport) {
		    /*
		     *	Assume version 1 if not set to that or higher. - FM
		     */
		    if (cur_cookie->version < 1) {
			cur_cookie->version = 1;
		    }
		    HTList_appendObject(CombinedCookies, cur_cookie);
		} else if (cur_cookie != NULL) {
		    CTRACE((tfp,
			"LYProcessSetCookies: Rejecting Set-Cookie2: %s=%s\n",
				(cur_cookie->name ?
				 cur_cookie->name : "[no name]"),
				(cur_cookie->value ?
				 cur_cookie->value : "[no value]")));
		    CTRACE((tfp,
			   invalidport ?
			   "                     due to excessive length!\n"
			 : "                     due to invalid port!\n"));
		    if (invalidport) {
			NumCookies --;
		    }
		    freeCookie(cur_cookie);
		    cur_cookie = NULL;
		}
		/*
		 *  Start a new cookie. - FM
		 */
		cur_cookie = newCookie();
		invalidport = FALSE;
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
    if (NumCookies <= max_cookies_domain
     && length <= max_cookies_buffer
     && cur_cookie != NULL && !invalidport) {
	if (cur_cookie->version < 1) {
	    cur_cookie->version = 1;
	}
	HTList_appendObject(CombinedCookies, cur_cookie);
    } else if (cur_cookie != NULL && !invalidport) {
	CTRACE((tfp, "LYProcessSetCookies: Rejecting Set-Cookie2: %s=%s\n",
		    (cur_cookie->name ? cur_cookie->name : "[no name]"),
		    (cur_cookie->value ? cur_cookie->value : "[no value]")));
	CTRACE((tfp, "                     due to excessive %s%s%s\n",
		    (length > max_cookies_buffer ? "length" : ""),
		    (length > max_cookies_buffer &&
		     NumCookies > max_cookies_domain
			? " and "
			: ""),
		    (NumCookies > max_cookies_domain ? "number!\n" : "!\n")));
	freeCookie(cur_cookie);
	cur_cookie = NULL;
    } else if (cur_cookie != NULL) {			/* invalidport */
	CTRACE((tfp, "LYProcessSetCookies: Rejecting Set-Cookie2: %s=%s\n",
		    (cur_cookie->name ? cur_cookie->name : "[no name]"),
		    (cur_cookie->value ? cur_cookie->value : "[no value]")));
	CTRACE((tfp, "                     due to invalid port!\n"));
	NumCookies --;
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
    if (SetCookie2 && *p) {
	CTRACE((tfp, "LYProcessSetCookies: Using Set-Cookie header.\n"));
    }
    while (NumCookies <= max_cookies_domain && *p) {
	attr_start = attr_end = value_start = value_end = NULL;
	p = LYSkipCBlanks(p);
	/*
	 *  Get the attribute name.
	 */
	attr_start = p;
	while (*p != '\0' && !isspace(UCH(*p)) &&
	       *p != '=' && *p != ';' && *p != ',')
	    p++;
	attr_end = p;
	p = LYSkipCBlanks(p);

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
	    p = LYSkipCBlanks(p);
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
		if (isdigit(UCH(*p))) {
		    /*
		     *	No alphabetic day field. - FM
		     */
		    spaces--;
		} else {
		    /*
		     *	Skip the alphabetic day field. - FM
		     */
		    while (*p != '\0' && isalpha(UCH(*p))) {
			p++;
		    }
		    while (*p == ',' || isspace(UCH(*p))) {
			p++;
		    }
		    spaces--;
		}
		while (*p != '\0' && *p != ';' && *p != ',' && spaces) {
		    p++;
		    if (isspace(UCH(*p))) {
			while (isspace(UCH(*(p + 1))))
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
		       isdigit(UCH(*p))) {
		/*
		 *  The value starts as an unquoted number.
		 */
		CONST char *cp, *cp1;
		value_start = p;
		while (1) {
		    while (isdigit(UCH(*p)))
			p++;
		    value_end = p;
		    p = LYSkipCBlanks(p);
		    if (*p == '\0' || *p == ';')
			break;
		    if (*p == ',') {
			cp = LYSkipCBlanks(p + 1);
			if (*cp != '\0' && isdigit(UCH(*cp))) {
			    cp1 = cp;
			    while (isdigit(UCH(*cp1)))
				cp1++;
			    cp1 = LYSkipCBlanks(cp1);
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
			isspace(UCH(*(value_end - 1)))) {
			value_end--;
			while ((value_end > (value_start + 1)) &&
			       isspace(UCH(*value_end)) &&
			       isspace(UCH(*(value_end - 1)))) {
			    value_end--;
			}
		    }
		    break;
		}
	    } else if (*p == '"') {
		BOOLEAN escaped = FALSE;
		/*
		 *  It looks like quoted string.
		 */
		p++;
		value_start = p;
		while (*p != '\0' && (*p != '"' || escaped)) {
		    escaped = (BOOL) (!escaped && *p == '\\');
		    p++;
		}
		if (p != value_start && *p == '"' && !escaped) {
		    value_end = p;
		    p++;
		    Quoted = TRUE;
		} else {
		    value_start--;
		    value_end = p;
		    if (*p)
			p++;
		    Quoted = FALSE;
		}
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
		    isspace(UCH(*(value_end - 1)))) {
		    value_end--;
		    while ((value_end > (value_start + 1)) &&
			   isspace(UCH(*value_end)) &&
			   isspace(UCH(*(value_end - 1)))) {
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

	    if (value_start && value_end >= value_start) {
		/*
		 * Presence of value is now needed (indicated normally by '=')
		 * to start a cookie, but it can now be an empty string.
		 * - kw 1999-06-24
		 */
		int value_len = (value_end - value_start);

		if (value_len > max_cookies_buffer) {
		    value_len = max_cookies_buffer;
		}
		value = typecallocn(char, value_len + 1);
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
			CTRACE((tfp, "LYProcessSetCookies: Rejecting commentURL value '%s'\n",
				    cur_cookie->commentURL));
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
		     *	(domains - case insensitive).
		     */
		    if (value[0] != '.' && value[0] != '\0' &&
			value[1] != '\0' && strcasecomp(value, hostname)) {
			char *ptr = strchr(value, '.');
			if (ptr != NULL && ptr[1] != '\0') {
			    ptr = value;
			    while (*ptr == '.' ||
				   isdigit(UCH(*ptr)))
				ptr++;
			    if (*ptr != '\0') {
				CTRACE((tfp,
	       "LYProcessSetCookies: Adding lead dot for domain value '%s'\n",
					    value));
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
			   (isdigit(UCH(*cp)) ||
			    *cp == ',' || *cp == ' ')) {
			cp++;
		    }
		    if (*cp == '\0' && port_matches(port, value)) {
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
			HTSprintf0(&(cur_cookie->PortList), "%d", port);
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
	    /* if (!known_attr && value_end > value_start) */

	    /* Is there any reason we don't want to accept cookies with
	     * no value?  This seems to be needed for sites that reset a
	     * cookie by nulling out the value.  If this causes problems,
	     * we can go back to the original behavior above.  - BJP
	     *
	     * Presence of value is now needed (indicated normally by '='),
	     * but it can now be an empty string. - kw 1999-06-24
	     */
	    if (!known_attr && value && value_end >= value_start) {
		/*
		 *  If we've started a cookie, and it's not too big,
		 *  save it in the CombinedCookies list. - FM
		 */
		if (length <= max_cookies_buffer && cur_cookie != NULL) {
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
		    CTRACE((tfp, "LYProcessSetCookies: Rejecting Set-Cookie: %s=%s\n",
				(cur_cookie->name ?
				 cur_cookie->name : "[no name]"),
				(cur_cookie->value ?
				 cur_cookie->value : "[no value]")));
		    CTRACE((tfp, "                     due to excessive length!\n"));
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
    if (NumCookies <= max_cookies_domain && length <= max_cookies_buffer && cur_cookie != NULL) {
	if (SetCookie2 != NULL) {
	    if (cur_cookie->version < 1) {
		cur_cookie->version = 1;
	    }
	    cur_cookie->quoted = TRUE;
	}
	HTList_appendObject(CombinedCookies, cur_cookie);
    } else if (cur_cookie != NULL) {
	CTRACE((tfp, "LYProcessSetCookies: Rejecting Set-Cookie: %s=%s\n",
		    (cur_cookie->name ? cur_cookie->name : "[no name]"),
		    (cur_cookie->value ? cur_cookie->value : "[no value]")));
	CTRACE((tfp, "                     due to excessive %s%s%s\n",
		    (length > max_cookies_buffer ? "length" : ""),
		    (length > max_cookies_buffer && NumCookies > max_cookies_domain ? " and " : ""),
		    (NumCookies > max_cookies_domain ? "number!\n" : "!\n")));
	freeCookie(cur_cookie);
	cur_cookie = NULL;
    }

    /*
     *	OK, now we can actually store any cookies
     *	in the CombinedCookies list. - FM
     */
    cl = CombinedCookies;
    while (NULL != (co = (cookie *)HTList_nextObject(cl))) {
	CTRACE((tfp, "LYProcessSetCookie: attr=value pair: '%s=%s'\n",
			    (co->name ? co->name : "[no name]"),
			    (co->value ? co->value : "[no value]")));
	if (co->expires > 0) {
		CTRACE((tfp, "                    expires: %ld, %s\n",
			    (long)co->expires,
			    ctime(&co->expires)));
	}
	if (!strncasecomp(address, "https:", 6) &&
	    LYForceSSLCookiesSecure == TRUE &&
	    !(co->flags & COOKIE_FLAG_SECURE)) {
	    co->flags |= COOKIE_FLAG_SECURE;
	    CTRACE((tfp, "                    Forced the 'secure' flag on.\n"));
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
    path = HTParse(address, "", PARSE_PATH|PARSE_PUNCTUATION);
    if (!(SetCookie && *SetCookie) &&
	!(SetCookie2 && *SetCookie2)) {
	/*
	 *  Yuk, something must have gone wrong in
	 *  HTMIME.c or HTTP.c because both SetCookie
	 *  and SetCookie2 are NULL or zero-length. - FM
	 */
	BadHeaders = TRUE;
    }
    CTRACE((tfp, "LYSetCookie called with host '%s', path '%s',\n",
		(hostname ? hostname : ""),
		(path ? path : "")));
    if (SetCookie) {
	CTRACE((tfp, "    and Set-Cookie: '%s'\n",
			 (SetCookie ? SetCookie : "")));
    }
    if (SetCookie2) {
	CTRACE((tfp, "    and Set-Cookie2: '%s'\n",
			 (SetCookie2 ? SetCookie2 : "")));
    }
    if (LYSetCookies == FALSE || BadHeaders == TRUE) {
	CTRACE((tfp, "    Ignoring this Set-Cookie/Set-Cookie2 request.\n"));
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
	char *,		hostname,
	char *,		path,
	int,		port,
	BOOL,		secure)
{
    char *header = NULL;
    HTList *hl = domain_list, *next = NULL;
    domain_entry *de;

    CTRACE((tfp, "LYCookie: Searching for '%s:%d', '%s'.\n",
		NONNULL(hostname),
		port,
		NONNULL(path)));

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
	    } else if (de->bv == QUERY_USER && de->invcheck_bv == DEFAULT_INVCHECK_BV) {
		/*
		 *  No cookies in this domain, and no default
		 *  accept/reject choice was set by the user,
		 *  so delete the domain. - FM
		 */
		FREE(de->domain);
		HTList_delete(de->cookie_list);
		de->cookie_list = NULL;
		HTList_removeObject(domain_list, de);
		FREE(de);
	    }
	}
	hl = next;
    }
    if (header)
	return(header);

    return(NULL);
}

#ifdef EXP_PERSISTENT_COOKIES
PRIVATE int number_of_file_cookies = 0;

/* rjp - experiment cookie loading */
PUBLIC void LYLoadCookies ARGS1 (
	char *,		cookie_file)
{
    FILE *cookie_handle;
    char *buf = NULL;
    static char domain[256], path[LY_MAXPATH], name[256], value[4100];
    static char what[8], secure[8], expires_a[16];
    static struct {
	char *s;
	size_t n;
    } tok_values[] = {
	{ domain,	sizeof(domain) },
	{ what,		sizeof(what) },
	{ path,		sizeof(path) },
	{ secure,	sizeof(secure) },
	{ expires_a,	sizeof(expires_a) },
	{ name,		sizeof(name) },
	{ value,	sizeof(value) },
	{ NULL, 0 }
	};
    time_t expires;

    cookie_handle = fopen(cookie_file, TXT_R);
    if (!cookie_handle)
	return;

    CTRACE((tfp, "LYLoadCookies: reading cookies from %s\n", cookie_file));

    number_of_file_cookies = 0;
    while (LYSafeGets(&buf, cookie_handle) != 0) {
	cookie *moo;
	unsigned i = 0;
	int tok_loop;
	char *tok_out, *tok_ptr;

	if ((buf[0] == '\0' || buf[0] == '\n' || buf[0] == '#')) {
	    continue;
	}

	number_of_file_cookies ++;

	/*
	 * Strip out the newline that fgets() puts at the end of a
	 * cookie.
	 */

	while(buf[i] != '\n' && buf[i] != 0) {
	    i++;
	}
	if (buf[i] == '\n') {
	    buf[i++] = '\t';	/* add sep after line if enough space - kw */
	    buf[i] = '\0';
	}

	/*
	 * Tokenise the cookie line into its component parts -
	 * this only works for Netscape style cookie files at the
	 * moment.  It may be worth investigating an alternative
	 * format for Lynx because the Netscape format isn't all
	 * that useful, or future-proof. - RP
	 *
	 * 'fixed' by using strsep instead of strtok.  No idea
	 * what kind of platform problems this might introduce. - RP
	 */
	/*
	 * This fails when the path is blank
	 *
	 * sscanf(buf, "%s\t%s\t%s\t%s\t%d\t%s\t%[ -~]",
	 *  domain, what, path, secure, &expires, name, value);
	 */
	CTRACE((tfp, "LYLoadCookies: tokenising %s\n", buf));
	tok_ptr = buf;
	tok_out = LYstrsep(&tok_ptr, "\t");
	for (tok_loop = 0; tok_out && tok_values[tok_loop].s; tok_loop++) {
	    CTRACE((tfp, "\t%d:%p:%p:[%s]\n",
		tok_loop, tok_values[tok_loop].s, tok_out, tok_out));
	    LYstrncpy(tok_values[tok_loop].s, tok_out, tok_values[tok_loop].n);
	    /*
	     * It looks like strtok ignores a leading delimiter,
	     * which makes things a bit more interesting.  Something
	     * like "FALSE\t\tFALSE\t" translates to FALSE,FALSE
	     * instead of FALSE,,FALSE. - RP
	     */
	    tok_out = LYstrsep(&tok_ptr, "\t");
	}

	if (tok_values[tok_loop].s) {
	    /* tok_out in above loop must have been NULL prematurely - kw */
	    CTRACE((tfp, "*** wrong format: not enough tokens, ignoring line!\n"));
	    continue;
	}

	expires = atol(expires_a);
	CTRACE((tfp, "expires:\t%s\n", ctime(&expires)));
/* 	CTRACE((tfp, "%s\t%s\t%s\t%s\t%ld\t%s\t%s\tREADCOOKIE\n", */
/* 	    domain, what, path, secure, (long) expires, name, value)); */
	moo = newCookie();
	StrAllocCopy(moo->domain, domain);
	StrAllocCopy(moo->path, path);
	StrAllocCopy(moo->name, name);
	if (value && value[0] == '"' &&
	    value[1] && value[strlen(value)-1] == '"' &&
	    value[strlen(value)-2] != '\\') {
	    value[strlen(value)-1] = '\0';
	    StrAllocCopy(moo->value, value+1);
	    moo->quoted = TRUE;
	} else {
	    StrAllocCopy(moo->value, value);
	}
	moo->pathlen = strlen(moo->path);
	/*
	 *  Justification for following flags:
	 *  COOKIE_FLAG_FROM_FILE    So we know were it comes from.
	 *  COOKIE_FLAG_EXPIRES_SET  It must have had an explicit
	 *			     expiration originally, otherwise
	 *			     it would not be in the file.
	 *  COOKIE_FLAG_DOMAIN_SET,  We don't know whether these were
	 *   COOKIE_FLAG_PATH_SET    explicit or implicit, but this
	 *			     only matters for sending version 1
	 *			     cookies; the cookies read from the
	 *			     file are currently treated all like
	 *			     version 0 (we don't set moo->version)
	 *			     so $Domain= and $Path= will normally
	 *			     not be sent to the server.  But if
	 *			     these cookies somehow get mixed with
	 *			     new version 1 cookies we may end up
	 *			     sending version 1 to the server, and
	 *			     in that case we should send $Domain
	 *			     and $Path.  The state-man-mec drafts
	 *			     and RFC 2109 say that $Domain and
	 *			     $Path SHOULD be omitted if they were
	 *			     not given explicitly, but not that
	 *			     they MUST be omitted.
	 *			     See 8.2 Cookie Spoofing in draft -10
	 *			     for a good reason to send them.
	 *			     However, an explicit domain should be
	 *			     now prefixed with a dot (unless it is
	 *			     for a single host), so we check for
	 *			     that.
	 *  COOKIE_FLAG_SECURE	     Should have "FALSE" for normal,
	 *			     otherwise set it.
	 */
	moo->flags |= COOKIE_FLAG_FROM_FILE | COOKIE_FLAG_EXPIRES_SET |
	    		COOKIE_FLAG_PATH_SET;
	if (domain[0] == '.')
	    moo->flags |= COOKIE_FLAG_DOMAIN_SET;
	if (secure[0] != 'F')
	    moo->flags |= COOKIE_FLAG_SECURE;
	/* @@@ Should we set port to 443 if secure is set? @@@ */
	moo->expires = expires;
	/*
	 * I don't like using this to store the cookies because it's
	 * designed to store cookies that have been received from an
	 * HTTP request, not from a persistent cookie jar.  Hence the
	 * mucking about with the COOKIE_FLAG_FROM_FILE above. - RP
	 */
	store_cookie(moo, domain, path);
    }
    LYCloseInput (cookie_handle);
}

/* rjp - experimental persistent cookie support */
PUBLIC void LYStoreCookies ARGS1 (
	char *,		cookie_file)
{
    HTList *dl, *cl;
    domain_entry *de;
    cookie *co;
    FILE *cookie_handle;
    time_t now = time(NULL); /* system specific? - RP */

    if (!strcmp(cookie_file, "/dev/null")) {
	/* We give /dev/null the Unix meaning, regardless of OS */
	return;
    }

    /*
     *	Check whether we have something to do. - FM
     */
    if (HTList_isEmpty(domain_list) &&
	number_of_file_cookies == 0) {
	/* No cookies now, and haven't read any,
	 * so don't bother updating the file.
	 */
	return;
    }

    CTRACE((tfp, "LYStoreCookies: save cookies to %s on exit\n", cookie_file));

    cookie_handle = LYNewTxtFile (cookie_file);
    if (cookie_handle == NULL) return;
    for (dl = domain_list; dl != NULL; dl = dl->next) {
	de = dl->object;
	if (de == NULL)
	    /*
	     *	Fote says the first object is NULL.  Go with that.
	     */
	    continue;

	/*
	 *  Show the domain's cookies. - FM
	 */
	for (cl = de->cookie_list; cl != NULL; cl = cl->next) {
	    /*
	     *	First object is always NULL. - FM
	     */
	    if ((co = (cookie *)cl->object) == NULL)
		continue;

	    CTRACE((tfp, "LYStoreCookies: %ld cf %ld ", (long) now, (long) co->expires));

	    if ((co->flags & COOKIE_FLAG_DISCARD)) {
		CTRACE((tfp, "not stored - DISCARD\n"));
		continue;
	    } else if (!(co->flags & COOKIE_FLAG_EXPIRES_SET)) {
		CTRACE((tfp, "not stored - no expiration time\n"));
		continue;
	    } else if (co->expires <= now) {
		CTRACE((tfp, "not stored - EXPIRED\n"));
		continue;
	    }

	    fprintf(cookie_handle, "%s\t%s\t%s\t%s\t%ld\t%s\t%s%s%s\n",
		de->domain,
		    (de->domain[0] == '.') ? "TRUE" : "FALSE",
		    co->path,
		co->flags & COOKIE_FLAG_SECURE ? "TRUE" : "FALSE",
		(long) co->expires, co->name,
		    (co->quoted ? "\"" : ""),
		    co->value,
		    (co->quoted ? "\"" : ""));

	    CTRACE((tfp, "STORED\n"));
	}
    }
    LYCloseOutput(cookie_handle);

    HTSYS_purge(cookie_file);
}
#endif

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
    char *buf = NULL;
    char *domain = NULL;
    char *lynxID = NULL;
    HTList *dl, *cl, *next;
    domain_entry *de;
    cookie *co;
    char *name = NULL, *value = NULL, *path = NULL;
    char *comment = NULL, *Address = NULL, *Title = NULL;
    int ch;

    /*
     *	Check whether we have something to do. - FM
     */
    if (HTList_isEmpty(domain_list)) {
	HTProgress(COOKIE_JAR_IS_EMPTY);
	LYSleepMsg();
	HTNoDataOK = 1;
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
	    if (!strcasecomp(domain, de->domain)) {
		FREE(domain);
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
			    {
				FREE(lynxID);
				HTNoDataOK = 1;
				return(HT_NO_DATA);
			    }
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
				FREE(de);
				HTProgress(DOMAIN_EATEN);
			    } else {
				HTProgress(COOKIE_EATEN);
			    }
			    LYSleepMsg();
			    HTNoDataOK = 1;
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
		    HTNoDataOK = 1;
		    while (1) {
			ch = LYgetch_single();
#ifdef VMS
			if (HadVMSInterrupt) {
			    HadVMSInterrupt = FALSE;
			    ch = 'C';
			}
#endif /* VMS */
			switch(ch) {
			    case 'A':
				/*
				 *  Set to accept all cookies
				 *  from this domain. - FM
				 */
				de->bv = ACCEPT_ALWAYS;
				HTUserMsg2(ALWAYS_ALLOWING_COOKIES,
					      de->domain);
				return(HT_NO_DATA);

			    case 'C':
				/*
				 *  Cancelled. - FM
				 */
			      reject:
				HTUserMsg(CANCELLED);
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
				    FREE(de);
				    HTProgress(DOMAIN_EATEN);
				    LYSleepMsg();
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
				LYSleepMsg();
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
				    FREE(de);
				    HTProgress(DOMAIN_EATEN);
				    LYSleepMsg();
				}
				break;

			    case 'P':
				/*
				 *  Set to prompt for cookie acceptance
				 *  from this domain. - FM
				 */
				de->bv = QUERY_USER;
				HTUserMsg2(PROMPTING_TO_ALLOW_COOKIES,
					   de->domain);
				return(HT_NO_DATA);

			    case 'V':
				/*
				 *  Set to reject all cookies
				 *  from this domain. - FM
				 */
				de->bv = REJECT_ALWAYS;
				HTUserMsg2(NEVER_ALLOWING_COOKIES,
					   de->domain);
				if ((!HTList_isEmpty(de->cookie_list)) &&
				    HTConfirm(DELETE_ALL_COOKIES_IN_DOMAIN))
				    goto Delete_all_cookies_in_domain;
				return(HT_NO_DATA);

			    default:
				if (LYCharIsINTERRUPT(ch))
				    goto reject;
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
	     *	There are no more domains left.
	     *	Don't delete the domain_list, otherwise
	     *  atexit may be called multiple times. - kw
	     */
	    HTProgress(ALL_COOKIES_EATEN);
	    LYSleepMsg();
	}
	FREE(domain);
	FREE(lynxID);
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
	HTSprintf0(&buf, CANNOT_CONVERT_I_TO_O,
		HTAtom_name(format_in), HTAtom_name(format_out));
	HTAlert(buf);
	FREE(buf);
	return(HT_NOT_LOADED);
    }

    /*
     *	Load HTML strings into buf and pass buf
     *	to the target for parsing and rendering. - FM
     */
#define PUTS(buf)    (*target->isa->put_block)(target, buf, strlen(buf))


    HTSprintf0(&buf, "<html>\n<head>\n<title>%s</title>\n</head>\n<body>\n",
		 COOKIE_JAR_TITLE);
    PUTS(buf);
    HTSprintf0(&buf, "<h1>%s (%s)%s<a href=\"%s%s\">%s</a></h1>\n",
	LYNX_NAME, LYNX_VERSION,
	HELP_ON_SEGMENT,
	helpfilepath, COOKIE_JAR_HELP, COOKIE_JAR_TITLE);
    PUTS(buf);

    HTSprintf0(&buf, "<note>%s\n", ACTIVATE_TO_GOBBLE);
    PUTS(buf);
    HTSprintf0(&buf, "%s</note>\n", OR_CHANGE_ALLOW);
    PUTS(buf);

    HTSprintf0(&buf, "<dl compact>\n");
    PUTS(buf);
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
	HTSprintf0(&buf, "<dt>%s<dd><a href=\"LYNXCOOKIE://%s/\">Domain=%s</a>\n",
		      de->domain, de->domain, de->domain);
	PUTS(buf);
	switch (de->bv) {
	    case (ACCEPT_ALWAYS):
		HTSprintf0(&buf, COOKIES_ALWAYS_ALLOWED);
		break;
	    case (REJECT_ALWAYS):
		HTSprintf0(&buf, COOKIES_NEVER_ALLOWED);
		break;
	    case (QUERY_USER):
		HTSprintf0(&buf, COOKIES_ALLOWED_VIA_PROMPT);
		break;
	}
	PUTS(buf);
	HTSprintf0(&buf, "\n");
	PUTS(buf);

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
	    HTSprintf0(&buf, "<dd><a href=\"LYNXCOOKIE://%s/%s\">%s=%s</a>\n",
			 de->domain, co->lynxID, name, value);
	    FREE(name);
	    FREE(value);
	    PUTS(buf);

	    if (co->flags & COOKIE_FLAG_FROM_FILE) {
		HTSprintf0(&buf, "%s\n", gettext("(from a previous session)"));
	        PUTS(buf);
	    }

	    /*
	     *	Show the path, port, secure and discard setting. - FM
	     */
	    if (co->path) {
		StrAllocCopy(path, co->path);
		LYEntify(&path, TRUE);
	    } else {
		StrAllocCopy(path, "/");
	    }
	    HTSprintf0(&buf, "<dd>Path=%s\n<dd>Port: %d Secure: %s Discard: %s\n",
			 path, co->port,
			 ((co->flags & COOKIE_FLAG_SECURE) ? "YES" : "NO"),
			 ((co->flags & COOKIE_FLAG_DISCARD) ? "YES" : "NO"));
	    FREE(path);
	    PUTS(buf);

	    /*
	     *	Show the list of acceptable ports, if present. - FM
	     */
	    if (co->PortList) {
		HTSprintf0(&buf, "<dD>PortList=\"%s\"\n", co->PortList);
		PUTS(buf);
	    }

	    /*
	     *	Show the commentURL, if we have one. - FM
	     */
	    if (co->commentURL) {
		StrAllocCopy(Address, co->commentURL);
		LYEntify(&Address, FALSE);
		StrAllocCopy(Title, co->commentURL);
		LYEntify(&Title, TRUE);
		HTSprintf0(&buf,
			"<dd>CommentURL: <a href=\"%s\">%s</a>\n",
			Address,
			Title);
		FREE(Address);
		FREE(Title);
		PUTS(buf);
	    }

	    /*
	     *	Show the comment, if we have one. - FM
	     */
	    if (co->comment) {
		StrAllocCopy(comment, co->comment);
		LYEntify(&comment, TRUE);
		HTSprintf0(&buf, "<dd>Comment: %s\n", comment);
		FREE(comment);
		PUTS(buf);
	    }

	    /*
	     *	Show the Maximum Gobble Date. - FM
	     */
	    HTSprintf0(&buf, "<dd><em>%s</em> %s%s",
	    		 gettext("Maximum Gobble Date:"),
			 ((co->flags & COOKIE_FLAG_EXPIRES_SET)
					    ?
			ctime(&co->expires) : END_OF_SESSION),
			 ((co->flags & COOKIE_FLAG_EXPIRES_SET)
					    ?
					 "" : "\n"));
	    PUTS(buf);
	}
	HTSprintf0(&buf, "</dt>\n");
	PUTS(buf);
    }
    HTSprintf0(&buf, "</dl>\n</body>\n</html>\n");
    PUTS(buf);

    /*
     *	Free the target to complete loading of the
     *	Cookie Jar Page, and report a successful load. - FM
     */
    (*target->isa->_free)(target);
    FREE(buf);
    return(HT_LOADED);
}


/*      cookie_domain_flag_set
**      ----------------------
**      All purpose function to handle setting domain flags for a
**      comma-delimited list of domains.  cookie_domain_flags handles
**      invcheck behavior, as well as accept/reject behavior. - BJP
*/

PUBLIC void cookie_domain_flag_set ARGS2(
	char *, 	domainstr,
	int, 		flag)
{
    domain_entry *de = NULL;
    domain_entry *de2 = NULL;
    HTList *hl = NULL;
    char **str = typecalloc(char *);
    char *dstr = NULL;
    char *strsmall = NULL;
    int isexisting = FALSE;

    if (str == NULL) {
	HTAlwaysAlert(gettext("Internal"),
		      gettext("cookie_domain_flag_set error, aborting program"));
	exit_immediately(EXIT_FAILURE);
    }

    /*
     * Is this the first domain we're handling?  If so, initialize
     * domain_list.
     */

    if (domain_list == NULL) {
#ifdef LY_FIND_LEAKS
	atexit(LYCookieJar_free);
#endif
	domain_list = HTList_new();
	total_cookies = 0;
    }

    StrAllocCopy(dstr, domainstr);

    *str = dstr;

    while ((strsmall = LYstrsep(str, ",")) != 0) {

	if (*strsmall == '\0')
	    /* Never add a domain for empty string.  It would actually
	     * make more sense to use strtok here. - kw */
	    continue;

	/*
	 * Check the list of existing domains to see if this is a
	 * re-setting of an already existing domains -- if so, just
	 * change the behavior, if not, create a new domain entry.
	 */

	for (hl = domain_list; hl != NULL; hl = hl->next) {
	    de2 = (domain_entry *)hl->object;
	    if ((de2 != NULL && de2->domain != NULL) &&
		!strcasecomp(strsmall, de2->domain)) {
			isexisting = TRUE;
			break;
	    } else {
		isexisting = FALSE;
	    }
	}

	if(!isexisting) {
	    de = typecalloc(domain_entry);

	    if (de == NULL)
		    outofmem(__FILE__, "cookie_domain_flag_set");

	    switch(flag) {
		case (FLAG_ACCEPT_ALWAYS): de->bv = ACCEPT_ALWAYS;
					   de->invcheck_bv = DEFAULT_INVCHECK_BV;
					   break;
		case (FLAG_REJECT_ALWAYS): de->bv = REJECT_ALWAYS;
					   de->invcheck_bv = DEFAULT_INVCHECK_BV;
					   break;
		case (FLAG_QUERY_USER):    de->bv = QUERY_USER;
					   de->invcheck_bv = DEFAULT_INVCHECK_BV;
					   break;
		case (FLAG_INVCHECK_QUERY): de->invcheck_bv = INVCHECK_QUERY;
					    de->bv = QUERY_USER;
					    break;
		case (FLAG_INVCHECK_STRICT): de->invcheck_bv = INVCHECK_STRICT;
					     de->bv = QUERY_USER;
					    break;
		case (FLAG_INVCHECK_LOOSE): de->invcheck_bv = INVCHECK_LOOSE;
					    de->bv = QUERY_USER;
					    break;
	    }

	    StrAllocCopy(de->domain, strsmall);
	    de->cookie_list = HTList_new();
	    HTList_appendObject(domain_list, de);
	} else {
	    switch(flag) {
		case (FLAG_ACCEPT_ALWAYS): de2->bv = ACCEPT_ALWAYS;
					   break;
		case (FLAG_REJECT_ALWAYS): de2->bv = REJECT_ALWAYS;
					   break;
		case (FLAG_QUERY_USER): de2->bv = QUERY_USER;
					   break;
		case (FLAG_INVCHECK_QUERY): de2->invcheck_bv = INVCHECK_QUERY;
					   break;
		case (FLAG_INVCHECK_STRICT): de2->invcheck_bv = INVCHECK_STRICT;
					   break;
		case (FLAG_INVCHECK_LOOSE): de2->invcheck_bv = INVCHECK_LOOSE;
					   break;
	    }
	}
    }

    FREE(strsmall);
    FREE(str);
    FREE(dstr);
}

/*
 * If any COOKIE_{ACCEPT,REJECT}_DOMAINS have been defined, process them. 
 * These are comma delimited lists of domains.  - BJP
 *
 * And for query/strict/loose invalid cookie checking.  - BJP
 */
PUBLIC void LYConfigCookies NOARGS
{
    static CONST struct {
	char **domain;
	int flag;
	int once;
    } table[] = {
	{ &LYCookieSAcceptDomains,	FLAG_ACCEPT_ALWAYS,   TRUE },
	{ &LYCookieSRejectDomains,	FLAG_REJECT_ALWAYS,   TRUE },
	{ &LYCookieSStrictCheckDomains, FLAG_INVCHECK_STRICT, TRUE },
	{ &LYCookieSLooseCheckDomains,	FLAG_INVCHECK_LOOSE,  TRUE },
	{ &LYCookieSQueryCheckDomains,	FLAG_INVCHECK_QUERY,  TRUE },
	{ &LYCookieAcceptDomains,	FLAG_ACCEPT_ALWAYS,   FALSE },
	{ &LYCookieRejectDomains,	FLAG_REJECT_ALWAYS,   FALSE },
	{ &LYCookieStrictCheckDomains,	FLAG_INVCHECK_STRICT, FALSE },
	{ &LYCookieLooseCheckDomains,	FLAG_INVCHECK_LOOSE,  FALSE },
	{ &LYCookieQueryCheckDomains,	FLAG_INVCHECK_QUERY,  FALSE },
    };
    unsigned n;

    for (n = 0; n < TABLESIZE(table); n++) {
	if (*(table[n].domain) != NULL) {
	    cookie_domain_flag_set(*(table[n].domain), table[n].flag);
	    /*
	     * Discard the value for system settings after we've used them.
	     * The local settings will be merged with the contents of .lynxrc
	     */
	    if (table[n].once) {
		FREE(*(table[n].domain));
	    }
	}
    }
}

#ifdef GLOBALDEF_IS_MACRO
#define _LYCOOKIE_C_GLOBALDEF_1_INIT { "LYNXCOOKIE",LYHandleCookies,0}
GLOBALDEF (HTProtocol,LYLynxCookies,_LYCOOKIE_C_GLOBALDEF_1_INIT);
#else
GLOBALDEF PUBLIC HTProtocol LYLynxCookies = {"LYNXCOOKIE",LYHandleCookies,0};
#endif /* GLOBALDEF_IS_MACRO */
