#	$OpenBSD: Makefile,v 1.9 2009/04/17 15:59:48 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/i386
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/i386/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf
	cp ${.OBJDIR}/../ramdisk_cd/${BSDRD} ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp ${DESTDIR}/usr/mdec/cdbr ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp ${DESTDIR}/usr/mdec/cdboot ${.OBJDIR}/cd-dir/${OSREV}/i386/cdboot
	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} i386 bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/i386    ${OSREV} boot-only CD" \
	    -b ${OSREV}/i386/cdbr -c ${OSREV}/i386/boot.catalog \
	    ${.OBJDIR}/cd-dir

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif	# RELEASEDIR

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
