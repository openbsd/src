#	$OpenBSD: Makefile,v 1.12 2012/05/25 17:32:38 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso
FFS=	ffs.img
# must match the size of `minicdroot' in disktab
CDFSMB=	12
all: ${CDROM}

ALLIP=		22 27 28 30 32

# Note regarding sgivol -h value:
# boot32 currently (2012/05/25) needs 87 512 byte sectors, boot64 needs 200,
# bootecoff needs 77, and we need to keep some room for growth.
${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/
	dd if=/dev/zero of=${.OBJDIR}/${FFS} bs=1m count=${CDFSMB}
	vnconfig -v -c vnd0 ${.OBJDIR}/${FFS}
	disklabel -w vnd0 minicdroot "OpenBSD/sgi     "
	newfs /dev/rvnd0a
	mount /dev/vnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSrev}/sgi
.for IP in ${ALLIP}
	gzip -9f < ${.OBJDIR}/../ramdisk/${BSDRD}.IP${IP} > \
	    ${.OBJDIR}/cd-dir/bsd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/bsd.rd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd.IP${IP}
.endfor
	cp ${DESTDIR}/usr/mdec/bootecoff ${.OBJDIR}/cd-dir/bootecoff
	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64
	umount ${.OBJDIR}/cd-dir
	@df /dev/vnd0a
	vnconfig -u vnd0
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=`expr ${CDFSMB} + 2`
	vnconfig -v -c vnd0 ${.OBJDIR}/${CDROM}
	/usr/mdec/sgivol -i -h 400 vnd0 > ${.OBJDIR}/volhdr
	/usr/mdec/sgivol -w bootecoff ${DESTDIR}/usr/mdec/bootecoff vnd0
	/usr/mdec/sgivol -l bootecoff sash vnd0
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 vnd0
	/usr/mdec/sgivol -l boot32 sashARCS vnd0
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 vnd0
	/usr/mdec/sgivol -l boot64 sash64 vnd0
	vnconfig -u vnd0
	dd if=${.OBJDIR}/${FFS} of=${.OBJDIR}/${CDROM} bs=512 \
	    seek=`cat ${.OBJDIR}/volhdr | grep 'Volume Header' | awk '{print $$3}'`

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u vnd0

clean cleandir:
	/bin/rm -f ${CDROM} ${FFS} volhdr
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
