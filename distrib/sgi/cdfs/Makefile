#	$OpenBSD: Makefile,v 1.10 2010/03/31 19:44:33 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso
FFS=	ffs.img
# must match the size of `minicdroot' in disktab
CDFSMB=	10
all: ${CDROM}

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/
	dd if=/dev/zero of=${.OBJDIR}/${FFS} bs=1m count=${CDFSMB}
	vnconfig -v -c svnd0 ${.OBJDIR}/${FFS}
	disklabel -w svnd0 minicdroot "OpenBSD/sgi     "
	newfs /dev/rsvnd0a
	mount /dev/svnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSrev}/sgi
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD}.IP27 > \
	    ${.OBJDIR}/cd-dir/bsd.IP27
	ln ${.OBJDIR}/cd-dir/bsd.IP27 ${.OBJDIR}/cd-dir/bsd.rd.IP27
	ln ${.OBJDIR}/cd-dir/bsd.IP27 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.IP27
	ln ${.OBJDIR}/cd-dir/bsd.IP27 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd.IP27
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD}.IP30 > \
	    ${.OBJDIR}/cd-dir/bsd.IP30
	ln ${.OBJDIR}/cd-dir/bsd.IP30 ${.OBJDIR}/cd-dir/bsd.rd.IP30
	ln ${.OBJDIR}/cd-dir/bsd.IP30 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.IP30
	ln ${.OBJDIR}/cd-dir/bsd.IP30 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd.IP30
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD}.IP32 > \
	    ${.OBJDIR}/cd-dir/bsd.IP32
	ln ${.OBJDIR}/cd-dir/bsd.IP32 ${.OBJDIR}/cd-dir/bsd.rd.IP32
	ln ${.OBJDIR}/cd-dir/bsd.IP32 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.IP32
	ln ${.OBJDIR}/cd-dir/bsd.IP32 ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd.IP32
	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64
	umount ${.OBJDIR}/cd-dir
	@df /dev/svnd0a
	vnconfig -u svnd0
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=`expr ${CDFSMB} + 2`
	vnconfig -v -c svnd0 ${.OBJDIR}/${CDROM}
	# boot32 needs 77, boot64 needs 173, and add some room for growth
	/usr/mdec/sgivol -i -h 320 svnd0 > ${.OBJDIR}/volhdr
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 svnd0
	/usr/mdec/sgivol -l boot32 sashARCS svnd0
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 svnd0
	/usr/mdec/sgivol -l boot64 sash64 svnd0
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
	/bin/rm -f ${CDROM} ${FFS} volhdr
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
