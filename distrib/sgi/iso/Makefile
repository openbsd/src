#	$OpenBSD: Makefile,v 1.2 2007/10/13 21:27:20 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR=	/home/relx-${MACHINE}
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
FFS=	ffs.img
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	objcopy -Sg -R .comment ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD} > ${.OBJDIR}/bsd
	dd if=/dev/zero of=${.OBJDIR}/${FFS} bs=10m count=32
	vnconfig -v -c svnd0 ${.OBJDIR}/${FFS}
	disklabel -w svnd0 cdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${.OBJDIR}/bsd ${.OBJDIR}/cd-dir/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/bsd.rd
	cp ${.OBJDIR}/boot ${.OBJDIR}/cd-dir/boot

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	umount ${.OBJDIR}/cd-dir
	@df /dev/svnd0a
	vnconfig -u svnd0
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=10
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	# would love to use -h 64, but boot is slightly too big
	/usr/mdec/sgivol -f -i -h 88 svnd0 > ${.OBJDIR}/volhdr
	/usr/mdec/sgivol -f -w boot ${.OBJDIR}/boot svnd0
	/usr/mdec/sgivol -f -l boot sashARCS svnd0
	vnconfig -u svnd0
	dd if=${.OBJDIR}/${FFS} of=${.OBJDIR}/${CDROM} bs=512 \
	    seek=`cat ${.OBJDIR}/volhdr | grep 'Volume Header' | awk '{print $$3}'`

install:
	cp ${CDROM} ${RELDIR}/

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u svnd0

clean cleandir:
	/bin/rm -f ${CDROM} ${FFS} volhdr bsd boot
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
