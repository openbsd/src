#	$OpenBSD: Makefile,v 1.2 1999/06/06 15:16:40 niklas Exp $

#
# You don't want to understand this makefile.
#

LIB	= arla
NOPIC	=
NOPROFILE=
MAN	=

CFLAGS	+= -DHAVE_CONFIG_H -I${.CURDIR}/../src/include -I. \
	   -I${.CURDIR}/../src/lib/roken -I${.CURDIR}/../src/util \
	   -I${.CURDIR}/../src -I${.CURDIR}/../libroken

# XXX Until an egtcs bug gets fixed
.if (${MACHINE_ARCH} == "alpha")
CFLAGS	+= -O0
.endif

.include "Makefile.lwp.inc"
.include "Makefile.rx.inc"
.include "Makefile.rxdef.inc"
.include "Makefile.rxkad.inc"
.include "Makefile.ko.inc"

.include <bsd.lib.mk>
