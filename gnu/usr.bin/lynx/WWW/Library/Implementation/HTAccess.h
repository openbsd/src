/*                                                      HTAccess:  Access manager  for libwww
                                      ACCESS MANAGER

   This module keeps a list of valid protocol (naming scheme) specifiers with associated
   access code.  It allows documents to be loaded given various combinations of
   parameters.  New access protocols may be registered at any time.

   Part of the libwww library .

 */
#ifndef HTACCESS_H
#define HTACCESS_H

extern char * use_this_url_instead;

/*      Definition uses:
*/
#include <HTAnchor.h>
#include <HTFormat.h>

/*      Return codes from load routines:
**
**      These codes may be returned by the protocol modules,
**      and by the HTLoad routines.
**      In general, positive codes are OK and negative ones are bad.
*/


/*

Default Addresses

   These control the home page selection.  To mess with these for normal browses is asking
   for user confusion.

 */
#define LOGICAL_DEFAULT "WWW_HOME"  /* Defined to be the home page */

#ifndef PERSONAL_DEFAULT
#define PERSONAL_DEFAULT "WWW/default.html"     /* in home directory */
#endif
#ifndef LOCAL_DEFAULT_FILE
#define LOCAL_DEFAULT_FILE "/usr/local/lib/WWW/default.html"
#endif
/*  If one telnets to a www access point,
    it will look in this file for home page */
#ifndef REMOTE_POINTER
#define REMOTE_POINTER  "/etc/www-remote.url"  /* can't be file */
#endif
/* and if that fails it will use this. */
#ifndef REMOTE_ADDRESS
#define REMOTE_ADDRESS  "http://www.w3.org/remote.html"  /* can't be file */
#endif

/* If run from telnet daemon and no -l specified, use this file:
*/
#ifndef DEFAULT_LOGFILE
#define DEFAULT_LOGFILE "/usr/adm/www-log/www-log"
#endif

/*      If the home page isn't found, use this file:
*/
#ifndef LAST_RESORT
#define LAST_RESORT     "http://www.w3.org/default.html"
#endif


/*

Flags which may be set to control this module

 */
#ifdef NOT
extern int HTDiag;                      /* Flag: load source as plain text */
#endif /* NOT */
extern char * HTClientHost;             /* Name or number of telnetting host */
extern FILE * HTlogfile;                /* File to output one-liners to */
extern BOOL HTSecure;                   /* Disable security holes? */
extern HTStream* HTOutputStream;        /* For non-interactive, set this */
extern HTFormat HTOutputFormat;         /* To convert on load, set this */

/*	Check for proxy override.			override_proxy()
**
**	Check the no_proxy environment variable to get the list
**	of hosts for which proxy server is not consulted.
**
**	no_proxy is a comma- or space-separated list of machine
**	or domain names, with optional :port part.  If no :port
**	part is present, it applies to all ports on that domain.
**
**  Example:
**          no_proxy="cern.ch,some.domain:8001"
**
**  Use "*" to override all proxy service:
**	     no_proxy="*"
*/
extern BOOL override_proxy PARAMS((
	CONST char *	addr));

/*

Load a document from relative name

  ON ENTRY,

  relative_name           The relative address of the file to be accessed.

  here                    The anchor of the object being searched

  ON EXIT,

  returns    YES          Success in opening file

  NO                      Failure

 */
extern  BOOL HTLoadRelative PARAMS((
                CONST char *            relative_name,
                HTParentAnchor *        here));


/*

Load a document from absolute name

  ON ENTRY,

  addr                    The absolute address of the document to be accessed.

  filter_it               if YES, treat document as HTML

 */

/*

  ON EXIT,

 */

/*

  returns YES             Success in opening document

  NO                      Failure

 */
extern BOOL HTLoadAbsolute PARAMS((CONST DocAddress * addr));


/*

Load a document from absolute name to a stream

  ON ENTRY,

  addr                    The absolute address of the document to be accessed.

  filter_it               if YES, treat document as HTML

  ON EXIT,

  returns YES             Success in opening document

  NO                      Failure

   Note: This is equivalent to HTLoadDocument

 */
extern BOOL HTLoadToStream PARAMS((CONST char * addr, BOOL filter_it,
                                HTStream * sink));


/*

Load if necessary, and select an anchor

  ON ENTRY,

  destination                The child or parenet anchor to be loaded.

 */

/*

  ON EXIT,

 */

/*

  returns YES             Success

  returns NO              Failure

 */



extern BOOL HTLoadAnchor PARAMS((HTAnchor * destination));


/*

Make a stream for Saving object back

  ON ENTRY,

  anchor                  is valid anchor which has previously beeing loaded

  ON EXIT,

  returns                 0 if error else a stream to save the object to.

 */


extern HTStream * HTSaveStream PARAMS((HTParentAnchor * anchor));


/*

Search

   Performs a search on word given by the user.  Adds the search words to the end of the
   current address and attempts to open the new address.

  ON ENTRY,

  *keywords               space-separated keyword list or similar search list

  here                    The anchor of the object being searched

 */
extern BOOL HTSearch PARAMS((CONST char * keywords, HTParentAnchor* here));


/*

Search Given Indexname

   Performs a keyword search on word given by the user.  Adds the keyword to  the end of
   the current address and attempts to open the new address.

  ON ENTRY,

  *keywords               space-separated keyword list or similar search list

  *indexname              is name of object search is to be done on.

 */
extern BOOL HTSearchAbsolute PARAMS((
        CONST char *    keywords,
        char *    	indexname));


/*

Register an access method

 */

typedef struct _HTProtocol {
        char * name;

        int (*load)PARAMS((
                CONST char *    full_address,
                HTParentAnchor * anchor,
                HTFormat        format_out,
                HTStream*       sink));

        HTStream* (*saveStream)PARAMS((HTParentAnchor * anchor));

} HTProtocol;

extern BOOL HTRegisterProtocol PARAMS((HTProtocol * protocol));


/*

Generate the anchor for the home page

 */

/*

   As it involves file access, this should only be done once when the program first runs.
   This is a default algorithm -- browser don't HAVE to use this.

 */
extern HTParentAnchor * HTHomeAnchor NOPARAMS;

/*

Return Host Name

 */
extern CONST char * HTHostName NOPARAMS;

/*

For registering protocols supported by Lynx

*/
extern void LYRegisterLynxProtocols NOARGS;

extern void LYUCPushAssumed PARAMS((
    HTParentAnchor *	anchor));
extern int LYUCPopAssumed NOPARAMS;

#endif /* HTACCESS_H */
/*

   end of HTAccess  */
