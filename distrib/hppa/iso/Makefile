#	$OpenBSD: Makefile,v 1.1 2007/08/10 02:28:53 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-hppa
RELDIR?=	/home/rel-hppa
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/hppa

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/hppa
	cp -p ${RELDIR}/{bsd,bsd.rd} \
	    ${.OBJDIR}/cd-dir/${OSREV}/hppa
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/hppa
	cp -p ${RELDIR}/INSTALL.hppa \
	    ${.OBJDIR}/cd-dir/${OSREV}/hppa

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
