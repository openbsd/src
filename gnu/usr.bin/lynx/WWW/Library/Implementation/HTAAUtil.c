/* MODULE							HTAAUtil.c
 *		COMMON PARTS OF ACCESS AUTHORIZATION MODULE
 *			FOR BOTH SERVER AND BROWSER
 *
 * IMPORTANT:
 *	Routines in this module use dynamic allocation, but free
 *	automatically all the memory reserved by them.
 *
 *	Therefore the caller never has to (and never should)
 *	free() any object returned by these functions.
 *
 *	Therefore also all the strings returned by this package
 *	are only valid until the next call to the same function
 *	is made.  This approach is selected, because of the nature
 *	of access authorization: no string returned by the package
 *	needs to be valid longer than until the next call.
 *
 *	This also makes it easy to plug the AA package in:
 *	you don't have to ponder whether to free() something
 *	here or is it done somewhere else (because it is always
 *	done somewhere else).
 *
 *	The strings that the package needs to store are copied
 *	so the original strings given as parameters to AA
 *	functions may be freed or modified with no side effects.
 *
 *	The AA package does not free() anything else than what
 *	it has itself allocated.
 *
 *	AA (Access Authorization) package means modules which
 *	names start with HTAA.
 *
 * AUTHORS:
 *	AL	Ari Luotonen	luotonen@dxcern.cern.ch
 *	MD	Mark Donszelmann    duns@vxdeop.cern.ch
 *
 * HISTORY:
 *	 8 Nov 93  MD	(VMS only) Added case insensitive comparison in HTAA_templateCaseMatch
 *
 *
 * BUGS:
 *
 *
 */

#include <HTUtils.h>

#include <HTAAUtil.h>		/* Implemented here     */
#include <HTAssoc.h>		/* Assoc list           */
#include <HTTCP.h>
#include <HTTP.h>

#include <LYStrings.h>
#include <LYLeaks.h>

/* PUBLIC						HTAAScheme_enum()
 *		TRANSLATE SCHEME NAME INTO
 *		A SCHEME ENUMERATION
 *
 * ON ENTRY:
 *	name		is a string representing the scheme name.
 *
 * ON EXIT:
 *	returns		the enumerated constant for that scheme.
 */
HTAAScheme HTAAScheme_enum(const char *name)
{
    char *upcased = NULL;

    if (!name)
	return HTAA_UNKNOWN;

    StrAllocCopy(upcased, name);
    LYUpperCase(upcased);

    if (!strncmp(upcased, "NONE", 4)) {
	FREE(upcased);
	return HTAA_NONE;
    } else if (!strncmp(upcased, "BASIC", 5)) {
	FREE(upcased);
	return HTAA_BASIC;
    } else if (!strncmp(upcased, "PUBKEY", 6)) {
	FREE(upcased);
	return HTAA_PUBKEY;
    } else if (!strncmp(upcased, "KERBEROSV4", 10)) {
	FREE(upcased);
	return HTAA_KERBEROS_V4;
    } else if (!strncmp(upcased, "KERBEROSV5", 10)) {
	FREE(upcased);
	return HTAA_KERBEROS_V5;
    } else {
	FREE(upcased);
	return HTAA_UNKNOWN;
    }
}

/* PUBLIC						HTAAScheme_name()
 *			GET THE NAME OF A GIVEN SCHEME
 * ON ENTRY:
 *	scheme		is one of the scheme enum values:
 *			HTAA_NONE, HTAA_BASIC, HTAA_PUBKEY, ...
 *
 * ON EXIT:
 *	returns		the name of the scheme, i.e.
 *			"None", "Basic", "Pubkey", ...
 */
