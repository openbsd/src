/*		Parse HyperText Document Address		HTParse.c
**		================================
*/

#include <HTUtils.h>
#include <HTParse.h>

#include <LYUtils.h>
#include <LYLeaks.h>
#include <LYStrings.h>
#include <LYCharUtils.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifdef __MINGW32__
#include <malloc.h>
#endif /* __MINGW32__ */
#endif

#define HEX_ESCAPE '%'

struct struct_parts {
	char * access;
	char * host;
	char * absolute;
	char * relative;
	char * search;		/* treated normally as part of path */
	char * anchor;
};


/*	Strip white space off a string.				HTStrip()
**	-------------------------------
**
** On exit,
**	Return value points to first non-white character, or to 0 if none.
**	All trailing white space is OVERWRITTEN with zero.
*/
PUBLIC char * HTStrip ARGS1(
	char *,		s)
{
#define SPACE(c) ((c == ' ') || (c == '\t') || (c == '\n'))
    char * p = s;
    for (p = s; *p; p++)
	;			/* Find end of string */
    for (p--; p >= s; p--) {
	if (SPACE(*p))
	    *p = '\0';		/* Zap trailing blanks */
	else
	    break;
    }
    while (SPACE(*s))
	s++;			/* Strip leading blanks */
    return s;
}

/*	Scan a filename for its constituents.			scan()
**	-------------------------------------
**
** On entry,
**	name	points to a document name which may be incomplete.
** On exit,
**	absolute or relative may be nonzero (but not both).
**	host, anchor and access may be nonzero if they were specified.
**	Any which are nonzero point to zero terminated strings.
*/
PRIVATE void scan ARGS2(
	char *,			name,
	struct struct_parts *,	parts)
{
    char * after_access;
    char * p;

    parts->access = NULL;
    parts->host = NULL;
    parts->absolute = NULL;
    parts->relative = NULL;
    parts->search = NULL;	/* normally not used - kw */
    parts->anchor = NULL;

    /*
    **	Scan left-to-right for a scheme (access).
    */
    after_access = name;
    for (p = name; *p; p++) {
	if (*p==':') {
	    *p = '\0';
	    parts->access = name;	/* Access name has been specified */
	    after_access = (p + 1);
	    break;
	}
	if (*p == '/' || *p == '#' || *p == ';' || *p == '?')
	    break;
    }

    /*
    **	Scan left-to-right for a fragment (anchor).
    */
    for (p = after_access; *p; p++) {
	if (*p =='#') {
	    parts->anchor = (p + 1);
	    *p = '\0';			/* terminate the rest */
	    break;		/* leave things after first # alone - kw */
	}
    }

    /*
    **	Scan left-to-right for a host or absolute path.
    */
    p = after_access;
    if (*p == '/') {
	if (p[1] == '/') {
	    parts->host = (p + 2);	  /* host has been specified	*/
	    *p = '\0';			  /* Terminate access		*/
	    p = strchr(parts->host, '/'); /* look for end of host name if any */
	    if (p != NULL) {
		*p = '\0';			/* Terminate host */
		parts->absolute = (p + 1);	/* Root has been found */
	    } else {
		p = strchr(parts->host, '?');
		if (p != NULL) {
		    *p = '\0';			/* Terminate host */
		    parts->search = (p + 1);
		}
	    }
	} else {
	    parts->absolute = (p + 1);		/* Root found but no host */
	}
    } else {
	parts->relative = (*after_access) ?
			     after_access : NULL; /* NULL for "" */
    }

    /*
    **	Check schemes that commonly have unescaped hashes.
    */
    if (parts->access && parts->anchor &&
		/* optimize */ strchr("lnsdLNSD", *parts->access) != NULL) {
	if ((!parts->host && strcasecomp(parts->access, "lynxcgi")) ||
	    !strcasecomp(parts->access, "nntp") ||
	    !strcasecomp(parts->access, "snews") ||
	    !strcasecomp(parts->access, "news") ||
	    !strcasecomp(parts->access, "data")) {
	    /*
	     *	Access specified but no host and not a lynxcgi URL, so the
	     *	anchor may not really be one, e.g., news:j462#36487@foo.bar,
	     *	or it's an nntp or snews URL, or news URL with a host.
	     *	Restore the '#' in the address.
	     */
	    /* but only if we have found a path component of which this will
	     * become part. - kw  */
	    if (parts->relative || parts->absolute) {
		*(parts->anchor - 1) = '#';
		parts->anchor = NULL;
	    }
	}
    }
} /*scan */

#if defined(HAVE_ALLOCA) && !defined(LY_FIND_LEAKS)
#define LYalloca(x)        alloca(x)
#define LYalloca_free(x)   {}
#else
#define LYalloca(x)        malloc(x)
#define LYalloca_free(x)   free(x)
#endif

