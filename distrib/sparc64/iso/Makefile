#	$OpenBSD: Makefile,v 1.4 2007/10/17 21:32:38 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz ${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.mp ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz \
		${RELXDIR}/xserv${OSrev}.tgz

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir
	-mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp -p ${RELDIR}/ofwboot ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	mkhybrid -R -T -L -d -D -N \
	    -A "OpenBSD ${OSREV} ${MACHINE} Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/${MACHINE} ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	disklabel -w svnd0 fakecdrom "OpenBSD${REV}${MACHINE}"
	vnconfig -u svnd0
	mksuncd f ${.OBJDIR}/${CDROM} ${.OBJDIR}/../miniroot/miniroot${REV}.fs

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -rf ${CDROM} ${.OBJDIR}/cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
