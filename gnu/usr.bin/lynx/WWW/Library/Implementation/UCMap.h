#ifndef UCMAP_H
#define UCMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

typedef long UCode_t;

extern int UCTransUniChar PARAMS((
	UCode_t		unicode,
	int		charset_out));
extern int UCTransUniCharStr PARAMS((
	char *		outbuf,
	int		buflen,
	UCode_t		unicode,
	int		charset_out,
	int		chk_single_flag));
extern int UCTransChar PARAMS((
	char		ch_in,
	int		charset_in,
	int		charset_out));
extern int UCReverseTransChar PARAMS((
	char		ch_out,
	int		charset_in,
	int		charset_out));
extern int UCTransCharStr PARAMS((
	char *		outbuf,
	int		buflen,
	char		ch_in,
	int		charset_in,
	int		charset_out,
	int		chk_single_flag));
extern UCode_t UCTransToUni PARAMS((
	char		ch_in,
	int		charset_in));
extern int UCGetRawUniMode_byLYhndl PARAMS((
	int		i));
extern int UCGetLYhndl_byMIME PARAMS((
	CONST char *	p)); /* returns -1 if name not recognized */
extern int safeUCGetLYhndl_byMIME PARAMS((
	CONST char *	p)); /* returns LATIN1 if name not recognized */

extern int UCLYhndl_for_unspec;
extern int UCLYhndl_for_unrec;
extern int UCLYhndl_HTFile_for_unspec;
extern int UCLYhndl_HTFile_for_unrec;

/* easy to type: */
extern int LATIN1;     /* UCGetLYhndl_byMIME("iso-8859-1") */
extern int US_ASCII;   /* UCGetLYhndl_byMIME("us-ascii")   */
extern int UTF8;       /* UCGetLYhndl_byMIME("utf-8")      */

#undef TRANSPARENT	/* defined on Solaris in <sys/stream.h> */
extern int TRANSPARENT; /* UCGetLYhndl_byMIME("x-transparent")  */

/*
In general, Lynx translates letters from document charset to display charset.
If document charset is not specified or not recognized by Lynx, we fall back
to different assumptions below, read also lynx.cfg for info.

UCLYhndl_for_unspec -  assume this as charset for documents that don't
                       specify a charset parameter in HTTP headers or via META
                       this corresponds to "assume_charset"

UCLYhndl_HTFile_for_unspec -  assume this as charset of local file
                       this corresponds to "assume_local_charset"

UCLYhndl_for_unrec  -  in case a charset parameter is not recognized;
                       this corresponds to "assume_unrec_charset"

UCLYhndl_HTFile_for_unrec  - the same but only for local files,
                             currently not used.


current_char_set  -	this corresponds to "display charset",
			declared in LYCharSets.c and really important.

All external charset information is available in so called MIME format.
For internal needs Lynx uses charset handlers as integers
from UCGetLYhndl_byMIME().  However, there is no way to recover
from user's error in configuration file lynx.cfg or command line switches,
those unrecognized MIME names are assumed as LATIN1 (via safeUCGetLYhndl...).
*/


#define UCTRANS_NOTFOUND (-4)

#endif /* UCMAP_H */