/*	Parse a Name relative to another name.			HTParse()
**	--------------------------------------
**
**	This returns those parts of a name which are given (and requested)
**	substituting bits from the related name where necessary.
**
** On entry,
**	aName		A filename given
**	relatedName	A name relative to which aName is to be parsed
**	wanted		A mask for the bits which are wanted.
**
** On exit,
**     returns         A pointer to a malloc'd string which MUST BE FREED
*/
PUBLIC char * HTParse ARGS3(
	CONST char *,	aName,
	CONST char *,	relatedName,
	int,		wanted)
{
    char * result = NULL;
    char * tail = NULL;  /* a pointer to the end of the 'result' string */
    char * return_value = NULL;
    int len, len1, len2;
    char * name = NULL;
    char * rel = NULL;
    char * p;
    char * acc_method;
    struct struct_parts given, related;

    CTRACE((tfp, "HTParse: aName:`%s'\n", aName));
    CTRACE((tfp, "   relatedName:`%s'\n", relatedName));

    if (wanted & (PARSE_STRICTPATH | PARSE_QUERY)) { /* if detail wanted... */
	if ((wanted & (PARSE_STRICTPATH | PARSE_QUERY))
	    == (PARSE_STRICTPATH | PARSE_QUERY)) /* if strictpath AND query */
	    wanted |= PARSE_PATH; /* then treat as if PARSE_PATH wanted */
	if (wanted & PARSE_PATH) /* if PARSE_PATH wanted */
	    wanted &= ~(PARSE_STRICTPATH | PARSE_QUERY); /* ignore details */
    }
    CTRACE((tfp, "   want:%s%s%s%s%s%s%s\n",
	    wanted & PARSE_PUNCTUATION ? " punc"   : "",
	    wanted & PARSE_ANCHOR      ? " anchor" : "",
	    wanted & PARSE_PATH        ? " path"   : "",
	    wanted & PARSE_HOST        ? " host"   : "",
	    wanted & PARSE_ACCESS      ? " access" : "",
	    wanted & PARSE_STRICTPATH  ? " PATH"   : "",
	    wanted & PARSE_QUERY       ? " QUERY"  : ""));

    /*
    ** Allocate the temporary string. Optimized.
    */
    len1 = strlen(aName) + 1;
    len2 = strlen(relatedName) + 1;
    len = len1 + len2 + 8;     /* Lots of space: more than enough */

    result = tail = (char*)LYalloca(len * 2 + len1 + len2);
    if (result == NULL) {
	outofmem(__FILE__, "HTParse");
    }
    *result = '\0';
    name = result + len;
    rel = name + len1;

    /*
    **	Make working copy of the input string to cut up.
    */
    memcpy(name, aName, len1);

    /*
    **	Cut up the string into URL fields.
    */
    scan(name, &given);

    /*
    **	Now related string.
    */
    if ((given.access && given.host && given.absolute) || !*relatedName) {
	/*
	**  Inherit nothing!
	*/
	related.access = NULL;
	related.host = NULL;
	related.absolute = NULL;
	related.relative = NULL;
	related.search = NULL;
	related.anchor = NULL;
    } else {
	memcpy(rel, relatedName, len2);
	scan(rel,  &related);
    }


    /*
    **	Handle the scheme (access) field.
    */
    if (given.access && given.host && !given.relative && !given.absolute) {
	if (!strcmp(given.access, "http") ||
	    !strcmp(given.access, "https") ||
	    !strcmp(given.access, "ftp"))
	    /*
	    **	Assume root.
	    */
	    given.absolute = "";
    }
    acc_method = given.access ? given.access : related.access;
    if (wanted & PARSE_ACCESS) {
	if (acc_method) {
	    strcpy(tail, acc_method);
	    tail += strlen(tail);
	    if (wanted & PARSE_PUNCTUATION) {
		*tail++ = ':';
		*tail = '\0';
	    }
	}
    }

    /*
    **	If different schemes, inherit nothing.
    **
    **	We'll try complying with RFC 1808 and
    **	the Fielding draft, and inherit nothing
    **	if both schemes are given, rather than
    **	only when they differ, except for
    **	file URLs - FM
    **
    **	After trying it for a while, it's still
    **	premature, IHMO, to go along with it, so
    **	this is back to inheriting for identical
    **	schemes whether or not they are "file".
    **	If you want to try it again yourself,
    **	uncomment the strcasecomp() below. - FM
    */
    if ((given.access && related.access) &&
	(/* strcasecomp(given.access, "file") || */
	 strcmp(given.access, related.access))) {
	related.host = NULL;
	related.absolute = NULL;
	related.relative = NULL;
	related.search = NULL;
	related.anchor = NULL;
    }

    /*
    **	Handle the host field.
    */
    if (wanted & PARSE_HOST) {
	if (given.host || related.host) {
	    if (wanted & PARSE_PUNCTUATION) {
		*tail++ = '/';
		*tail++ = '/';
	    }
	    strcpy(tail, given.host ? given.host : related.host);
#define CLEAN_URLS
#ifdef CLEAN_URLS
	    /*
	    **	Ignore default port numbers, and trailing dots on FQDNs,
	    **	which will only cause identical addresses to look different.
	    **  (related is already a clean url).
	    */
	    {
		char *p2, *h;
		if ((p2 = strchr(result, '@')) != NULL)
		   tail = (p2 + 1);
		p2 = strchr(tail, ':');
		if (p2 != NULL && !isdigit(UCH(p2[1])))
		    /*
		    **	Colon not followed by a port number.
		    */
		    *p2 = '\0';
		if (p2 != NULL && *p2 != '\0' && acc_method != NULL) {
		    /*
		    **	Port specified.
		    */
		    if ((!strcmp(acc_method, "http"	 ) && !strcmp(p2, ":80" )) ||
			(!strcmp(acc_method, "https"	 ) && !strcmp(p2, ":443")) ||
			(!strcmp(acc_method, "gopher"	 ) && !strcmp(p2, ":70" )) ||
			(!strcmp(acc_method, "ftp"	 ) && !strcmp(p2, ":21" )) ||
			(!strcmp(acc_method, "wais"	 ) && !strcmp(p2, ":210")) ||
			(!strcmp(acc_method, "nntp"	 ) && !strcmp(p2, ":119")) ||
			(!strcmp(acc_method, "news"	 ) && !strcmp(p2, ":119")) ||
			(!strcmp(acc_method, "newspost"  ) && !strcmp(p2, ":119")) ||
			(!strcmp(acc_method, "newsreply" ) && !strcmp(p2, ":119")) ||
			(!strcmp(acc_method, "snews"	 ) && !strcmp(p2, ":563")) ||
			(!strcmp(acc_method, "snewspost" ) && !strcmp(p2, ":563")) ||
			(!strcmp(acc_method, "snewsreply") && !strcmp(p2, ":563")) ||
			(!strcmp(acc_method, "finger"	 ) && !strcmp(p2, ":79" )) ||
			(!strcmp(acc_method, "telnet"	 ) && !strcmp(p2, ":23" )) ||
			(!strcmp(acc_method, "tn3270"	 ) && !strcmp(p2, ":23" )) ||
			(!strcmp(acc_method, "rlogin"	 ) && !strcmp(p2, ":513")) ||
			(!strcmp(acc_method, "cso"	 ) && !strcmp(p2, ":105")))
		    *p2 = '\0'; /* It is the default: ignore it */
		}
		if (p2 == NULL) {
		    int len3 = strlen(tail);

		    if (len3 > 0) {
			h = tail + len3 - 1;	/* last char of hostname */
			if (*h == '.')
			    *h = '\0';		/* chop final . */
		    }
		} else if (p2 != result) {
		    h = p2;
		    h--;		/* End of hostname */
		    if (*h == '.') {
			/*
			**  Slide p2 over h.
			*/
			while (*p2 != '\0')
			    *h++ = *p2++;
			*h = '\0';	/* terminate */
		    }
		}
	    }
#endif /* CLEAN_URLS */
	}
    }

    /*
     * Trim any blanks from the result so far - there's no excuse for blanks
     * in a hostname.  Also update the tail here.
     */
    tail = LYRemoveBlanks(result);

    /*
    **	If host in given or related was ended directly with a '?' (no
    **  slash), fake the search part into absolute.  This is the only
    **  case search is returned from scan.  A host must have been present.
    **  this restores the '?' at which the host part had been truncated in
    **  scan, we have to do this after host part handling is done. - kw
    */
    if (given.search && *(given.search - 1) == '\0') {
	given.absolute = given.search - 1;
	given.absolute[0] = '?';
    } else if (related.search && !related.absolute &&
	       *(related.search - 1) == '\0') {
	related.absolute = related.search - 1;
	related.absolute[0] = '?';
    }

    /*
    **	If different hosts, inherit no path.
    */
    if (given.host && related.host)
	if (strcmp(given.host, related.host) != 0) {
	    related.absolute = NULL;
	    related.relative = NULL;
	    related.anchor = NULL;
	}

    /*
    **	Handle the path.
    */
    if (wanted & (PARSE_PATH | PARSE_STRICTPATH | PARSE_QUERY)) {
	int want_detail = (wanted & (PARSE_STRICTPATH | PARSE_QUERY));

	if (acc_method && !given.absolute && given.relative) {
	    /*
	     * Treat all given nntp or snews paths, or given paths for news
	     * URLs with a host, as absolute.
	     */
	    switch (*acc_method) {
	    case 'N':
	    case 'n':
		if (!strcasecomp(acc_method, "nntp") ||
		    (!strcasecomp(acc_method, "news") &&
		     !strncasecomp(result, "news://", 7))) {
		    given.absolute = given.relative;
		    given.relative = NULL;
		}
		break;
	    case 'S':
	    case 's':
		if (!strcasecomp(acc_method, "snews")) {
		    given.absolute = given.relative;
		    given.relative = NULL;
		}
		break;
	    }
	}

	if (given.absolute) {			/* All is given */
	    if (wanted & PARSE_PUNCTUATION)
		*tail++ = '/';
	    strcpy(tail, given.absolute);
	    CTRACE((tfp, "HTParse: (ABS)\n"));
	} else if (related.absolute) {		/* Adopt path not name */
	    *tail++ = '/';
	    strcpy(tail, related.absolute);
	    if (given.relative) {
		p = strchr(tail, '?');	/* Search part? */
		if (p == NULL)
		    p = (tail + strlen(tail) - 1);
		for (; *p != '/'; p--)
		    ;				/* last / */
		p[1] = '\0';			/* Remove filename */
		strcat(p, given.relative); /* Add given one */
		HTSimplify (result);
	    }
	    CTRACE((tfp, "HTParse: (Related-ABS)\n"));
	} else if (given.relative) {
	    strcpy(tail, given.relative);		/* what we've got */
	    CTRACE((tfp, "HTParse: (REL)\n"));
	} else if (related.relative) {
	    strcpy(tail, related.relative);
	    CTRACE((tfp, "HTParse: (Related-REL)\n"));
	} else {  /* No inheritance */
	    if (!isLYNXCGI(aName) &&
		!isLYNXEXEC(aName) &&
		!isLYNXPROG(aName)) {
		*tail++ = '/';
		*tail = '\0';
	    }
	    if (!strcmp(result, "news:/"))
		result[5] = '*';
	    CTRACE((tfp, "HTParse: (No inheritance)\n"));
	}
	if (want_detail) {
	    p = strchr(tail, '?');	/* Search part? */
	    if (p) {
		if (PARSE_STRICTPATH) {
		    *p = '\0';
		} else {
		    if (!(wanted & PARSE_PUNCTUATION))
			p++;
		    do {
			*tail++ = *p;
		    } while (*p++);
		}
	    } else {
		if (wanted & PARSE_QUERY)
		    *tail = '\0';
	    }
	}
    }

    /*
    **	Handle the fragment (anchor). Never inherit.
    */
    if (wanted & PARSE_ANCHOR) {
	if (given.anchor && *given.anchor) {
	    tail += strlen(tail);
	    if (wanted & PARSE_PUNCTUATION)
		*tail++ = '#';
	    strcpy(tail, given.anchor);
	}
    }

    /*
     * If there are any blanks remaining in the string, escape them as needed.
     * See the discussion in LYLegitimizeHREF() for example.
     */
    if ((p = strchr(result, ' ')) != 0) {
	switch (is_url(result)) {
	case UNKNOWN_URL_TYPE:
	    CTRACE((tfp, "HTParse:      ignore:`%s'\n", result));
	    break;
	case LYNXEXEC_URL_TYPE:
	case LYNXPROG_URL_TYPE:
	case LYNXCGI_URL_TYPE:
	case LYNXPRINT_URL_TYPE:
	case LYNXHIST_URL_TYPE:
	case LYNXDOWNLOAD_URL_TYPE:
	case LYNXKEYMAP_URL_TYPE:
	case LYNXIMGMAP_URL_TYPE:
	case LYNXCOOKIE_URL_TYPE:
	case LYNXDIRED_URL_TYPE:
	case LYNXOPTIONS_URL_TYPE:
	case LYNXCFG_URL_TYPE:
	case LYNXCOMPILE_OPTS_URL_TYPE:
	case LYNXMESSAGES_URL_TYPE:
	    CTRACE((tfp, "HTParse:      spaces:`%s'\n", result));
	    break;
	case NOT_A_URL_TYPE:
	default:
	    CTRACE((tfp, "HTParse:      encode:`%s'\n", result));
	    do {
		char *q = p + strlen(p) + 2;
		while (q != p + 1) {
		    q[0] = q[-2];
		    --q;
		}
		p[0] = '%';
		p[1] = '2';
		p[2] = '0';
	    } while ((p = strchr(result, ' ')) != 0);
	    break;
	}
    }
    CTRACE((tfp, "HTParse:      result:`%s'\n", result));

    StrAllocCopy(return_value, result);
    LYalloca_free(result);

    /* FIXME: could be optimized using HTParse() internals */
    if (*relatedName &&
	((wanted & PARSE_ALL_WITHOUT_ANCHOR) == PARSE_ALL_WITHOUT_ANCHOR)) {
	/*
	 *  Check whether to fill in localhost. - FM
	 */
	LYFillLocalFileURL(&return_value, relatedName);
	CTRACE((tfp, "pass LYFillLocalFile:`%s'\n", return_value));
    }

    return return_value;		/* exactly the right length */
}

