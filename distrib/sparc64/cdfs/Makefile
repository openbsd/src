#	$OpenBSD: Makefile,v 1.6 2003/03/04 18:08:21 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	-@mkdir ${.OBJDIR}/cd-dir
	mkhybrid -A "OpenBSD ${OSREV} sparc64 bootonly CD" \
	    -P "Copyright (c) 2003 Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD${OSREV} CD boot-only CD " \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	disklabel -w svnd0 fakecdrom "OpenBSD${OSREV} sparc64"
	vnconfig -u svnd0
	mksuncd f ${.OBJDIR}/${CDROM} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

install:
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDROM}

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