const char *HTAAScheme_name(HTAAScheme scheme)
{
    switch (scheme) {
    case HTAA_NONE:
	return "None";
    case HTAA_BASIC:
	return "Basic";
    case HTAA_PUBKEY:
	return "Pubkey";
    case HTAA_KERBEROS_V4:
	return "KerberosV4";
    case HTAA_KERBEROS_V5:
	return "KerberosV5";
    case HTAA_UNKNOWN:
	return "UNKNOWN";
    default:
	return "THIS-IS-A-BUG";
    }
}

/* PUBLIC						    HTAAMethod_enum()
 *		TRANSLATE METHOD NAME INTO AN ENUMERATED VALUE
 * ON ENTRY:
 *	name		is the method name to translate.
 *
 * ON EXIT:
 *	returns		HTAAMethod enumerated value corresponding
 *			to the given name.
 */
HTAAMethod HTAAMethod_enum(const char *name)
{
    if (!name)
	return METHOD_UNKNOWN;

    if (0 == strcasecomp(name, "GET"))
	return METHOD_GET;
    else if (0 == strcasecomp(name, "PUT"))
	return METHOD_PUT;
    else
	return METHOD_UNKNOWN;
}

/* PUBLIC						HTAAMethod_name()
 *			GET THE NAME OF A GIVEN METHOD
 * ON ENTRY:
 *	method		is one of the method enum values:
 *			METHOD_GET, METHOD_PUT, ...
 *
 * ON EXIT:
 *	returns		the name of the scheme, i.e.
 *			"GET", "PUT", ...
 */
const char *HTAAMethod_name(HTAAMethod method)
{
    switch (method) {
    case METHOD_GET:
	return "GET";
    case METHOD_PUT:
	return "PUT";
    case METHOD_UNKNOWN:
	return "UNKNOWN";
    default:
	return "THIS-IS-A-BUG";
    }
}

/* PUBLIC						HTAAMethod_inList()
 *		IS A METHOD IN A LIST OF METHOD NAMES
 * ON ENTRY:
 *	method		is the method to look for.
 *	list		is a list of method names.
 *
 * ON EXIT:
 *	returns		YES, if method was found.
 *			NO, if not found.
 */
BOOL HTAAMethod_inList(HTAAMethod method, HTList *list)
{
    HTList *cur = list;
    char *item;

    while (NULL != (item = (char *) HTList_nextObject(cur))) {
	CTRACE((tfp, " %s", item));
	if (method == HTAAMethod_enum(item))
	    return YES;
    }

    return NO;			/* Not found */
}

/* PUBLIC						HTAA_templateMatch()
 *		STRING COMPARISON FUNCTION FOR FILE NAMES
 *		   WITH ONE WILDCARD * IN THE TEMPLATE
 * NOTE:
 *	This is essentially the same code as in HTRules.c, but it
 *	cannot be used because it is embedded in between other code.
 *	(In fact, HTRules.c should use this routine, but then this
 *	 routine would have to be more sophisticated... why is life
 *	 sometimes so hard...)
 *
 * ON ENTRY:
 *	ctemplate	is a template string to match the file name
 *			against, may contain a single wildcard
 *			character * which matches zero or more
 *			arbitrary characters.
 *	filename	is the filename (or pathname) to be matched
 *			against the template.
 *
 * ON EXIT:
 *	returns		YES, if filename matches the template.
 *			NO, otherwise.
 */
BOOL HTAA_templateMatch(const char *ctemplate,
			const char *filename)
{
    const char *p = ctemplate;
    const char *q = filename;
    int m;

    for (; *p && *q && *p == *q; p++, q++)	/* Find first mismatch */
	;			/* do nothing else */

    if (!*p && !*q)
	return YES;		/* Equally long equal strings */
    else if ('*' == *p) {	/* Wildcard */
	p++;			/* Skip wildcard character */
	m = strlen(q) - strlen(p);	/* Amount to match to wildcard */
	if (m < 0)
	    return NO;		/* No match, filename too short */
	else {			/* Skip the matched characters and compare */
	    if (strcmp(p, q + m))
		return NO;	/* Tail mismatch */
	    else
		return YES;	/* Tail match */
	}
	/* if wildcard */
    } else
	return NO;		/* Length or character mismatch */
}

