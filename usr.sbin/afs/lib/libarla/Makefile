#	$OpenBSD: Makefile,v 1.1 2002/06/07 05:02:58 hin Exp $

#
# You don't want to understand this makefile.
#

AFSTOPDIR	= ../..
.include <../../Makefile.inc>

LIB	= arla
NOPIC	=
NOPROFILE=
MAN	=

CFLAGS	+= -DHAVE_CONFIG_H -I. -I${AFSSRC}/util

# XXX Until an egcs bug gets fixed
.if (${MACHINE_ARCH} == "alpha")
CFLAGS	+= -O0
.endif

# Don't install libarla.a
install:

.include "Makefile.lwp.inc"
.include "Makefile.rx.inc"
.include "Makefile.rxdef.inc"
.include "Makefile.rxkad.inc"
.include "Makefile.ko.inc"
.include "Makefile.bufdir.inc"
.include "Makefile.util.inc"
.include "Makefile.sl.inc"
.include "Makefile.arla.inc"

.include <bsd.lib.mk>
