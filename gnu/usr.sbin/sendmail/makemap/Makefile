#	$OpenBSD: Makefile,v 1.6 2014/03/19 21:10:27 tedu Exp $

PROG=	makemap
NOMAN=1
ENVDEF=	-DNOT_SENDMAIL

WANT_LIBSM=1
WANT_LIBSMDB=1
WANT_LIBSMUTIL=1

.include "../../Makefile.inc"

BINDIR=/usr/libexec/sendmail

.include <bsd.prog.mk>
