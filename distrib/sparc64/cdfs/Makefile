#	$OpenBSD: Makefile,v 1.16 2011/06/08 19:16:19 krw Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=	cd${REV}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	-mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	-cp ${.OBJDIR}/../bsd.rd/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	ln ${.OBJDIR}/cd-dir/${OSREV}/sparc64/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/sparc64/bsd
	-cp ${DESTDIR}/usr/mdec/ofwboot ${.OBJDIR}/cd-dir/${OSREV}/sparc64

	echo 'm 1	sun4' > ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 2	sun4c' >> ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 3	sun4m' >> ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 4	sun4d' >> ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 5	sun4u' >> ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 5	sun4us' >> ${.OBJDIR}/cd-dir/.slicemapfile
	echo 'm 6	sun4v' >> ${.OBJDIR}/cd-dir/.slicemapfile
	mkhybrid -a -R -r -T -L -d -D -N \
	    -A "OpenBSD ${OSREV} sparc64 bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/sparc64 ${OSREV} boot-only CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	vnconfig -v -c vnd0 ${.OBJDIR}/${CDROM}
	disklabel -w vnd0 fakecdrom "OpenBSD${REV}sparc64"
	vnconfig -u vnd0
	mksuncd f ${.OBJDIR}/${CDROM} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	/bin/rm -rf ${CDROM} ${.OBJDIR}/cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
