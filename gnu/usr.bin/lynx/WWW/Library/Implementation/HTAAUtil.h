/*                                            Utilities for the Authorization parts of libwww
             COMMON PARTS OF AUTHORIZATION MODULE TO BOTH SERVER AND BROWSER
                                             
   This module is the interface to the common parts of Access Authorization (AA) package
   for both server and browser. Important to know about memory allocation:
   
   Routines in this module use dynamic allocation, but free automatically all the memory
   reserved by them.
   
   Therefore the caller never has to (and never should) free() any object returned by
   these functions.
   
   Therefore also all the strings returned by this package are only valid until the next
   call to the same function is made. This approach is selected, because of the nature of
   access authorization: no string returned by the package needs to be valid longer than
   until the next call.
   
   This also makes it easy to plug the AA package in: you don't have to ponder whether to
   free() something here or is it done somewhere else (because it is always done somewhere
   else).
   
   The strings that the package needs to store are copied so the original strings given as
   parameters to AA functions may be freed or modified with no side effects.
   
   Also note: The AA package does not free() anything else than what it has itself
   allocated.
   
 */

#ifndef HTAAUTIL_H
#define HTAAUTIL_H

#ifndef HTUTILS_H
#include "HTUtils.h"            /* BOOL, PARAMS, ARGS */
#endif /* HTUTILS_H */
#include "tcp.h"
#include "HTList.h"

#ifdef SHORT_NAMES
#define HTAASenu        HTAAScheme_enum
#define HTAASnam        HTAAScheme_name
#define HTAAMenu        HTAAMethod_enum
#define HTAAMnam        HTAAMethod_name
#define HTAAMinL        HTAAMethod_inList
#define HTAAteMa        HTAA_templateMatch
#define HTAAmaPT        HTAA_makeProtectionTemplate
#define HTAApALi        HTAA_parseArgList
#define HTAAsuRe        HTAA_setupReader
#define HTAAgUfL        HTAA_getUnfoldedLine
#endif /*SHORT_NAMES*/


/*

Default filenames

 */
#ifndef PASSWD_FILE
#define PASSWD_FILE     "/home2/luotonen/passwd"
#endif

#ifndef GROUP_FILE
#define GROUP_FILE      "/home2/luotonen/group"
#endif

#define ACL_FILE_NAME   ".www_acl"


/*
** Numeric constants
*/
#define MAX_USERNAME_LEN        16      /* @@ Longest allowed username    */
#define MAX_PASSWORD_LEN        3*13    /* @@ Longest allowed password    */
                                        /* (encrypted, so really only 3*8)*/
#define MAX_METHODNAME_LEN      12      /* @@ Longest allowed method name */
#define MAX_FIELDNAME_LEN       16      /* @@ Longest field name in       */
                                        /* protection setup file          */
#define MAX_PATHNAME_LEN        80      /* @@ Longest passwd/group file   */
                                        /* patname to allow               */

/*
** Helpful macros
*/
#define FREE(x) if (x) {free(x); x = NULL;}

/*

Datatype definitions

  HTAASCHEME
  
   The enumeration HTAAScheme represents the possible authentication schemes used by the
   WWW Access Authorization.
   
 */

typedef enum {
    HTAA_UNKNOWN,
    HTAA_NONE,
    HTAA_BASIC,
    HTAA_PUBKEY,
    HTAA_KERBEROS_V4,
    HTAA_KERBEROS_V5,
    HTAA_MAX_SCHEMES /* THIS MUST ALWAYS BE LAST! Number of schemes */
} HTAAScheme;

/*

  ENUMERATION TO REPRESENT HTTP METHODS
  
 */

typedef enum {
    METHOD_UNKNOWN,
    METHOD_GET,
    METHOD_PUT
} HTAAMethod;

/*

Authentication Schemes

 */

/* PUBLIC                                               HTAAScheme_enum()
**              TRANSLATE SCHEME NAME TO A SCHEME ENUMERATION
** ON ENTRY:
**      name            is a string representing the scheme name.
**
** ON EXIT:
**      returns         the enumerated constant for that scheme.
*/
PUBLIC HTAAScheme HTAAScheme_enum PARAMS((CONST char* name));


/* PUBLIC                                               HTAAScheme_name()
**                      GET THE NAME OF A GIVEN SCHEME
** ON ENTRY:
**      scheme          is one of the scheme enum values:
**                      HTAA_NONE, HTAA_BASIC, HTAA_PUBKEY, ...
**
** ON EXIT:
**      returns         the name of the scheme, i.e.
**                      "none", "basic", "pubkey", ...
*/
PUBLIC char *HTAAScheme_name PARAMS((HTAAScheme scheme));

/*

Methods

 */

/* PUBLIC                                                   HTAAMethod_enum()
**              TRANSLATE METHOD NAME INTO AN ENUMERATED VALUE
** ON ENTRY:
**      name            is the method name to translate.
**
** ON EXIT:
**      returns         HTAAMethod enumerated value corresponding
**                      to the given name.
*/
PUBLIC HTAAMethod HTAAMethod_enum PARAMS((CONST char * name));


/* PUBLIC                                               HTAAMethod_name()
**                      GET THE NAME OF A GIVEN METHOD
** ON ENTRY:
**      method          is one of the method enum values:
**                      METHOD_GET, METHOD_PUT, ...
**
** ON EXIT:
**      returns         the name of the scheme, i.e.
**                      "GET", "PUT", ...
*/
PUBLIC char *HTAAMethod_name PARAMS((HTAAMethod method));


