#	$OpenBSD: Makefile,v 1.2 2007/08/10 02:45:07 deraadt Exp $

TOP=		${.CURDIR}/..

#.include "${TOP}/Makefile.inc"

CDROM=		install${OSrev}.iso
RELXDIR=	/home/relx-macppc
RELDIR?=	/home/rel-macppc
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir/
	-@mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/macppc/

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp -p ${RELDIR}/{bsd,bsd.rd} \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc
	cp -p ${RELDIR}/INSTALL.macppc \
	    ${.OBJDIR}/cd-dir/${OSREV}/macppc

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
