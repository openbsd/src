#	$OpenBSD: Makefile,v 1.5 2001/09/09 16:41:33 deraadt Exp $

PROG=	smrsh
MAN=	smrsh.8
ENVDEF=	-DNOT_SENDMAIL -DCMDDIR=\"/usr/libexec/sm.bin\"

WANT_LIBSMUTIL=1

BINDIR?=        /usr/libexec
.include <bsd.prog.mk>