/* PUBLIC						HTAA_templateCaseMatch()
 *		STRING COMPARISON FUNCTION FOR FILE NAMES
 *		   WITH ONE WILDCARD * IN THE TEMPLATE (Case Insensitive)
 * NOTE:
 *	This is essentially the same code as in HTAA_templateMatch, but
 *	it compares case insensitive (for VMS). Reason for this routine
 *	is that HTAA_templateMatch gets called from several places, also
 *	there where a case sensitive match is needed, so one cannot just
 *	change the HTAA_templateMatch routine for VMS.
 *
 * ON ENTRY:
 *	template	is a template string to match the file name
 *			against, may contain a single wildcard
 *			character * which matches zero or more
 *			arbitrary characters.
 *	filename	is the filename (or pathname) to be matched
 *			against the template.
 *
 * ON EXIT:
 *	returns		YES, if filename matches the template.
 *			NO, otherwise.
 */
BOOL HTAA_templateCaseMatch(const char *ctemplate,
			    const char *filename)
{
    const char *p = ctemplate;
    const char *q = filename;
    int m;

    /* Find first mismatch */
    for (; *p && *q && TOUPPER(*p) == TOUPPER(*q); p++, q++) ;	/* do nothing else */

    if (!*p && !*q)
	return YES;		/* Equally long equal strings */
    else if ('*' == *p) {	/* Wildcard */
	p++;			/* Skip wildcard character */
	m = strlen(q) - strlen(p);	/* Amount to match to wildcard */
	if (m < 0)
	    return NO;		/* No match, filename too short */
	else {			/* Skip the matched characters and compare */
	    if (strcasecomp(p, q + m))
		return NO;	/* Tail mismatch */
	    else
		return YES;	/* Tail match */
	}
	/* if wildcard */
    } else
	return NO;		/* Length or character mismatch */
}

/* PUBLIC					HTAA_makeProtectionTemplate()
 *		CREATE A PROTECTION TEMPLATE FOR THE FILES
 *		IN THE SAME DIRECTORY AS THE GIVEN FILE
 *		(Used by server if there is no fancier way for
 *		it to tell the client, and by browser if server
 *		didn't send WWW-ProtectionTemplate: field)
 * ON ENTRY:
 *	docname is the document pathname (from URL).
 *
 * ON EXIT:
 *	returns a template matching docname, and other files
 *		files in that directory.
 *
 *		E.g.  /foo/bar/x.html  =>  /foo/bar/ *
 *						    ^
 *				Space only to prevent it from
 *				being a comment marker here,
 *				there really isn't any space.
 */
char *HTAA_makeProtectionTemplate(const char *docname)
{
    char *ctemplate = NULL;
    char *slash = NULL;

    if (docname) {
	StrAllocCopy(ctemplate, docname);
	slash = strrchr(ctemplate, '/');
	if (slash)
	    slash++;
	else
	    slash = ctemplate;
	*slash = '\0';
	StrAllocCat(ctemplate, "*");
    } else
	StrAllocCopy(ctemplate, "*");

    CTRACE((tfp, "make_template: made template `%s' for file `%s'\n",
	    ctemplate, docname));

    return ctemplate;
}

/*
 * Skip leading whitespace from *s forward
 */
#define SKIPWS(s) while (*s==' ' || *s=='\t') s++;

/*
 * Kill trailing whitespace starting from *(s-1) backwards
 */
#define KILLWS(s) {char *c=s-1; while (*c==' ' || *c=='\t') *(c--)='\0';}

