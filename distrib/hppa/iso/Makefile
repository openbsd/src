#	$OpenBSD: Makefile,v 1.9 2011/04/18 16:52:09 thib Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.mp ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz \
		${RELXDIR}/xserv${OSrev}.tgz

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd \
	    ${.OBJDIR}/cd-dir/bsd.rd

	mkhybrid  -R -T -L -d -D -N \
	    -A "OpenBSD ${OSREV} ${MACHINE} Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/${MACHINE}    ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	dd if=${DESTDIR}/usr/mdec/cdboot of=${.OBJDIR}/${CDROM} \
	    bs=32k count=1 conv=notrunc
	dd if=/dev/zero bs=64k count=1 >> ${.OBJDIR}/${CDROM}

install:
	cp ${CDROM} ${RELDIR}/
	# update the SHA256 file
	grep -v ${CDROM} ${RELDIR}/SHA256 > ${RELDIR}/SHA256.new
	sum -a sha256 ${CDROM} >> ${RELDIR}/SHA256.new
	sort ${RELDIR}/SHA256.new > ${RELDIR}/SHA256
	rm ${RELDIR}/SHA256.new

clean cleandir:
	/bin/rm -rf cd-dir ${CDROM}

unconfig:
	-vnconfig -u vnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
