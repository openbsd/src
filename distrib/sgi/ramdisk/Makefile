#	$OpenBSD: Makefile,v 1.23 2016/12/06 19:46:11 natano Exp $

REV=		${OSrev}

TOP=		${.CURDIR}/..

BSD_RD=		bsd.rd
IMAGE=		mr.fs
CBIN?=		instbin
CRUNCHCONF?=	${CBIN}.conf
CRUNCHCONFS?=	${.CURDIR}/../common/crunch.conf
LISTS=		${.CURDIR}/list
UTILS?=		${.CURDIR}/../../miniroot

MTREE=		${UTILS}/mtree.conf

DISKTYPE=	rdroot
MAKEFSARGS_RD=	-o disklabel=${DISKTYPE},minfree=0,density=4096

.ifndef DESTDIR
all ${IMAGE}:
	@echo setenv DESTDIR before makeing a ramdisk!
	@false
.else

ALLIP=		22 26 27 28 30 32
ALLBSD=
ALLBSDRD=
.for IP in ${ALLIP}
ALLBSD+=	bsd.IP${IP}
ALLBSDRD+=	${BSD_RD}.IP${IP}
all:	${ALLBSDRD}
.endfor

.for IP in ${ALLIP}
bsd.IP${IP}:
.ifndef NOBUILD
	cd ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP${IP} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
.endif
	cp -p ${.TOP}/../../sys/arch/sgi/compile/RAMDISK-IP${IP}/obj/bsd bsd.IP${IP}

${BSD_RD}.IP${IP}: ${IMAGE} bsd.IP${IP} rdsetroot
	cp bsd.IP${IP} ${BSD_RD}.IP${IP}
	${.OBJDIR}/rdsetroot ${BSD_RD}.IP${IP} ${IMAGE}
.endfor

${IMAGE}: ${CBIN}
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	TOPDIR=${TOP} CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} REV=${REV} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/${CBIN}
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

rdsetroot:	${TOP}/../common/elfrdsetroot.c
	${HOSTCC} ${HOSTCFLAGS} -o rdsetroot \
	    ${TOP}/../common/elfrdsetroot.c ${TOP}/../common/elf32.c \
	    ${TOP}/../common/elf64.c

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    lib*.a lib*.olist ${CBIN}.map \
	    ${ALLBSD} ${ALLBSDRD} *.o *.lo *.c rdsetroot
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
