#	$OpenBSD: Makefile,v 1.16 2010/10/18 05:40:47 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/bsd.IP27 ${RELDIR}/bsd.rd.IP27 \
		${RELDIR}/bsd.IP30 ${RELDIR}/bsd.rd.IP30 \
		${RELDIR}/bsd.IP32 ${RELDIR}/bsd.rd.IP32 \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz \
		${RELXDIR}/xserv${OSrev}.tgz

all: ${CDROM}

# Note regarding sgivol -h value:
# boot32 currently needs 20 2048 byte sectors, boot64 currently needs 44,
# and there is some room for growth.
# IF THIS VALUE IS EVER CHANGED, make sure it is consistent with the cdroot
# layout in /etc/disktab!
${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=2048b count=280
	vnconfig -v -c -t cdroot svnd0 ${.OBJDIR}/${CDROM}

	/usr/mdec/sgivol -i -h 80 svnd0
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 svnd0
	/usr/mdec/sgivol -l boot32 sashARCS svnd0
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 svnd0
	/usr/mdec/sgivol -l boot64 sash64 svnd0

	disklabel -w svnd0 cdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP27
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP30
	ln ${.OBJDIR}/cd-dir/boot32 ${.OBJDIR}/cd-dir/boot-IP32

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd.IP27 \
	    ${.OBJDIR}/cd-dir/bsd.rd.IP27
	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd.IP30 \
	    ${.OBJDIR}/cd-dir/bsd.rd.IP30
	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd.IP32 \
	    ${.OBJDIR}/cd-dir/bsd.rd.IP32

	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	umount ${.OBJDIR}/cd-dir
	vnconfig -u svnd0

install:
	cp ${CDROM} ${RELDIR}/
	# update the SHA256 file
	grep -v ${CDROM} ${RELDIR}/SHA256 > ${RELDIR}/SHA256.new
	sum -a sha256 ${CDROM} >> ${RELDIR}/SHA256.new
	sort ${RELDIR}/SHA256.new > ${RELDIR}/SHA256
	rm ${RELDIR}/SHA256.new

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u svnd0

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
