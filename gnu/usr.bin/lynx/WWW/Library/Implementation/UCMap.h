/*
 * $LynxId: UCMap.h,v 1.23 2009/01/01 02:03:25 tom Exp $
 */
#ifndef UCMAP_H
#define UCMAP_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
    typedef enum {
	ucError = -1,
	ucZeroWidth = -2,
	ucInvalidHash = -3,
	ucNotFound = -4,
	ucNeedMore = -10,
	ucCannotConvert = -11,
	ucCannotOutput = -12,
	ucBufferTooSmall = -13,
	ucUnknown = -14
    } UCStatus;

    typedef long UCode_t;

    extern int UCTransUniChar(UCode_t unicode,
			      int charset_out);
    extern int UCTransUniCharStr(char *outbuf,
				 int buflen,
				 UCode_t unicode,
				 int charset_out,
				 int chk_single_flag);
    extern int UCTransChar(char ch_in,
			   int charset_in,
			   int charset_out);
    extern int UCReverseTransChar(char ch_out,
				  int charset_in,
				  int charset_out);
    extern int UCTransCharStr(char *outbuf,
			      int buflen,
			      char ch_in,
			      int charset_in,
			      int charset_out,
			      int chk_single_flag);
#ifdef EXP_JAPANESEUTF8_SUPPORT
    extern UCode_t UCTransJPToUni(char *inbuf,
				  int buflen,
				  int charset_in);
#endif
    extern UCode_t UCTransToUni(char ch_in,
				int charset_in);
    extern int UCGetRawUniMode_byLYhndl(int i);
    extern int UCGetLYhndl_byMIME(const char *p);	/* returns -1 if name not recognized */
    extern int safeUCGetLYhndl_byMIME(const char *p);	/* returns LATIN1 if name not recognized */

#ifdef USE_LOCALE_CHARSET
    extern void LYFindLocaleCharset(void);
#endif

    extern int UCLYhndl_for_unspec;
    extern int UCLYhndl_for_unrec;
    extern int UCLYhndl_HTFile_for_unspec;
    extern int UCLYhndl_HTFile_for_unrec;

/* easy to type: */
    extern int LATIN1;		/* UCGetLYhndl_byMIME("iso-8859-1") */
    extern int US_ASCII;	/* UCGetLYhndl_byMIME("us-ascii")   */
    extern int UTF8_handle;	/* UCGetLYhndl_byMIME("utf-8")      */

#undef TRANSPARENT		/* defined on Solaris in <sys/stream.h> */
    extern int TRANSPARENT;	/* UCGetLYhndl_byMIME("x-transparent")  */

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

#ifdef __cplusplus
}
#endif
#endif				/* UCMAP_H */