/*	HTParseAnchor(), fast HTParse() specialization
**	----------------------------------------------
**
** On exit,
**	returns		A pointer within input string (probably to its end '\0')
*/
PUBLIC CONST char * HTParseAnchor ARGS1(
	CONST char *,	aName)
{
    CONST char* p = aName;
    for ( ; *p && *p != '#'; p++)
	;
    if (*p == '#') {
	/* the safe way based on HTParse() -
	 * keeping in mind scan() peculiarities on schemes:
	 */
	struct struct_parts given;

	char* name = (char*)LYalloca((p - aName) + strlen(p) + 1);
	if (name == NULL) {
	    outofmem(__FILE__, "HTParseAnchor");
	}
	strcpy(name, aName);
	scan(name, &given);
	LYalloca_free(name);

	p++; /*next to '#'*/
	if (given.anchor == NULL) {
	    for ( ; *p; p++)  /*scroll to end '\0'*/
		;
	}
    }
    return p;
}

/*	Simplify a filename.				HTSimplify()
**	--------------------
**
**  A unix-style file is allowed to contain the sequence xxx/../ which may
**  be replaced by "" , and the sequence "/./" which may be replaced by "/".
**  Simplification helps us recognize duplicate filenames.
**
**	Thus,	/etc/junk/../fred	becomes /etc/fred
**		/etc/junk/./fred	becomes /etc/junk/fred
**
**	but we should NOT change
**		http://fred.xxx.edu/../..
**
**	or	../../albert.html
*/
PUBLIC void HTSimplify ARGS1(
	char *,		filename)
{
    char *p;
    char *q, *q1;

    if (filename == NULL)
	return;

    if (!(filename[0] && filename[1]) ||
	filename[0] == '?' || filename[1] == '?' || filename[2] == '?')
	return;

    if (strchr(filename, '/') != NULL) {
	for (p = (filename + 2); *p; p++) {
	    if (*p == '?') {
		/*
		**  We're still treating a ?searchpart as part of
		**  the path in HTParse() and scan(), but if we
		**  encounter a '?' here, assume it's the delimiter
		**  and break.	We also could check for a parameter
		**  delimiter (';') here, but the current Fielding
		**  draft (wisely or ill-advisedly :) says that it
		**  should be ignored and collapsing be allowed in
		**  it's value).  The only defined parameter at
		**  present is ;type=[A, I, or D] for ftp URLs, so
		**  if there's a "/..", "/../", "/./", or terminal
		**  '.' following the ';', it must be due to the
		**  ';' being an unescaped path character and not
		**  actually a parameter delimiter. - FM
		*/
		break;
	    }
	    if (*p == '/') {
		if ((p[1] == '.') && (p[2] == '.') &&
		    (p[3] == '/' || p[3] == '?' || p[3] == '\0')) {
		    /*
		    **	Handle "../", "..?" or "..".
		    */
		    for (q = (p - 1); (q >= filename) && (*q != '/'); q--)
			/*
			**  Back up to previous slash or beginning of string.
			*/
			;
		    if ((q[0] == '/') &&
			(strncmp(q, "/../", 4) &&
			 strncmp(q, "/..?", 4)) &&
			!((q - 1) > filename && q[-1] == '/')) {
			/*
			**  Not at beginning of string or in a
			**  host field, so remove the "/xxx/..".
			*/
			q1 = (p + 3);
			p = q;
			while (*q1 != '\0')
			    *p++ = *q1++;
			*p = '\0';		/* terminate */
			/*
			**  Start again with previous slash.
			*/
			p = (q - 1);
		    }
		} else if (p[1] == '.' && p[2] == '/') {
		    /*
		    **	Handle "./" by removing both characters.
		    */
		    q = p;
		    q1 = (p + 2);
		    while (*q1 != '\0')
			*q++ = *q1++;
		    *q = '\0';		/* terminate */
		    p--;
		} else if (p[1] == '.' && p[2] == '?') {
		    /*
		    **	Handle ".?" by removing the dot.
		    */
		    q = (p + 1);
		    q1 = (p + 2);
		    while (*q1 != '\0')
			*q++ = *q1++;
		    *q = '\0';		/* terminate */
		    p--;
		} else if (p[1] == '.' && p[2] == '\0') {
		    /*
		    **	Handle terminal "." by removing the character.
		    */
		    p[1] = '\0';
		}
	    }
	}
	if (p >= filename + 2 && *p == '?' && *(p-1)  == '.') {
	    if (*(p-2) == '/') {
		/*
		**  Handle "/.?" by removing the dot.
		*/
		q = p - 1;
		q1 = p;
		while (*q1 != '\0')
		    *q++ = *q1++;
		*q = '\0';
	    } else if (*(p-2) == '.' &&
		       p >= filename + 4 && *(p-3) == '/' &&
		       (*(p-4) != '/' ||
			(p > filename + 4 && *(p-5) != ':'))) {
		    /*
		    **	Handle "xxx/..?"
		    */
		for (q = (p - 4); (q > filename) && (*q != '/'); q--)
			/*
			**  Back up to previous slash or beginning of string.
			*/
		    ;
		if (*q == '/') {
		    if (q > filename && *(q-1) == '/' &&
			!(q > filename + 1 && *(q-1) != ':'))
			return;
		    q++;
		}
		if (strncmp(q, "../", 3) && strncmp(q, "./", 2)) {
			/*
			**  Not after "//" at beginning of string or
			**  after "://", and xxx is not ".." or ".",
			**  so remove the "xxx/..".
			*/
		    q1 = p;
		    p = q;
		    while (*q1 != '\0')
			*p++ = *q1++;
		    *p = '\0';		/* terminate */
		}
	    }
	}
    }
}

