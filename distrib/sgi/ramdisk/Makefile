#	$OpenBSD: Makefile,v 1.28 2019/04/29 15:56:26 deraadt Exp $

IMAGE=		mr.fs
CRUNCHCONF?=	instbin.conf
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
ALLBSDRD+=	bsd.rd.IP${IP}
all:	${ALLBSDRD}
.endfor

.for IP in ${ALLIP}
bsd.IP${IP}:
.ifndef NOBUILD
	cd ${.CURDIR}/../../../sys/arch/sgi/compile/RAMDISK-IP${IP} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
.endif
	cp -p ${.CURDIR}/../../../sys/arch/sgi/compile/RAMDISK-IP${IP}/obj/bsd bsd.IP${IP}

bsd.rd.IP${IP}: ${IMAGE} bsd.IP${IP}
	cp bsd.IP${IP} bsd.rd.IP${IP}
	rdsetroot bsd.rd.IP${IP} ${IMAGE}
.endfor

${IMAGE}: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MAKEFSARGS_RD} $@ $@.d

.endif

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib ${.ALLSRC}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map ${ALLBSD} ${ALLBSDRD} *.o *.lo *.c
	/bin/rm -rf ${IMAGE}.d

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
