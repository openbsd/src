#	$OpenBSD: Makefile,v 1.3 2004/03/11 22:16:50 deraadt Exp $

TOP=	${.CURDIR}/..

CDROM=	cd${OSrev}.iso

all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp ${.OBJDIR}/../bsd.rd/bsd.rd ${.OBJDIR}/cd-dir/${OSREV}/alpha
	ln ${.OBJDIR}/cd-dir/${OSREV}/alpha/bsd.rd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd.rd ${.OBJDIR}/cd-dir/bsd
	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/cd-dir/${OSREV}/alpha
	cp ${DESTDIR}/usr/mdec/bootxx ${.OBJDIR}/cd-dir/${OSREV}/alpha
	(mkhybrid -a -R -v -v -T -L -d -D -N -o ${.OBJDIR}/${CDROM} \
	    -A "OpenBSD ${OSREV} alpha bootonly CD" \
	    -P "Copyright (c) 2004 Theo de Raadt, The OpenBSD project" \
	    -p "Theo de Raadt <deraadt@openbsd.org>" \
	    -V "OpenBSD/alpha   ${OSREV} boot-only CD" \
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
	cp ${CDROM} ${DESTDIR}/snapshot

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir log

unconfig:
	-umount /mnt
	-vnconfig -u svnd0

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
