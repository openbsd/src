#	$OpenBSD: Makefile,v 1.4 2001/05/31 15:56:13 todd Exp $

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

# XXX Until an egcs bug gets fixed
.if (${MACHINE_ARCH} == "alpha")
CFLAGS	+= -O0
.endif

.include "Makefile.lwp.inc"
.include "Makefile.rx.inc"
.include "Makefile.rxdef.inc"
.include "Makefile.rxkad.inc"
.include "Makefile.ko.inc"
.include "Makefile.bufdir.inc"

.include <bsd.lib.mk>
