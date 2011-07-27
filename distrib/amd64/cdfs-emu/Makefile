#	$OpenBSD: Makefile,v 1.5 2011/07/27 14:53:33 espie Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDFS=	cdrom${REV}.fs
CDROM=	cdemu${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/amd64
	cp ${.OBJDIR}/../ramdisk_cd/${CDFS} ${.OBJDIR}/cd-dir/${OSREV}/amd64
	mkhybrid -a -R -T -L -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} amd64 bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/amd64   ${OSREV} boot-only CD" \
	    -b ${OSREV}/amd64/${CDFS} -c ${OSREV}/amd64/boot.catalog \
	    ${.OBJDIR}/cd-dir

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
