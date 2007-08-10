#	$OpenBSD: Makefile,v 1.1 2007/08/10 02:28:51 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${OSrev}.iso
RELXDIR=	/home/relx-alpha
RELDIR?=	/home/rel-alpha
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/alpha

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp -p ${RELDIR}/{bsd,bsd.rd} \
	    ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp -p ${RELDIR}/INSTALL.alpha \
	    ${.OBJDIR}/cd-dir/${OSREV}/alpha

	cp -p ${RELDIR}/boot ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp -p ${RELDIR}/bootxx ${.OBJDIR}/cd-dir/${OSREV}/alpha

	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} alpha Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/alpha   ${OSREV} Install CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	mount -t cd9660 /dev/svnd0a /mnt
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/alpha/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/alpha/boot$$" | cut -d' ' -f2` \
	    /mnt/${OSREV}/alpha/boot /usr/mdec/bootxx /dev/rsvnd0c
	#disklabel -w svnd0 fakecdrom "OpenBSD/alpha   "
	umount /mnt
	vnconfig -u svnd0

install:
	cp ${CDROM} ${RELDIR}/

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir log

unconfig:
	-umount /mnt
	-vnconfig -u svnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