/*	Make Relative Name.					HTRelative()
**	-------------------
**
** This function creates and returns a string which gives an expression of
** one address as related to another.  Where there is no relation, an absolute
** address is returned.
**
**  On entry,
**	Both names must be absolute, fully qualified names of nodes
**	(no anchor bits)
**
**  On exit,
**	The return result points to a newly allocated name which, if
**	parsed by HTParse relative to relatedName, will yield aName.
**	The caller is responsible for freeing the resulting name later.
**
*/
PUBLIC char * HTRelative ARGS2(
	CONST char *,	aName,
	CONST char *,	relatedName)
{
    char * result = NULL;
    CONST char *p = aName;
    CONST char *q = relatedName;
    CONST char * after_access = NULL;
    CONST char * path = NULL;
    CONST char * last_slash = NULL;
    int slashes = 0;

    for (; *p; p++, q++) {	/* Find extent of match */
	if (*p != *q)
	    break;
	if (*p == ':')
	    after_access = p+1;
	if (*p == '/') {
	    last_slash = p;
	    slashes++;
	    if (slashes == 3)
		path=p;
	}
    }

    /* q, p point to the first non-matching character or zero */

    if (!after_access) {			/* Different access */
	StrAllocCopy(result, aName);
    } else if (slashes < 3){			/* Different nodes */
	StrAllocCopy(result, after_access);
    } else if (slashes == 3){			/* Same node, different path */
	StrAllocCopy(result, path);
    } else {					/* Some path in common */
	int levels = 0;
	for (; *q && (*q != '#'); q++)
	    if (*q == '/')
		levels++;
	result = typecallocn(char, 3*levels + strlen(last_slash) + 1);
	if (result == NULL)
	    outofmem(__FILE__, "HTRelative");
	result[0] = '\0';
	for (; levels; levels--)
	    strcat(result, "../");
	strcat(result, last_slash+1);
    }
    CTRACE((tfp,
	    "HTparse: `%s' expressed relative to\n   `%s' is\n   `%s'.\n",
	    aName, relatedName, result));
    return result;
}

