#	$OpenBSD: Makefile,v 1.1 2007/08/10 02:28:53 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-sparc64
RELDIR?=	/home/rel-sparc64
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	-mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/sparc64

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	cp -p ${RELDIR}/{bsd,bsd.rd} \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc64
	cp -p ${RELDIR}/INSTALL.sparc64 \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc64

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
