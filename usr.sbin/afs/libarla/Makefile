#	$OpenBSD: Makefile,v 1.1.1.1 1998/09/14 21:53:28 art Exp $

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

.include "Makefile.lwp.inc"
.include "Makefile.rx.inc"
.include "Makefile.rxdef.inc"
.include "Makefile.rxkad.inc"
.include "Makefile.ko.inc"

.include <bsd.lib.mk>
