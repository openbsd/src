#ifndef UCAUX_H
#define UCAUX_H

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifndef UCDEFS_H
#include <UCDefs.h>
#endif /* UCDEFS_H */

#ifndef HTSTREAM_H
#include <HTStream.h>
#endif /* HTSTREAM_H */

#ifndef UCMAP_H
#include <UCMap.h>
#endif /* UCMAP_H */

#ifdef __cplusplus
extern "C" {
#endif
    extern BOOL UCCanUniTranslateFrom(int from);
    extern BOOL UCCanTranslateUniTo(int to);
    extern BOOL UCCanTranslateFromTo(int from, int to);
    extern BOOL UCNeedNotTranslate(int from,
				   int to);

    struct _UCTransParams {
	BOOL transp;
	BOOL do_cjk;
	BOOL decode_utf8;
	BOOL output_utf8;
	BOOL use_raw_char_in;
	BOOL strip_raw_char_in;
	BOOL pass_160_173_raw;
	BOOL do_8bitraw;
	BOOL trans_to_uni;
	BOOL trans_C0_to_uni;
	BOOL repl_translated_C0;
	BOOL trans_from_uni;
    };
    typedef struct _UCTransParams UCTransParams;

    extern void UCSetTransParams(UCTransParams * pT, int cs_in,
				 const LYUCcharset *p_in,
				 int cs_out,
				 const LYUCcharset *p_out);

    extern void UCTransParams_clear(UCTransParams * pT);

    extern void UCSetBoxChars(int cset,
			      int *pvert_out,
			      int *phori_out,
			      int vert_in,
			      int hori_in);

    typedef void putc_func_t(HTStream *me,
			     char ch);

    extern BOOL UCPutUtf8_charstring(HTStream *target,
				     putc_func_t * actions,
				     UCode_t code);

    extern BOOL UCConvertUniToUtf8(UCode_t code,
				   char *buffer);

    extern UCode_t UCGetUniFromUtf8String(char **ppuni);

#ifdef __cplusplus
}
#endif
#endif				/* UCAUX_H */
