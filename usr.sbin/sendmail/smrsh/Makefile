#	$OpenBSD: Makefile,v 1.2 2000/02/16 16:51:09 form Exp $
#	@(#)Makefile	8.1 (Berkeley) 7/2/95

PROG=	smrsh
MAN=	smrsh.8
CFLAGS+=-I${.CURDIR}/../src -DCMDDIR=\"/usr/libexec/sm.bin\"

BINDIR=	/usr/libexec

.include "../../Makefile.inc"
.include <bsd.prog.mk>
