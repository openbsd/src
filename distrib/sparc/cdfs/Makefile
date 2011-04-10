#	$OpenBSD: Makefile,v 1.7 2011/04/10 20:00:43 krw Exp $

TOP=	${.CURDIR}/..

CDROM=	cd${OSrev}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp ${.OBJDIR}/../../ramdisk/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/sparc
	ln ${.OBJDIR}/cd-dir/${OSREV}/sparc/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/vmunix # for kicks
	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/cd-dir/${OSREV}/sparc
	cp ${DESTDIR}/usr/mdec/bootxx ${.OBJDIR}/cd-dir/${OSREV}/sparc
	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} sparc bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/sparc   ${OSREV} boot-only CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	mount -t cd9660 /dev/svnd0a /mnt
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/sparc/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/sparc/boot$$" | cut -d' ' -f2` \
	    /mnt/${OSREV}/sparc/boot /usr/mdec/bootxx /dev/rsvnd0c
	umount /mnt
	vnconfig -u svnd0
	# XXX Some sparc machines fail to load the kernel correctly if the
	# XXX cd image is truncated.  It is not yet known why this happens.
	# XXX For now we pad the image.
	dd if=/dev/zero bs=2k count=200 >> ${.OBJDIR}/${CDROM}

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir log

unconfig:
	-umount /mnt
	-vnconfig -u svnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
