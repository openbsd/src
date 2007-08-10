#	$OpenBSD: Makefile,v 1.1 2007/08/10 02:28:53 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-i386
RELDIR?=	/home/rel-i386
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/i386
	mkdir -p ${.OBJDIR}/cd-dir/etc
	echo "set image /${OSREV}/i386/bsd.rd" > ${.OBJDIR}/cd-dir/etc/boot.conf

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp -p ${RELDIR}/{bsd,bsd.rd,bsd.mp} \
	    ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp -p ${RELDIR}/INSTALL.i386 \
	    ${.OBJDIR}/cd-dir/${OSREV}/i386

	cp -p ${RELDIR}/cdbr ${.OBJDIR}/cd-dir/${OSREV}/i386
	cp -p ${RELDIR}/cdboot ${.OBJDIR}/cd-dir/${OSREV}/i386/cdboot

	mkhybrid -a -R -T -L -l -d -D -N -o ${.OBJDIR}/${CDROM} -v -v \
	    -A "OpenBSD ${OSREV} i386 Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/i386    ${OSREV} Install CD" \
	    -b ${OSREV}/i386/cdbr -c ${OSREV}/i386/boot.catalog \
	    ${.OBJDIR}/cd-dir

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
