#	$OpenBSD: Makefile,v 1.16 2015/09/27 19:11:36 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

BSDRD=	bsd.rd
CDROM=	cd${REV}.iso
FFS=	ffs.img
# must match the size of `minicdroot' in disktab
CDFSMB=	15
all: ${CDROM}

ALLIP=		22 26 27 28 30 32

# Note regarding sgivol -h value:
# boot32 currently (2012/05/25) needs 87 512 byte sectors, boot64 needs 200,
# bootecoff needs 77, the XFS partition needs 32768, and we need to keep some
# room for growth.
SGIVOLSZ=	33500

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
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=`expr ${CDFSMB} + 1 + ${SGIVOLSZ} / 2048`
	vnconfig -v -c vnd0 ${.OBJDIR}/${CDROM}
	/usr/mdec/sgivol -i -h ${SGIVOLSZ} vnd0
	uudecode -o ${.OBJDIR}/xfs512.bin.gz ${.CURDIR}/xfs512.bin.gz.uue
	gzip -d ${.OBJDIR}/xfs512.bin.gz
	cat ${DESTDIR}/usr/mdec/boot64 >> ${.OBJDIR}/xfs512.bin
	dd if=${.OBJDIR}/xfs512.bin of=${.OBJDIR}/xfs obs=16m conv=osync
	/usr/mdec/sgivol -w xfsboot ${.OBJDIR}/xfs vnd0
	/usr/mdec/sgivol -w bootecoff ${DESTDIR}/usr/mdec/bootecoff vnd0
	/usr/mdec/sgivol -l bootecoff sash vnd0
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 vnd0
	/usr/mdec/sgivol -l boot32 sashARCS vnd0
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 vnd0
	/usr/mdec/sgivol -l boot64 sash64 vnd0
	vnconfig -u vnd0
	dd if=${.OBJDIR}/${FFS} of=${.OBJDIR}/${CDROM} bs=512 seek=${SGIVOLSZ}

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u vnd0

clean cleandir:
	/bin/rm -f ${CDROM} ${FFS} xfs512.bin xfs
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
