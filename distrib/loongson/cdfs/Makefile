#	$OpenBSD: Makefile,v 1.1 2010/02/07 08:38:40 otto Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/loongson
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/loongson/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf
	cp ${.OBJDIR}/../ramdisk/${BSDRD} ${.OBJDIR}/cd-dir/${OSREV}/loongson
	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} loongson bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/loongson ${OSREV} boot CD" \
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
