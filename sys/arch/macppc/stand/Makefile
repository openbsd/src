#	$OpenBSD: Makefile,v 1.4 2007/11/25 18:25:32 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1996/09/30 16:34:59 ws Exp $

SUBDIR=	ofwboot boot.mac mbr tbxidata

SAREL=
KERNREL=
KERN_AS=library
Z_AS=library
SA_ZLIB=smth
USE_LOADFILE=yes

.include "Makefile.inc"
.include "$S/lib/libz/Makefile.inc"
.include "$S/lib/libsa/Makefile.inc"

all:	${ZLIB} ${SALIB} _SUBDIRUSE

libdep:
	@echo ${.OBJDIR}/${SALIB} ${ZLIB}


${PROG}:	

clean::	_SUBDIRUSE

cleandir:	_SUBDIRUSE

depend::	_SUBDIRUSE

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
