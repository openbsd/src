/*

Copyright 1997-1999,2001 Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.


The tables and some of the code that used to be here was borrowed from
metamail, which comes with this message:

  Copyright (c) 1991 Bell Communications Research, Inc. (Bellcore)

  Permission to use, copy, modify, and distribute this material
  for any purpose and without fee is hereby granted, provided
  that the above copyright notice and this permission notice
  appear in all copies, and that the name of Bellcore not be
  used in advertising or publicity pertaining to this
  material without the specific, prior written permission
  of an authorized representative of Bellcore.	BELLCORE
  MAKES NO REPRESENTATIONS ABOUT THE ACCURACY OR SUITABILITY
  OF THIS MATERIAL FOR ANY PURPOSE.  IT IS PROVIDED "AS IS",
  WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES.

*/


#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#ifdef __cplusplus
}
#endif


#define MAX_LINE  76 /* size of encoded lines */

static char basis_64[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define XX      255	/* illegal base64 char */
#define EQ      254	/* padding */
#define INVALID XX

static unsigned char index_64[256] = {
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,62, XX,XX,XX,63,
    52,53,54,55, 56,57,58,59, 60,61,XX,XX, XX,EQ,XX,XX,
    XX, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
    15,16,17,18, 19,20,21,22, 23,24,25,XX, XX,XX,XX,XX,
    XX,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
    41,42,43,44, 45,46,47,48, 49,50,51,XX, XX,XX,XX,XX,

    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
    XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX, XX,XX,XX,XX,
};



MODULE = MIME::Base64		PACKAGE = MIME::Base64

SV*
encode_base64(sv,...)
	SV* sv
	PROTOTYPE: $;$

	PREINIT:
	char *str;     /* string to encode */
	SSize_t len;   /* length of the string */
	char *eol;     /* the end-of-line sequence to use */
	STRLEN eollen; /* length of the EOL sequence */
	char *r;       /* result string */
	STRLEN rlen;   /* length of result string */
	unsigned char c1, c2, c3;
	int chunk;

	CODE:
	sv_utf8_downgrade(sv, FALSE);
	str = SvPV(sv, rlen); /* SvPV(sv, len) gives warning for signed len */
	len = (SSize_t)rlen;

	/* set up EOL from the second argument if present, default to "\n" */
	if (items > 1 && SvOK(ST(1))) {
	    eol = SvPV(ST(1), eollen);
	} else {
	    eol = "\n";
	    eollen = 1;
	}

	/* calculate the length of the result */
	rlen = (len+2) / 3 * 4;	 /* encoded bytes */
	if (rlen) {
	    /* add space for EOL */
	    rlen += ((rlen-1) / MAX_LINE + 1) * eollen;
	}

	/* allocate a result buffer */
	RETVAL = newSV(rlen ? rlen : 1);
	SvPOK_on(RETVAL);	
	SvCUR_set(RETVAL, rlen);
	r = SvPVX(RETVAL);

	/* encode */
	for (chunk=0; len > 0; len -= 3, chunk++) {
	    if (chunk == (MAX_LINE/4)) {
		char *c = eol;
		char *e = eol + eollen;
		while (c < e)
		    *r++ = *c++;
		chunk = 0;
	    }
	    c1 = *str++;
	    c2 = *str++;
	    *r++ = basis_64[c1>>2];
	    *r++ = basis_64[((c1 & 0x3)<< 4) | ((c2 & 0xF0) >> 4)];
	    if (len > 2) {
		c3 = *str++;
		*r++ = basis_64[((c2 & 0xF) << 2) | ((c3 & 0xC0) >>6)];
		*r++ = basis_64[c3 & 0x3F];
	    } else if (len == 2) {
		*r++ = basis_64[(c2 & 0xF) << 2];
		*r++ = '=';
	    } else { /* len == 1 */
		*r++ = '=';
		*r++ = '=';
	    }
	}
	if (rlen) {
	    /* append eol to the result string */
	    char *c = eol;
	    char *e = eol + eollen;
	    while (c < e)
		*r++ = *c++;
	}
	*r = '\0';  /* every SV in perl should be NUL-terminated */

	OUTPUT:
	RETVAL

SV*
decode_base64(sv)
	SV* sv
	PROTOTYPE: $

	PREINIT:
	STRLEN len;
	register unsigned char *str = (unsigned char*)SvPVbyte(sv, len);
	unsigned char const* end = str + len;
	char *r;
	unsigned char c[4];

	CODE:
	{
	    /* always enough, but might be too much */
	    STRLEN rlen = len * 3 / 4;
	    RETVAL = newSV(rlen ? rlen : 1);
	}
        SvPOK_on(RETVAL);
        r = SvPVX(RETVAL);

	while (str < end) {
	    int i = 0;
            do {
		unsigned char uc = index_64[NATIVE_TO_ASCII(*str++)];
		if (uc != INVALID)
		    c[i++] = uc;

		if (str == end) {
		    if (i < 4) {
			if (i && PL_dowarn)
			    warn("Premature end of base64 data");
			if (i < 2) goto thats_it;
			if (i == 2) c[2] = EQ;
			c[3] = EQ;
		    }
		    break;
		}
            } while (i < 4);
	
	    if (c[0] == EQ || c[1] == EQ) {
		if (PL_dowarn) warn("Premature padding of base64 data");
		break;
            }
	    /* printf("c0=%d,c1=%d,c2=%d,c3=%d\n", c[0],c[1],c[2],c[3]);*/

	    *r++ = (c[0] << 2) | ((c[1] & 0x30) >> 4);

	    if (c[2] == EQ)
		break;
	    *r++ = ((c[1] & 0x0F) << 4) | ((c[2] & 0x3C) >> 2);

	    if (c[3] == EQ)
		break;
	    *r++ = ((c[2] & 0x03) << 6) | c[3];
	}

      thats_it:
	SvCUR_set(RETVAL, r - SvPVX(RETVAL));
	*r = '\0';

	OUTPUT:
	RETVAL
