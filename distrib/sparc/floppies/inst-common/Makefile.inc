#	$OpenBSD: Makefile.inc,v 1.11 1997/09/26 01:51:29 millert Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	/mnt
VND?=		svnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
PID!=		echo $$$$
REALIMAGE!=	echo /var/tmp/image.${PID}
IMAGE?=		inst${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
.if (${KERBEROS} == "yes")
CRUNCHCONF=	${COMMONDIR}/${CBIN}-krb.conf
.else
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
.endif
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=120k count=12
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} floppy3
	newfs -O -m 0 -o space -i 8192 -c 80 ${VND_RDEV}
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
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
.if (${KERBEROS} == "yes")
	mv ${CBIN}-krb ${CBIN}
.endif

clean cleandir:
	/bin/rm -f ${IMAGE} ${CBIN} ${CBIN}-krb.mk ${CBIN}-krb.cache \
	    ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

beforeinstall:
	cp ${IMAGE} ${DESTDIR}/snapshot

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
