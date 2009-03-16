#	$OpenBSD: Makefile,v 1.5 2009/03/16 14:26:24 jacekm Exp $

PROG=	makemap
MAN=	makemap.8
ENVDEF=	-DNOT_SENDMAIL

WANT_LIBSM=1
WANT_LIBSMDB=1
WANT_LIBSMUTIL=1

.include "../../Makefile.inc"

BINDIR=/usr/libexec/sendmail

.include <bsd.prog.mk>
