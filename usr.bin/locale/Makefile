#	$OpenBSD: Makefile,v 1.1 2013/06/03 14:27:50 stsp Exp $

# for LOCALECHARSETS
.include "${.CURDIR}/../../share/locale/ctype/Makefile.inc"

PROG=	locale
CFLAGS+=	-DLOCALE_CHARSETS="\"${LOCALECHARSETS}\""

.include <bsd.prog.mk>
