
#ifndef UCMAP_H
#define UCMAP_H

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
extern int UCGetLYhndl_byMIME PARAMS((
	CONST char *	p));
extern int UCGetRawUniMode_byLYhndl PARAMS((
	int		i));

extern int UCLYhndl_for_unspec;
extern int UCLYhndl_for_unrec;
extern int UCLYhndl_HTFile_for_unspec;
extern int UCLYhndl_HTFile_for_unrec;

#define UCTRANS_NOTFOUND (-4)

#define HT_CANNOT_TRANSLATE -4	/* could go into HTUtils.h */

#endif /* UCMAP_H */
