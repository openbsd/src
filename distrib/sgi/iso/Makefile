#	$OpenBSD: Makefile,v 1.29 2018/11/13 12:48:23 visa Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

ALLIP=		22 26 27 28 30 32

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/game${OSrev}.tgz ${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/INSTALL.${MACHINE}
.for IP in ${ALLIP}
BASE+=		${RELDIR}/bsd.IP${IP} ${RELDIR}/bsd.rd.IP${IP}
.endfor
BASE+=		${RELDIR}/bsd.mp.IP27 ${RELDIR}/bsd.mp.IP30
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xfont${OSrev}.tgz \
		${RELXDIR}/xshare${OSrev}.tgz ${RELXDIR}/xserv${OSrev}.tgz

all: ${CDROM}

# Note regarding volume header size:
# boot32 currently (2013/12/29) needs 81 512 byte sectors, boot64 needs 190,
# bootecoff needs 73, the XFS partition needs 32768, and we need to keep some
# room for growth.

# The following values need to be consistent with the `cdroot' layout in
# /etc/disktab
SGIVOLSZ=8320	# oa, pp (size of volume header in 2048 byte sectors)
CDROMSZ=460	# nt     (size of the iso image in megabytes)

# Creating an IP27 bootable CD-ROM image with a usable FFS filesystem on it
# is extremely tricky. We are facing the following challenges:
# - the IP27 XFS filesystem will only get recognized correctly if the volume
#   header uses 512 byte sectors.
# - the OpenBSD disklabel will reuse the sector size from the volume header,
#   yet we need to build a label with 2048 byte sectors.
# This can be overcome by building a 512 byte sector volume header, then
# filling the 2048 byte sector filesystem.

${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=2048b count=${CDROMSZ}

	# Create the 512 byte sector volume header

	vnconfig -v -c vnd0 ${.OBJDIR}/${CDROM}

	/usr/mdec/sgivol -qi -h `expr ${SGIVOLSZ} \* 4` vnd0
	uudecode -o ${.OBJDIR}/xfs512.bin.gz \
	    ${.CURDIR}/../cdfs/xfs512.bin.gz.uue
	gzip -d ${.OBJDIR}/xfs512.bin.gz
	cat ${DESTDIR}/usr/mdec/boot64 >> ${.OBJDIR}/xfs512.bin
	dd if=${.OBJDIR}/xfs512.bin of=${.OBJDIR}/xfs obs=16m conv=osync
	/usr/mdec/sgivol -qw xfsboot ${.OBJDIR}/xfs vnd0
	/usr/mdec/sgivol -qw bootecof ${DESTDIR}/usr/mdec/bootecoff vnd0
	/usr/mdec/sgivol -ql bootecof sash vnd0
	/usr/mdec/sgivol -qw boot32 ${DESTDIR}/usr/mdec/boot32 vnd0
	/usr/mdec/sgivol -ql boot32 sashARCS vnd0
	/usr/mdec/sgivol -qw boot64 ${DESTDIR}/usr/mdec/boot64 vnd0
	/usr/mdec/sgivol -ql boot64 sash64 vnd0

	vnconfig -u vnd0

	# Create the 2048 byte sector media with the FFS partition

	vnconfig -v -c -t cdroot vnd0 ${.OBJDIR}/${CDROM}

	disklabel -w vnd0 cdroot "OpenBSD/sgi     "
	newfs /dev/rvnd0a
	mount /dev/vnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${DESTDIR}/usr/mdec/bootecoff ${.OBJDIR}/cd-dir/bootecoff
	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64

	ln ${.OBJDIR}/cd-dir/bootecoff ${.OBJDIR}/cd-dir/boot-IP22
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP26
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP27
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP28
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP30
	ln ${.OBJDIR}/cd-dir/boot32 ${.OBJDIR}/cd-dir/boot-IP32

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}
	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cat ${RELDIR}/SHA256 ${RELXDIR}/SHA256 > \
	    ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/SHA256
	# XXX no SHA256.sig

.for IP in ${ALLIP}
	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd.IP${IP} \
	    ${.OBJDIR}/cd-dir/bsd.rd.IP${IP}
.endfor

	umount ${.OBJDIR}/cd-dir

	# Summary
	disklabel vnd0
	/usr/mdec/sgivol vnd0

	vnconfig -u vnd0

install:
	cp ${CDROM} ${RELDIR}/

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u vnd0

clean cleandir:
	/bin/rm -f ${CDROM} xfs512.bin xfs
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
