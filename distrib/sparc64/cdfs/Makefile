#	$OpenBSD: Makefile,v 1.9 2004/03/20 06:58:58 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	-mkdir -p ${.OBJDIR}/cd-dir/3.5/sparc64
	-cp ${.OBJDIR}/../bsd.rd/bsd.rd ${.OBJDIR}/cd-dir/3.5/sparc64
	ln ${.OBJDIR}/cd-dir/3.5/sparc64/bsd.rd ${.OBJDIR}/cd-dir/3.5/sparc64/bsd
	-cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/3.5/sparc64
	mkhybrid -A "OpenBSD ${OSREV} sparc64 bootonly CD" \
	    -P "Copyright (c) 2004 Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/sparc64 ${OSREV} boot-only CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	disklabel -w svnd0 fakecdrom "OpenBSD${REV}sparc64"
	vnconfig -u svnd0
	mksuncd f ${.OBJDIR}/${CDROM} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

install:
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -rf ${CDROM} ${.OBJDIR}/cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