/*	Escape undesirable characters using %			HTEscape()
**	-------------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be unacceptable unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits.
**
**	Unlike HTUnEscape(), this routine returns a calloc'd string.
*/
PRIVATE CONST unsigned char isAcceptable[96] =

/*	Bit 0		xalpha		-- see HTFile.h
**	Bit 1		xpalpha		-- as xalpha but with plus.
**	Bit 2 ...	path		-- as xpalphas but with /
*/
    /*	 0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {	 0,0,0,0,0,0,0,0,0,0,7,6,0,7,7,4,	/* 2x	!"#$%&'()*+,-./  */
	 7,7,7,7,7,7,7,7,7,7,0,0,0,0,0,0,	/* 3x  0123456789:;<=>?  */
	 7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 4x  @ABCDEFGHIJKLMNO  */
	 7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,7,	/* 5X  PQRSTUVWXYZ[\]^_  */
	 0,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,	/* 6x  `abcdefghijklmno  */
	 7,7,7,7,7,7,7,7,7,7,7,0,0,0,0,0 };	/* 7X  pqrstuvwxyz{|}~	DEL */

PRIVATE char *hex = "0123456789ABCDEF";
#define ACCEPTABLE(a)	( a>=32 && a<128 && ((isAcceptable[a-32]) & mask))

