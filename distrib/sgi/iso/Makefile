#	$OpenBSD: Makefile,v 1.7 2008/08/25 17:13:25 jsing Exp $

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

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	objcopy -Sg -R .comment ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD} > ${.OBJDIR}/bsd

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=2048b count=224
	vnconfig -v -c -s 2048 svnd0 ${.OBJDIR}/${CDROM}

	# 22 * 2048 byte sectors = 44KB
	/usr/mdec/sgivol -i -h 22 svnd0
	/usr/mdec/sgivol -w boot ${.OBJDIR}/boot svnd0
	/usr/mdec/sgivol -l boot sashARCS svnd0

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
	vnconfig -u svnd0

install:
	cp ${CDROM} ${RELDIR}/

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u svnd0

clean cleandir:
	/bin/rm -f ${CDROM} bsd boot
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