/* PUBLIC                                               HTAAMethod_inList()
**              IS A METHOD IN A LIST OF METHOD NAMES
** ON ENTRY:
**      method          is the method to look for.
**      list            is a list of method names.
**
** ON EXIT:
**      returns         YES, if method was found.
**                      NO, if not found.
*/
PUBLIC BOOL HTAAMethod_inList PARAMS((HTAAMethod        method,
                                     HTList *           list));
/*

Match Template Against Filename

 */

/* PUBLIC                                               HTAA_templateMatch()
**              STRING COMPARISON FUNCTION FOR FILE NAMES
**                 WITH ONE WILDCARD * IN THE TEMPLATE
** NOTE:
**      This is essentially the same code as in HTRules.c, but it
**      cannot be used because it is embedded in between other code.
**      (In fact, HTRules.c should use this routine, but then this
**       routine would have to be more sophisticated... why is life
**       sometimes so hard...)
**
** ON ENTRY:
**      template        is a template string to match the file name
**                      agaist, may contain a single wildcard
**                      character * which matches zero or more
**                      arbitrary characters.
**      filename        is the filename (or pathname) to be matched
**                      agaist the template.
**
** ON EXIT:
**      returns         YES, if filename matches the template.
**                      NO, otherwise.
*/
PUBLIC BOOL HTAA_templateMatch PARAMS((CONST char * template,
                                       CONST char * filename));


/* PUBLIC                                               HTAA_templateCaseMatch()
**              STRING COMPARISON FUNCTION FOR FILE NAMES
**                 WITH ONE WILDCARD * IN THE TEMPLATE (Case Insensitive)
** NOTE:
**      This is essentially the same code as in HTAA_templateMatch, but
**      it compares case insensitive (for VMS). Reason for this routine
**      is that HTAA_templateMatch gets called from several places, also
**      there where a case sensitive match is needed, so one cannot just
**      change the HTAA_templateMatch routine for VMS.
**
** ON ENTRY:
**      template        is a template string to match the file name
**                      agaist, may contain a single wildcard
**                      character * which matches zero or more
**                      arbitrary characters.
**      filename        is the filename (or pathname) to be matched
**                      agaist the template.
**
** ON EXIT:
**      returns         YES, if filename matches the template.
**                      NO, otherwise.
*/
PUBLIC BOOL HTAA_templateCaseMatch PARAMS((CONST char * template,
                                         CONST char * filename));


/* PUBLIC                                       HTAA_makeProtectionTemplate()
**              CREATE A PROTECTION TEMPLATE FOR THE FILES
**              IN THE SAME DIRECTORY AS THE GIVEN FILE
**              (Used by server if there is no fancier way for
**              it to tell the client, and by browser if server
**              didn't send WWW-ProtectionTemplate: field)
** ON ENTRY:
**      docname is the document pathname (from URL).
**
** ON EXIT:
**      returns a template matching docname, and other files
**              files in that directory.
**
**              E.g.  /foo/bar/x.html  =>  /foo/bar/ *
**                                                  ^
**                              Space only to prevent it from
**                              being a comment marker here,
**                              there really isn't any space.
*/
PUBLIC char *HTAA_makeProtectionTemplate PARAMS((CONST char * docname));
/*

MIME Argument List Parser

 */


/* PUBLIC                                               HTAA_parseArgList()
**              PARSE AN ARGUMENT LIST GIVEN IN A HEADER FIELD
** ON ENTRY:
**      str     is a comma-separated list:
**
**                      item, item, item
**              where
**                      item ::= value
**                             | name=value
**                             | name="value"
**
**              Leading and trailing whitespace is ignored
**              everywhere except inside quotes, so the following
**              examples are equal:
**
**                      name=value,foo=bar
**                       name="value",foo="bar"
**                        name = value ,  foo = bar
**                         name = "value" ,  foo = "bar"
**
** ON EXIT:
**      returns a list of name-value pairs (actually HTAssocList*).
**              For items with no name, just value, the name is
**              the number of order number of that item. E.g.
**              "1" for the first, etc.
*/
PUBLIC HTList *HTAA_parseArgList PARAMS((char * str));

/*

Header Line Reader

 */

/* PUBLIC                                               HTAA_setupReader()
**              SET UP HEADER LINE READER, i.e. give
**              the already-read-but-not-yet-processed
**              buffer of text to be read before more
**              is read from the socket.
** ON ENTRY:
**      start_of_headers is a pointer to a buffer containing
**                      the beginning of the header lines
**                      (rest will be read from a socket).
**      length          is the number of valid characters in
**                      'start_of_headers' buffer.
**      soc             is the socket to use when start_of_headers
**                      buffer is used up.
** ON EXIT:
**      returns         nothing.
**                      Subsequent calls to HTAA_getUnfoldedLine()
**                      will use this buffer first and then
**                      proceed to read from socket.
*/
PUBLIC void HTAA_setupReader PARAMS((char *     start_of_headers,
                                     int        length,
				     void *	handle,
                                     int        soc));


/* PUBLIC                                               HTAA_getUnfoldedLine()
**              READ AN UNFOLDED HEADER LINE FROM SOCKET
** ON ENTRY:
**      HTAA_setupReader must absolutely be called before
**      this function to set up internal buffer.
**
** ON EXIT:
**      returns a newly-allocated character string representing
**              the read line.  The line is unfolded, i.e.
**              lines that begin with whitespace are appended
**              to current line.  E.g.
**
**                      Field-Name: Blaa-Blaa
**                       This-Is-A-Continuation-Line
**                       Here-Is_Another
**
**              is seen by the caller as:
**
**      Field-Name: Blaa-Blaa This-Is-A-Continuation-Line Here-Is_Another
**
*/
PUBLIC char *HTAA_getUnfoldedLine NOPARAMS;

#endif  /* NOT HTAAUTIL_H */
/*

   End of file HTAAUtil.h. */
