#	$OpenBSD: Makefile,v 1.4 2000/05/15 03:37:33 millert Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF=	-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSMUTIL=1

.include "../../../libexec/Makefile.inc"
.include <bsd.prog.mk>
