#	$OpenBSD: Makefile,v 1.1 2007/08/10 02:28:53 deraadt Exp $

TOP=	${.CURDIR}/..

#.include "${TOP}/Makefile.inc"

CDROM=	install${OSrev}.iso
RELXDIR=	/home/relx-sparc
RELDIR?=	/home/rel-sparc
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/sparc

	cp -p ${RELDIR}/{base,comp,etc,game,man,misc}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp -p ${RELDIR}/{bsd,bsd.rd} \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp -p ${RELXDIR}/{xbase,xetc,xfont,xserv,xshare}${OSrev}.tgz \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp -p ${RELDIR}/INSTALL.sparc \
	    ${.OBJDIR}/cd-dir/${OSREV}/sparc

	ln -f ${.OBJDIR}/cd-dir/${OSREV}/sparc/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln -f ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	ln -f ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/vmunix # for kicks

	cp ${RELDIR}/boot ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp ${RELDIR}/bootxx ${.OBJDIR}/cd-dir/${OSREV}/sparc

	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} sparc Install CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/sparc   ${OSREV} Install CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	mount -t cd9660 /dev/svnd0a /mnt
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/sparc/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/sparc/boot$$" | cut -d' ' -f2` \
	    /mnt/${OSREV}/sparc/boot /usr/mdec/bootxx /dev/rsvnd0c
	disklabel -w svnd0 fakecdrom "OpenBSD/sparc   "
	umount /mnt
	vnconfig -u svnd0
	# XXX Some sparc machines fail to load the kernel correctly if the
	# XXX cd image is truncated.  It is not yet known why this happens.
	# XXX For now we pad the image.
	dd if=/dev/zero bs=2k count=200 >> ${.OBJDIR}/${CDROM}

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
