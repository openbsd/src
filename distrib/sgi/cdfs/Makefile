#	$OpenBSD: Makefile,v 1.1 2005/04/27 03:15:54 deraadt Exp $

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
	cp ${.OBJDIR}/../ramdisk/${BSDRD} ${.OBJDIR}/bsd
	gzip -9f bsd
	dd if=/dev/zero of=${.OBJDIR}/${FFS} bs=1m count=8
	vnconfig -v -c svnd0 ${.OBJDIR}/${FFS}
	disklabel -w svnd0 rdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSrev}/sgi
	cp ${.OBJDIR}/bsd.gz ${.OBJDIR}/cd-dir/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/bsd.rd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd
	ln ${.OBJDIR}/cd-dir/bsd ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd
	cp ${DESTDIR}/usr/mdec/boot ${.OBJDIR}/cd-dir/boot
	umount ${.OBJDIR}/cd-dir
	@df /dev/svnd0a
	vnconfig -u svnd0

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=10
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	/usr/mdec/sgivol -f -i svnd0 | tee ${.OBJDIR}/volhdr
	/usr/mdec/sgivol -f -w boot ${DESTDIR}/usr/mdec/boot svnd0
	vnconfig -u svnd0
	dd if=${.OBJDIR}/${FFS} of=${.OBJDIR}/${CDROM} bs=512 \
	    seek=`cat ${.OBJDIR}/volhdr | grep 'Volume Header' | awk '{print $$3}'`

install:
	cp ${CDROM} ${DESTDIR}/snapshot

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u svnd0

clean cleandir:
	/bin/rm -f ${CDROM} ${CDROM2} ${FFS} volhdr bsd bsd.gz
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
