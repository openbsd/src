#	$OpenBSD: Makefile,v 1.1 2003/03/04 09:28:19 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDFS=	cdrom${REV}.fs
CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp ${.OBJDIR}/../ramdisk_cd/${CDFS} ${.OBJDIR}/cd-dir/${OSREV}/i386
	mkhybrid -a -R -T -L -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} i386 bootonly CD" \
	    -P "Copyright (c) 2003 Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD${OSREV} CD boot-only CD      " \
            -V "OpenBSD3.3 CD 1 www.OpenBSD.org " \
	    -b ${OSREV}/i386/${CDFS} -c ${OSREV}/i386/boot.catalog \
	    ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