/* PUBLIC						HTAA_parseArgList()
 *		PARSE AN ARGUMENT LIST GIVEN IN A HEADER FIELD
 * ON ENTRY:
 *	str	is a comma-separated list:
 *
 *			item, item, item
 *		where
 *			item ::= value
 *			       | name=value
 *			       | name="value"
 *
 *		Leading and trailing whitespace is ignored
 *		everywhere except inside quotes, so the following
 *		examples are equal:
 *
 *			name=value,foo=bar
 *			 name="value",foo="bar"
 *			  name = value ,  foo = bar
 *			   name = "value" ,  foo = "bar"
 *
 * ON EXIT:
 *	returns a list of name-value pairs (actually HTAssocList*).
 *		For items with no name, just value, the name is
 *		the number of order number of that item. E.g.
 *		"1" for the first, etc.
 */
HTAssocList *HTAA_parseArgList(char *str)
{
    HTAssocList *assoc_list = HTAssocList_new();
    char *cur = NULL;
    char *name = NULL;
    int n = 0;

    if (!str)
	return assoc_list;

    while (*str) {
	SKIPWS(str);		/* Skip leading whitespace */
	cur = str;
	n++;

	while (*cur && *cur != '=' && *cur != ',')
	    cur++;		/* Find end of name (or lonely value without a name) */
	KILLWS(cur);		/* Kill trailing whitespace */

	if (*cur == '=') {	/* Name followed by a value */
	    *(cur++) = '\0';	/* Terminate name */
	    StrAllocCopy(name, str);
	    SKIPWS(cur);	/* Skip WS leading the value */
	    str = cur;
	    if (*str == '"') {	/* Quoted value */
		str++;
		cur = str;
		while (*cur && *cur != '"')
		    cur++;
		if (*cur == '"')
		    *(cur++) = '\0';	/* Terminate value */
		/* else it is lacking terminating quote */
		SKIPWS(cur);	/* Skip WS leading comma */
		if (*cur == ',')
		    cur++;	/* Skip separating colon */
	    } else {		/* Unquoted value */
		while (*cur && *cur != ',')
		    cur++;
		KILLWS(cur);	/* Kill trailing whitespace */
		if (*cur == ',')
		    *(cur++) = '\0';
		/* else *cur already NULL */
	    }
	} else {		/* No name, just a value */
	    if (*cur == ',')
		*(cur++) = '\0';	/* Terminate value */
	    /* else last value on line (already terminated by NULL) */
	    HTSprintf0(&name, "%d", n);		/* Item order number for name */
	}
	HTAssocList_add(assoc_list, name, str);
	str = cur;
    }				/* while *str */

    FREE(name);
    return assoc_list;
}

/************** HEADER LINE READER -- DOES UNFOLDING *************************/

#define BUFFER_SIZE	1024

static size_t buffer_length;
static char *buffer = 0;
static char *start_pointer;
static char *end_pointer;
static int in_soc = -1;

#ifdef LY_FIND_LEAKS
static void FreeHTAAUtil(void)
{
    FREE(buffer);
}
#endif /* LY_FIND_LEAKS */

/* PUBLIC						HTAA_setupReader()
 *		SET UP HEADER LINE READER, i.e., give
 *		the already-read-but-not-yet-processed
 *		buffer of text to be read before more
 *		is read from the socket.
 * ON ENTRY:
 *	start_of_headers is a pointer to a buffer containing
 *			the beginning of the header lines
 *			(rest will be read from a socket).
 *	length		is the number of valid characters in
 *			'start_of_headers' buffer.
 *	soc		is the socket to use when start_of_headers
 *			buffer is used up.
 * ON EXIT:
 *	returns		nothing.
 *			Subsequent calls to HTAA_getUnfoldedLine()
 *			will use this buffer first and then
 *			proceed to read from socket.
 */
