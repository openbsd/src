#	$OpenBSD: Makefile,v 1.6 2001/09/11 19:02:50 millert Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF=	-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSM=1

.include "../../../../libexec/Makefile.inc"
.include <bsd.prog.mk>
