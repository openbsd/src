#	$OpenBSD: Makefile,v 1.3 2007/08/10 03:02:38 deraadt Exp $

TOP=		${.CURDIR}/..

#.include "${TOP}/Makefile.inc"

CDROM=		install${OSrev}.iso
RELXDIR=	/home/relx-macppc
RELDIR?=	/home/rel-macppc

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz ${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.macppc
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz

all: ${CDROM}

${CDROM}: ${BASE} ${XBASE}
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/macppc/

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/macppc

	cp -p ${.CURDIR}/../../../sys/arch/macppc/stand/tbxidata/bsd.tbxi \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp -p ${RELDIR}/ofwboot ${.OBJDIR}/cd-dir/

	mkhybrid -v -v -r -part -hfs \
	    -hfs-bless ${.OBJDIR}/cd-dir/${OSREV}/macppc \
	    -map ${.CURDIR}/../../../gnu/usr.sbin/mkhybrid/src/more.mapping \
	    -A "OpenBSD ${OSREV} macppc Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/macppc  ${OSREV} Install CD" \
	    -o ${.OBJDIR}/${CDROM} ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
