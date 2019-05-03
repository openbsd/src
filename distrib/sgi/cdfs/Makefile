#	$OpenBSD: Makefile,v 1.28 2019/05/03 23:53:05 deraadt Exp $

FS=		miniroot${OSrev}.fs
CDROM=		cd${OSrev}.iso
# must match the size of `minicdroot' in disktab
CDFSMB=	15
MOUNT_POINT=	/mnt

all: ${CDROM}

ALLIP=		22 26 27 28 30 32

# Note regarding sgivol -h value:
# boot32 currently (2012/05/25) needs 87 512 byte sectors, boot64 needs 200,
# bootecoff needs 77, the XFS partition needs 32768, and we need to keep some
# room for growth.
SGIVOLSZ=	33500

${CDROM}:
	dd if=/dev/zero of=${FS} bs=1m count=${CDFSMB}
	vnconfig -v ${FS} > vnd
	disklabel -w `cat vnd` minicdroot "OpenBSD/sgi     "
	newfs -m 0 -o space -i 524288 /dev/r`cat vnd`a
	mount /dev/`cat vnd`a ${MOUNT_POINT}
	mkdir -p ${MOUNT_POINT}/${OSrev}/sgi
.for IP in ${ALLIP}
	gzip -9cf ${.OBJDIR}/../ramdisk/bsd.rd.IP${IP} > \
	    ${MOUNT_POINT}/bsd.IP${IP}
	ln ${MOUNT_POINT}/bsd.IP${IP} ${MOUNT_POINT}/bsd.rd.IP${IP}
	ln ${MOUNT_POINT}/bsd.IP${IP} ${MOUNT_POINT}/${OSrev}/sgi/bsd.IP${IP}
	ln ${MOUNT_POINT}/bsd.IP${IP} ${MOUNT_POINT}/${OSrev}/sgi/bsd.rd.IP${IP}
.endfor
	cp ${DESTDIR}/usr/mdec/bootecoff ${MOUNT_POINT}/bootecoff
	cp ${DESTDIR}/usr/mdec/boot32 ${MOUNT_POINT}/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${MOUNT_POINT}/boot64
	umount ${MOUNT_POINT}
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
	dd if=${FS} of=${CDROM} bs=512 seek=${SGIVOLSZ}

unconfig:
	-umount -f ${MOUNT_POINT}
	-[ -f vnd ] && vnconfig -u `cat vnd` && rm -f vnd

.ifdef RELEASEDIR
install:
	cp ${CDROM} ${RELEASEDIR}
.endif

clean cleandir:
	rm -f ${CDROM} ${FS} xfs512.bin xfs
	rm -rf cd-dir

.include <bsd.obj.mk>
