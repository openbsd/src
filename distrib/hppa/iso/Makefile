#	$OpenBSD: Makefile,v 1.2 2007/08/10 03:02:38 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-hppa
RELDIR?=	/home/rel-hppa

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz ${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.hppa
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/hppa

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/hppa
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/hppa

	ln -f ${.OBJDIR}/cd-dir/${OSREV}/hppa/bsd.rd \
	    ${.OBJDIR}/cd-dir/bsd.rd

	mkhybrid  -R -T -L -d -D -N \
	    -A "OpenBSD ${OSREV} hppa Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/hppa    ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	dd if=${DESTDIR}/usr/mdec/cdboot of=${.OBJDIR}/${CDROM} \
	    bs=32k count=1 conv=notrunc
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	disklabel -w svnd0 fakecdrom "OpenBSD/hppa    "
	vnconfig -u svnd0
	dd if=/dev/zero bs=64k count=1 >> ${.OBJDIR}/${CDROM}

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -rf cd-dir ${CDROM}

unconfig:
	-vnconfig -u svnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
