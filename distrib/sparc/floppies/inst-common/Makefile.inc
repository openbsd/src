#	$Id: Makefile.inc,v 1.5 1997/03/31 07:36:05 deraadt Exp $

# TOP is assumed to be defined by Makefile including this one.

CBIN=		instbin
COMMONDIR=	${TOP}/inst-common

MOUNT_POINT?=	/mnt
VND?=		vnd0
VND_DEV=	/dev/${VND}a
VND_RDEV=	/dev/r${VND}a
PID!=		echo $$$$
REALIMAGE!=	echo /tmp/image.${PID}
IMAGE?=		inst${REV}.fs
MDEC=		${DESTDIR}/usr/mdec

LISTS=		${COMMONDIR}/list ${.CURDIR}/list
.if defined(KERBEROS)
CRUNCHCONF=	${COMMONDIR}/${CBIN}-krb.conf
.else
CRUNCHCONF=	${COMMONDIR}/${CBIN}.conf
.endif
MTREE=		${COMMONDIR}/mtree.conf

all: ${CBIN}
	dd if=/dev/zero of=${REALIMAGE} bs=120k count=12
	vnconfig -v -c ${VND} ${REALIMAGE}
	disklabel -w ${VND} floppy3
	newfs -O -m 0 -o space -i 8192 -c 80 ${VND_RDEV} floppy
	mount ${VND_DEV} ${MOUNT_POINT}
	mtree -def ${MTREE} -p ${MOUNT_POINT}/ -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} \
	    TARGDIR=${MOUNT_POINT} sh ${TOP}/runlist.sh ${LISTS}
	df -i ${MOUNT_POINT}
	umount ${MOUNT_POINT}
	vnconfig -u ${VND}
	cat /bin/* > /dev/null          # flush buffer cache (yuck)
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

clean cleandir:
	/bin/rm -f ${IMAGE} ${CBIN} ${CBIN}-krb.mk ${CBIN}-krb.cache \
	    ${CBIN}.mk ${CBIN}.cache *.o *.lo *.c

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
