#	$Id: Makefile.inc,v 1.13 1997/01/25 09:16:39 deraadt Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
IMAGE?=		xxx${REV}.fs
PID!=		echo $$$$
REALIMAGE=	/tmp/image.${PID}
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
.if defined(KERBEROS)
CRUNCHCONF=	${COMMONDIR}/${CBIN}-krb.conf
.else
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
.endif
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
.ifndef FLOPPY3
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=120
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -B -b /usr/mdec/fdboot -s /usr/mdec/bootfd ${VND} floppy5
	newfs -O -m 0 -o space -i 3000 -c 80 ${VND_RDEV} floppy5
.else
	dd if=/dev/zero of=${REALIMAGE} bs=10k count=144
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w -B -b /usr/mdec/fdboot -s /usr/mdec/bootfd ${VND} floppy3
	newfs -O -m 0 -o space -i 3000 -c 80 ${VND_RDEV} floppy3
.endif
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	@echo ""
	@df -i ${MOUNT_POINT}
	@echo ""
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cat /*bin/* > /dev/null
	cp ${REALIMAGE} ${IMAGE}
	rm ${REALIMAGE}

unconfig:
	-umount -f ${MOUNT_POINT}
	-vnconfig -u ${VND}
	-/bin/rm -f ${IMAGE}

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -D ${TOP}/../../.. -m ${CBIN}.mk -L ${DESTDIR}/usr/lib \
		  ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	make -f ${CBIN}.mk all
.if defined(KERBEROS)
	mv ${CBIN}-krb ${CBIN}
.endif

install:
	cp ${IMAGE} ${DESTDIR}/snapshot/

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk \
		${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