PUBLIC char * HTEscape ARGS2(
	CONST char *,	str,
	unsigned char,	mask)
{
    CONST char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for (p = str; *p; p++)
	if (!ACCEPTABLE(UCH(TOASCII(*p))))
	    unacceptable++;
    result = typecallocn(char, p-str + unacceptable + unacceptable + 1);
    if (result == NULL)
	outofmem(__FILE__, "HTEscape");
    for (q = result, p = str; *p; p++) {
	unsigned char a = TOASCII(*p);
	if (!ACCEPTABLE(a)) {
	    *q++ = HEX_ESCAPE;	/* Means hex coming */
	    *q++ = hex[a >> 4];
	    *q++ = hex[a & 15];
	}
	else *q++ = *p;
    }
    *q++ = '\0';		/* Terminate */
    return result;
}

/*	Escape unsafe characters using %			HTEscapeUnsafe()
**	--------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be that may be unsafe are unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits.
**
**	Unlike HTUnEscape(), this routine returns a malloc'd string.
*/
#define UNSAFE(ch) (((ch) <= 32) || ((ch) >= 127))

PUBLIC char *HTEscapeUnsafe ARGS1(
	CONST char *,	str)
{
    CONST char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for (p = str; *p; p++)
	if (UNSAFE(UCH(TOASCII(*p))))
	    unacceptable++;
    result = typecallocn(char, p-str + unacceptable + unacceptable + 1);
    if (result == NULL)
	outofmem(__FILE__, "HTEscapeUnsafe");
    for (q = result, p = str; *p; p++) {
	unsigned char a = TOASCII(*p);
	if (UNSAFE(a)) {
	    *q++ = HEX_ESCAPE;	/* Means hex coming */
	    *q++ = hex[a >> 4];
	    *q++ = hex[a & 15];
	}
	else *q++ = *p;
    }
    *q++ = '\0';		/* Terminate */
    return result;
}

