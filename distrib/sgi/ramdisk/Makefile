#	$OpenBSD: Makefile,v 1.31 2019/05/03 20:03:59 deraadt Exp $

MTREE=		${UTILS}/mtree.conf

LISTS=		${.CURDIR}/list
UTILS=		${.CURDIR}/../../miniroot

MRDISKTYPE=	rdroot
MRMAKEFSARGS=	-o disklabel=${MRDISKTYPE},minfree=0,density=4096

ALLIP=		22 26 27 28 30 32
ALLBSDRD=
.for IP in ${ALLIP}
ALLBSDRD+=	bsd.rd.IP${IP}
all:	${ALLBSDRD}
.endfor

.for IP in ${ALLIP}
bsd.rd.IP${IP}: mr.fs bsd.IP${IP}
	cp bsd.IP${IP} bsd.rd.IP${IP}
	rdsetroot bsd.rd.IP${IP} mr.fs

bsd.IP${IP}:
	cd ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/RAMDISK-IP${IP} && \
	    su ${BUILDUSER} -c '${MAKE} config && ${MAKE} clean && exec ${MAKE}'
	cp -p ${.CURDIR}/../../../sys/arch/${MACHINE}/compile/RAMDISK-IP${IP}/obj/bsd bsd.IP${IP}
.endfor

mr.fs: instbin
	rm -rf $@.d
	install -d -o root -g wheel $@.d
	mtree -def ${MTREE} -p $@.d -u
	CURDIR=${.CURDIR} OBJDIR=${.OBJDIR} OSrev=${OSrev} \
	    TARGDIR=$@.d UTILS=${UTILS} RELEASEDIR=${RELEASEDIR} \
	    sh ${UTILS}/runlist.sh ${LISTS}
	rm $@.d/instbin
	makefs ${MRMAKEFSARGS} $@ $@.d

instbin.mk instbin.cache instbin.c: instbin.conf
	crunchgen -M -E -D ${.CURDIR}/../../.. -L ${DESTDIR}/usr/lib \
	    -c instbin.c -e instbin -m instbin.mk instbin.conf

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all

instbin.conf: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

.ifdef RELEASEDIR
install:
	cp ${ALLBSDRD} ${RELEASEDIR}
.endif


clean cleandir:
	rm -f *.core mr.fs instbin instbin.mk instbin.cache \
	    lib*.a lib*.olist instbin.map ${ALLBSD} ${ALLBSDRD} *.o *.lo *.c
	rm -rf mr.fs.d

.include <bsd.obj.mk>
