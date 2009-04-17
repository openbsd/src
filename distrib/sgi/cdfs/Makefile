#	$OpenBSD: Makefile,v 1.6 2009/04/17 03:58:54 deraadt Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso
CDROM2=	cd${REV}.tmp
FFS=	ffs.img
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/
	#cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/boot
	objcopy -Sg -R .comment ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/boot
	strip ${.OBJDIR}/boot
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD} > ${.OBJDIR}/bsd
	dd if=/dev/zero of=${.OBJDIR}/${FFS} bs=1m count=8
	vnconfig -v -c svnd0 ${.OBJDIR}/${FFS}
	disklabel -w svnd0 rdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSrev}/sgi
	cp ${.OBJDIR}/bsd ${.OBJDIR}/cd-dir/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd
	cp ${.OBJDIR}/boot ${.OBJDIR}/cd-dir/boot
	umount ${.OBJDIR}/cd-dir
	@df /dev/svnd0a
	vnconfig -u svnd0
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=10
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	# would love to use -h 64, but boot is slightly too big
	/usr/mdec/sgivol -i -h 88 svnd0 > ${.OBJDIR}/volhdr
	/usr/mdec/sgivol -w boot ${.OBJDIR}/boot svnd0
	/usr/mdec/sgivol -l boot sashARCS svnd0
	vnconfig -u svnd0
	dd if=${.OBJDIR}/${FFS} of=${.OBJDIR}/${CDROM} bs=512 \
	    seek=`cat ${.OBJDIR}/volhdr | grep 'Volume Header' | awk '{print $$3}'`

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u svnd0

clean cleandir:
	/bin/rm -f ${CDROM} ${CDROM2} ${FFS} volhdr bsd boot
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
