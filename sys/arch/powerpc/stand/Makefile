#	$OpenBSD: Makefile,v 1.3 1997/01/09 05:21:27 rahnds Exp $
#	$NetBSD: Makefile,v 1.1 1996/09/30 16:34:59 ws Exp $

SUBDIR=	boot

SAREL=
KERNREL=
KERN_AS=library

.include "Makefile.inc"
.include "$S/lib/libsa/Makefile.inc"
.include "$S/lib/libkern/Makefile.inc"

all:	${SALIB} ${KERNLIB} _SUBDIRUSE

libdep:
	@echo ${.OBJDIR}/${SALIB} $(KERNLIB)


${PROG}:	

clean::	_SUBDIRUSE

cleandir:	_SUBDIRUSE

depend::	_SUBDIRUSE

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
