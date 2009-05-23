#	$OpenBSD: Makefile,v 1.11 2009/05/23 18:20:37 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz ${RELDIR}/misc${OSrev}.tgz \
		${RELDIR}/bsd ${RELDIR}/bsd.rd \
		${RELDIR}/INSTALL.${MACHINE}
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz \
		${RELXDIR}/xserv${OSrev}.tgz

BSDRD=	bsd.rd
all: ${CDROM}

# Note regarding sgivol -h value:
# boot32 currently needs 20 2048 byte sectors, boot64 currently needs 44,
# and there is some room for growth.
# IF THIS VALUE IS EVER CHANGED, make sure it is consistent with the cdroot
# layout in /etc/disktab!
${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/boot64
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD} > ${.OBJDIR}/bsd

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=2048b count=224
	vnconfig -v -c -t cdroot svnd0 ${.OBJDIR}/${CDROM}

	/usr/mdec/sgivol -i -h 80 svnd0
	/usr/mdec/sgivol -w boot32 ${.OBJDIR}/boot32 svnd0
	/usr/mdec/sgivol -l boot32 sashARCS svnd0
	/usr/mdec/sgivol -w boot64 ${.OBJDIR}/boot64 svnd0
	/usr/mdec/sgivol -l boot64 sash64 svnd0

	disklabel -w svnd0 cdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${.OBJDIR}/bsd ${.OBJDIR}/cd-dir/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/bsd.rd
	cp ${.OBJDIR}/boot32 ${.OBJDIR}/cd-dir/boot32
	ln ${.OBJDIR}/boot32 ${.OBJDIR}/cd-dir/boot-IP32
	cp ${.OBJDIR}/boot64 ${.OBJDIR}/cd-dir/boot64
	ln ${.OBJDIR}/boot64 ${.OBJDIR}/cd-dir/boot-IP27
	ln ${.OBJDIR}/boot64 ${.OBJDIR}/cd-dir/boot-IP30

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
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
	/bin/rm -f ${CDROM} bsd boot32 boot64 boot-IP27 boot-IP30 boot-IP32
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