/*	Escape undesirable characters using % but space to +.	HTEscapeSP()
**	-----------------------------------------------------
**
**	This function takes a pointer to a string in which
**	some characters may be unacceptable unescaped.
**	It returns a string which has these characters
**	represented by a '%' character followed by two hex digits,
**	except that spaces are converted to '+' instead of %2B.
**
**	Unlike HTUnEscape(), this routine returns a calloced string.
*/
PUBLIC char * HTEscapeSP ARGS2(
	CONST char *,	str,
	unsigned char,	mask)
{
    CONST char * p;
    char * q;
    char * result;
    int unacceptable = 0;
    for (p = str; *p; p++)
	if (!(*p == ' ' || ACCEPTABLE(UCH(TOASCII(*p)))))
	    unacceptable++;
    result = typecallocn(char, p-str + unacceptable + unacceptable + 1);
    if (result == NULL)
	outofmem(__FILE__, "HTEscape");
    for (q = result, p = str; *p; p++) {
	unsigned char a = TOASCII(*p);
	if (a == 32) {
	    *q++ = '+';
	} else if (!ACCEPTABLE(a)) {
	    *q++ = HEX_ESCAPE;	/* Means hex coming */
	    *q++ = hex[a >> 4];
	    *q++ = hex[a & 15];
	} else {
	    *q++ = *p;
	}
    }
    *q++ = '\0';			/* Terminate */
    return result;
}

/*	Decode %xx escaped characters.				HTUnEscape()
**	------------------------------
**
**	This function takes a pointer to a string in which some
**	characters may have been encoded in %xy form, where xy is
**	the ASCII hex code for character 16x+y.
**	The string is converted in place, as it will never grow.
*/
PRIVATE char from_hex ARGS1(
	char,		c)
{
    return (char) ( c >= '0' && c <= '9' ?  c - '0'
	    : c >= 'A' && c <= 'F'? c - 'A' + 10
	    : c - 'a' + 10);     /* accept small letters just in case */
}

PUBLIC char * HTUnEscape ARGS1(
	char *,		str)
{
    char * p = str;
    char * q = str;

    if (!(p && *p))
	return str;

    while (*p != '\0') {
	if (*p == HEX_ESCAPE &&
	    /*
	     *	Tests shouldn't be needed, but better safe than sorry.
	     */
	    p[1] && p[2] &&
	    isxdigit(UCH(p[1])) &&
	    isxdigit(UCH(p[2]))) {
	    p++;
	    if (*p)
		*q = (char) (from_hex(*p++) * 16);
	    if (*p) {
		/*
		** Careful! FROMASCII() may evaluate its arg more than once!
		*/  /* S/390 -- gil -- 0221 */
		*q = (char) (*q + from_hex(*p++));
	    }
	    *q = FROMASCII(*q);
	    q++;
	} else {
	    *q++ = *p++;
	}
    }

    *q++ = '\0';
    return str;

} /* HTUnEscape */

