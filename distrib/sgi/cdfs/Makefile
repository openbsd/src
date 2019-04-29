#	$OpenBSD: Makefile,v 1.21 2019/04/29 15:56:26 deraadt Exp $

CDROM=	cd${OSrev}.iso
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
	dd if=/dev/zero of=${FFS} bs=1m count=${CDFSMB}
	vnconfig -v ${FFS} > vnd
	disklabel -w `cat vnd` minicdroot "OpenBSD/sgi     "
	newfs /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSrev}/sgi
.for IP in ${ALLIP}
	gzip -9f < ${.OBJDIR}/../ramdisk/bsd.rd.IP${IP} > \
	    ${.OBJDIR}/cd-dir/bsd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/bsd.rd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.IP${IP}
	ln ${.OBJDIR}/cd-dir/bsd.IP${IP} ${.OBJDIR}/cd-dir/${OSrev}/sgi/bsd.rd.IP${IP}
.endfor
	cp ${DESTDIR}/usr/mdec/bootecoff ${.OBJDIR}/cd-dir/bootecoff
	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64
	umount ${.OBJDIR}/cd-dir
	df -i /dev/`cat vnd`a
	vnconfig -u `cat vnd`
	rm -f vnd
	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=1m count=`expr ${CDFSMB} + 1 + ${SGIVOLSZ} / 2048`
	vnconfig -v ${CDROM} > vnd
	/usr/mdec/sgivol -i -h ${SGIVOLSZ} `cat vnd`
	uudecode -o ${.OBJDIR}/xfs512.bin.gz ${.CURDIR}/xfs512.bin.gz.uue
	gzip -d ${.OBJDIR}/xfs512.bin.gz
	cat ${DESTDIR}/usr/mdec/boot64 >> ${.OBJDIR}/xfs512.bin
	dd if=${.OBJDIR}/xfs512.bin of=${.OBJDIR}/xfs obs=16m conv=osync
	/usr/mdec/sgivol -w xfsboot ${.OBJDIR}/xfs `cat vnd`
	/usr/mdec/sgivol -w bootecoff ${DESTDIR}/usr/mdec/bootecoff `cat vnd`
	/usr/mdec/sgivol -l bootecoff sash `cat vnd`
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 `cat vnd`
	/usr/mdec/sgivol -l boot32 sashARCS `cat vnd`
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 `cat vnd`
	/usr/mdec/sgivol -l boot64 sash64 `cat vnd`
	vnconfig -u `cat vnd`
	rm -f vnd
	dd if=${FFS} of=${CDROM} bs=512 seek=${SGIVOLSZ}

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

clean cleandir:
	/bin/rm -f ${CDROM} ${FFS} xfs512.bin xfs
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
