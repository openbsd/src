#	$OpenBSD: Makefile,v 1.2 2007/08/10 03:02:38 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-sparc64
RELDIR?=	/home/rel-sparc64

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz ${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.sparc64
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir
	-mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/sparc64

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/sparc64

	cp -p ${RELDIR}/ofwboot ${.OBJDIR}/cd-dir/${OSREV}/sparc64

	mkhybrid -R -T -L -d -D -N \
	    -A "OpenBSD ${OSREV} sparc64 Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/sparc64 ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	disklabel -w svnd0 fakecdrom "OpenBSD${REV}sparc64"
	vnconfig -u svnd0
	mksuncd f ${.OBJDIR}/${CDROM} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -rf ${CDROM} ${.OBJDIR}/cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
