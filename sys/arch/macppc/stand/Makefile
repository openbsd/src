#	$OpenBSD: Makefile,v 1.8 2014/07/13 21:11:40 deraadt Exp $
#	$NetBSD: Makefile,v 1.1 1996/09/30 16:34:59 ws Exp $

MANSUBDIR=macppc
MAN=boot.8
MLINKS?=	boot.8 boot.conf.5

.if ${MACHINE} == "macppc"
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
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
.include <bsd.man.mk>
