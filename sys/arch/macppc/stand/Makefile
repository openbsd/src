#	$OpenBSD: Makefile,v 1.1 2001/09/01 15:39:02 drahn Exp $
#	$NetBSD: Makefile,v 1.1 1996/09/30 16:34:59 ws Exp $

SUBDIR=	ofwboot boot.mac mbr

SAREL=
KERNREL=
KERN_AS=library
Z_AS=library
SA_ZLIB=smth
USE_LOADFILE=yes

.include "Makefile.inc"
.include "$S/lib/libz/Makefile.inc"
.include "$S/lib/libsa/Makefile.inc"
.include "$S/lib/libkern/Makefile.inc"

all:	${ZLIB} ${SALIB} ${KERNLIB} _SUBDIRUSE

libdep:
	@echo ${.OBJDIR}/${SALIB} $(KERNLIB) ${ZLIB}


${PROG}:	

clean::	_SUBDIRUSE

cleandir:	_SUBDIRUSE

depend::	_SUBDIRUSE

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
