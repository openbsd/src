#	$OpenBSD: Makefile,v 1.2 2000/04/02 19:48:36 millert Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF+=-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSMUTIL=1

.include "../../Makefile.inc"
.include <bsd.prog.mk>
