#
#	$OpenBSD: Makefile.inc,v 1.15 2017/09/17 16:33:08 deraadt Exp $
#

# TOP is assumed to be defined by Makefile including this one.

CBIN?=		instbin
UTILS=		${TOP}/../miniroot

LISTS?=		${.CURDIR}/../common/list
CRUNCHCONF?=	${CBIN}.conf
MTREE=		${UTILS}/mtree.conf
STRIP=		strip
STRIPOPTS?=	-R .eh_frame -R .SUNW_ctf -R .shstrtab

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk CBIN=${CBIN} ${LISTS} > ${CBIN}.conf

${CBIN}.mk ${CBIN}.cache ${CBIN}.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

${CBIN}: ${CBIN}.mk ${CBIN}.cache ${CBIN}.c
	${MAKE} -f ${CBIN}.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	${STRIP} ${STRIPOPTS} ${CBIN}

clean cleandir:
	/bin/rm -f *.core ${IMAGE} ${CBIN} ${CBIN}.mk ${CBIN}.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist ${CBIN}.map ${CLEANFILES}
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
