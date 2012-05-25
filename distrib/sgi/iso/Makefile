#	$OpenBSD: Makefile,v 1.18 2012/05/25 17:32:38 miod Exp $

TOP=	${.CURDIR}/..

.include "${TOP}/Makefile.inc"

CDROM=		install${REV}.iso
RELXDIR?=	/home/relx-${MACHINE}
RELDIR?=	/home/rel-${MACHINE}

ALLIP=		22 27 28 30 32

BASE=		${RELDIR}/base${OSrev}.tgz ${RELDIR}/comp${OSrev}.tgz \
		${RELDIR}/etc${OSrev}.tgz ${RELDIR}/game${OSrev}.tgz \
		${RELDIR}/man${OSrev}.tgz \
		${RELDIR}/INSTALL.${MACHINE}
.for IP in ${ALLIP}
BASE+=		${RELDIR}/bsd.IP${IP} ${RELDIR}/bsd.rd.IP${IP}
.endfor
BASE+=		${RELDIR}/bsd.mp.IP30
XBASE=		${RELXDIR}/xbase${OSrev}.tgz ${RELXDIR}/xetc${OSrev}.tgz \
		${RELXDIR}/xfont${OSrev}.tgz ${RELXDIR}/xshare${OSrev}.tgz \
		${RELXDIR}/xserv${OSrev}.tgz

all: ${CDROM}

# Note regarding sgivol -h value:
# boot32 currently (2012/05/25) needs 22 2048 byte sectors, boot64 needs 50,
# bootecoff needs 20, and we need to keep some room for growth.
# IF THIS VALUE IS EVER CHANGED, make sure it is consistent with the `cdroot'
# layout in /etc/disktab!
${CDROM}:
	-rm -rf ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/

	dd if=/dev/zero of=${.OBJDIR}/${CDROM} bs=2048b count=300
	vnconfig -v -c -t cdroot vnd0 ${.OBJDIR}/${CDROM}

	/usr/mdec/sgivol -i -h 128 vnd0
	/usr/mdec/sgivol -w bootecoff ${DESTDIR}/usr/mdec/bootecoff vnd0
	/usr/mdec/sgivol -l bootecoff sash vnd0
	/usr/mdec/sgivol -w boot32 ${DESTDIR}/usr/mdec/boot32 vnd0
	/usr/mdec/sgivol -l boot32 sashARCS vnd0
	/usr/mdec/sgivol -w boot64 ${DESTDIR}/usr/mdec/boot64 vnd0
	/usr/mdec/sgivol -l boot64 sash64 vnd0

	disklabel -w vnd0 cdroot "OpenBSD/sgi     "
	newfs /dev/rvnd0a
	mount /dev/vnd0a ${.OBJDIR}/cd-dir
	mkdir -p ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	cp ${DESTDIR}/usr/mdec/bootecoff ${.OBJDIR}/cd-dir/bootecoff
	cp ${DESTDIR}/usr/mdec/boot32 ${.OBJDIR}/cd-dir/boot32
	cp ${DESTDIR}/usr/mdec/boot64 ${.OBJDIR}/cd-dir/boot64
	ln ${.OBJDIR}/cd-dir/bootecoff ${.OBJDIR}/cd-dir/boot-IP22
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP27
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP28
	ln ${.OBJDIR}/cd-dir/boot64 ${.OBJDIR}/cd-dir/boot-IP30
	ln ${.OBJDIR}/cd-dir/boot32 ${.OBJDIR}/cd-dir/boot-IP32

	cp -p ${BASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

.for IP in ${ALLIP}
	ln -f ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}/bsd.rd.IP${IP} \
	    ${.OBJDIR}/cd-dir/bsd.rd.IP${IP}
.endfor

	cp -p ${XBASE} ${.OBJDIR}/cd-dir/${OSREV}/${MACHINE}

	umount ${.OBJDIR}/cd-dir
	vnconfig -u vnd0

install:
	cp ${CDROM} ${RELDIR}/
	# update the SHA256 file
	grep -v ${CDROM} ${RELDIR}/SHA256 > ${RELDIR}/SHA256.new
	sum -a sha256 ${CDROM} >> ${RELDIR}/SHA256.new
	sort ${RELDIR}/SHA256.new > ${RELDIR}/SHA256
	rm ${RELDIR}/SHA256.new

unconfig:
	-umount /mnt
	-umount ${.OBJDIR}/cd-dir
	-vnconfig -u vnd0

clean cleandir:
	/bin/rm -f ${CDROM}
	rm -rf cd-dir

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