/*	Decode some %xx escaped characters.		      HTUnEscapeSome()
**	-----------------------------------			Klaus Weide
**							    (kweide@tezcat.com)
**	This function takes a pointer to a string in which some
**	characters may have been encoded in %xy form, where xy is
**	the ASCII hex code for character 16x+y, and a pointer to
**	a second string containing one or more characters which
**	should be unescaped if escaped in the first string.
**	The first string is converted in place, as it will never grow.
*/
PUBLIC char * HTUnEscapeSome ARGS2(
	char *,		str,
	CONST char *,	do_trans)
{
    char * p = str;
    char * q = str;
    char testcode;

    if (p == NULL || *p == '\0' || do_trans == NULL || *do_trans == '\0')
	return str;

    while (*p != '\0') {
	if (*p == HEX_ESCAPE &&
	    p[1] && p[2] &&	/* tests shouldn't be needed, but.. */
	    isxdigit(UCH(p[1])) &&
	    isxdigit(UCH(p[2])) &&
	    (testcode = (char) FROMASCII(from_hex(p[1])*16 +
		from_hex(p[2]))) && /* %00 no good*/
	    strchr(do_trans, testcode)) { /* it's one of the ones we want */
	    *q++ = testcode;
	    p += 3;
	} else {
	    *q++ = *p++;
	}
    }

    *q++ = '\0';
    return str;

} /* HTUnEscapeSome */

PRIVATE CONST unsigned char crfc[96] =

/*	Bit 0		xalpha		-- need "quoting"
**	Bit 1		xpalpha		-- need \escape if quoted
*/
    /*	 0 1 2 3 4 5 6 7 8 9 A B C D E F */
    {	 1,0,3,0,0,0,0,0,1,1,0,0,1,0,1,0,	/* 2x	!"#$%&'()*+,-./  */
	 0,0,0,0,0,0,0,0,0,0,1,1,1,0,1,0,	/* 3x  0123456789:;<=>?  */
	 1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 4x  @ABCDEFGHIJKLMNO  */
	 0,0,0,0,0,0,0,0,0,0,0,1,2,1,0,0,	/* 5X  PQRSTUVWXYZ[\]^_  */
	 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,	/* 6x  `abcdefghijklmno  */
	 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3 };	/* 7X  pqrstuvwxyz{|}~	DEL */

/*
**  Turn a string which is not a RFC 822 token into a quoted-string. - KW
**  The "quoted" parameter tells whether we need the beginning/ending quote
**  marks.  If not, the caller will provide them -TD
*/
PUBLIC void HTMake822Word ARGS2(
	char **,	str,
	int,		quoted)
{
    CONST char * p;
    char * q;
    char * result;
    unsigned char a;
    int added = 0;

    if (isEmpty(*str)) {
	StrAllocCopy(*str, quoted ? "\"\"" : "");
	return;
    }
    for (p = *str; *p; p++) {
	a = TOASCII(*p);  /* S/390 -- gil -- 0240 */
	if (a < 32 || a >= 128 ||
	    ((crfc[a-32]) & 1)) {
	    if (!added)
		added = 2;
	    if (a >= 160 || a == '\t')
		continue;
	    if (a == '\r' || a == '\n')
		added += 2;
	    else if ((a & 127) < 32 || ((crfc[a-32]) & 2))
		added++;
	}
    }
    if (!added)
	return;
    result = typecallocn(char, p-(*str) + added + 1);
    if (result == NULL)
	outofmem(__FILE__, "HTMake822Word");

    q = result;
    if (quoted)
	*q++ = '"';
    /*
    ** Having converted the character to ASCII, we can't use symbolic
    ** escape codes, since they're in the host character set, which
    ** is not necessarily ASCII.  Thus we use octal escape codes instead.
    ** -- gil (Paul Gilmartin) <pg@sweng.stortek.com>
    */  /* S/390 -- gil -- 0268 */
    for (p = *str; *p; p++) {
	a = TOASCII(*p);
	if ((a != '\011') && ((a & 127) < 32 ||
			    ( a < 128 && ((crfc[a-32]) & 2))))
	    *q++ = '\033';
	*q++ = *p;
	if (a == '\012' || (a == '\015' && (TOASCII(*(p+1)) != '\012')))
	    *q++ = ' ';
    }
    if (quoted)
	*q++ = '"';
    *q++ = '\0';			/* Terminate */
    FREE(*str);
    *str = result;
}