void HTAA_setupReader(char *start_of_headers,
		      int length,
		      int soc)
{
    if (!start_of_headers)
	length = 0;		/* initialize length (is this reached at all?) */
    if (buffer == NULL) {	/* first call? */
	buffer_length = length;
	if (buffer_length < BUFFER_SIZE)	/* would fall below BUFFER_SIZE? */
	    buffer_length = BUFFER_SIZE;
	buffer = (char *) malloc((size_t) (sizeof(char) * (buffer_length + 1)));
    } else if (length > (int) buffer_length) {	/* need more space? */
	buffer_length = length;
	buffer = (char *) realloc((char *) buffer,
				  (size_t) (sizeof(char) * (buffer_length + 1)));
    }
    if (buffer == NULL)
	outofmem(__FILE__, "HTAA_setupReader");
#ifdef LY_FIND_LEAKS
    atexit(FreeHTAAUtil);
#endif
    start_pointer = buffer;
    if (start_of_headers) {
	strncpy(buffer, start_of_headers, length);
	buffer[length] = '\0';
	end_pointer = buffer + length;
    } else {
	*start_pointer = '\0';
	end_pointer = start_pointer;
    }
    in_soc = soc;
}

/* PUBLIC						HTAA_getUnfoldedLine()
 *		READ AN UNFOLDED HEADER LINE FROM SOCKET
 * ON ENTRY:
 *	HTAA_setupReader must absolutely be called before
 *	this function to set up internal buffer.
 *
 * ON EXIT:
 *	returns a newly-allocated character string representing
 *		the read line.	The line is unfolded, i.e.
 *		lines that begin with whitespace are appended
 *		to current line.  E.g.
 *
 *			Field-Name: Blaa-Blaa
 *			 This-Is-A-Continuation-Line
 *			 Here-Is_Another
 *
 *		is seen by the caller as:
 *
 *	Field-Name: Blaa-Blaa This-Is-A-Continuation-Line Here-Is_Another
 *
 */
char *HTAA_getUnfoldedLine(void)
{
    char *line = NULL;
    char *cur;
    int count;
    BOOL peek_for_folding = NO;

    if (in_soc < 0) {
	CTRACE((tfp, "%s %s\n",
		"HTAA_getUnfoldedLine: buffer not initialized",
		"with function HTAA_setupReader()"));
	return NULL;
    }

    for (;;) {

	/* Reading from socket */

	if (start_pointer >= end_pointer) {	/*Read the next block and continue */
#ifdef USE_SSL
	    if (SSL_handle)
		count = SSL_read(SSL_handle, buffer, BUFFER_SIZE);
	    else
		count = NETREAD(in_soc, buffer, BUFFER_SIZE);
#else
	    count = NETREAD(in_soc, buffer, BUFFER_SIZE);
#endif /* USE_SSL */
	    if (count <= 0) {
		in_soc = -1;
		return line;
	    }
	    start_pointer = buffer;
	    end_pointer = buffer + count;
	    *end_pointer = '\0';
#ifdef NOT_ASCII
	    cur = start_pointer;
	    while (cur < end_pointer) {
		*cur = TOASCII(*cur);
		cur++;
	    }
#endif /*NOT_ASCII */
	}
	cur = start_pointer;

	/* Unfolding */

	if (peek_for_folding) {
	    if (*cur != ' ' && *cur != '\t')
		return line;	/* Ok, no continuation line */
	    else		/* So this is a continuation line, continue */
		peek_for_folding = NO;
	}

	/* Finding end-of-line */

	while (cur < end_pointer && *cur != '\n')	/* Find the end-of-line */
	    cur++;		/* (or end-of-buffer).  */

	/* Terminating line */

	if (cur < end_pointer) {	/* So *cur==LF, terminate line */
	    *cur = '\0';	/* Overwrite LF */
	    if (*(cur - 1) == '\r')
		*(cur - 1) = '\0';	/* Overwrite CR */
	    peek_for_folding = YES;	/* Check for a continuation line */
	}

	/* Copying the result */

	if (line)
	    StrAllocCat(line, start_pointer);	/* Append */
	else
	    StrAllocCopy(line, start_pointer);	/* A new line */

	start_pointer = cur + 1;	/* Skip the read line */

    }				/* forever */
}
