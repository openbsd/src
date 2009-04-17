#	$OpenBSD: Makefile,v 1.6 2009/04/17 03:58:54 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		cd${OSrev}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${.OBJDIR}/../bsd.rd/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	# make sure right kernel is in /
	ln ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd

	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp ${DESTDIR}/usr/mdec/bootxx ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} ${MACHINE} bootonly CD" \
	    -P "Copyright (c) `date +%Y` Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/${MACHINE}   ${OSREV} boot-only CD" \
	    ${.OBJDIR}/cd-dir 2>&1) | tee log
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	mount -t cd9660 /dev/svnd0a /mnt
	/usr/mdec/installboot -v \
	    -s `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f1` \
	    -e `cat log | grep -v 'Name' | egrep "/cd-dir/${OSREV}/${MACHINE}/boot$$" | cut -d' ' -f2` \
	    /mnt/${OSREV}/${MACHINE}/boot /usr/mdec/bootxx /dev/rsvnd0c
	#disklabel -w svnd0 fakecdrom "OpenBSD/${MACHINE}   "
	umount /mnt
	vnconfig -u svnd0

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
