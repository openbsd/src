#
#	$OpenBSD: Makefile.inc,v 1.17 2019/04/28 15:21:49 deraadt Exp $
#

# TOP is assumed to be defined by Makefile including this one.

UTILS=		${TOP}/../miniroot

LISTS?=		${.CURDIR}/../common/list
CRUNCHCONF?=	instbin.conf
MTREE=		${UTILS}/mtree.conf
STRIPOPTS?=	-R .eh_frame -R .SUNW_ctf -R .shstrtab

${CRUNCHCONF}: ${LISTS}
	awk -f ${UTILS}/makeconf.awk  ${LISTS} > instbin.conf

instbin.mk instbin.cache instbin.c: ${CRUNCHCONF}
	crunchgen -E -D ${BSDSRCDIR} -L ${DESTDIR}/usr/lib ${CRUNCHCONF}

instbin: instbin.mk instbin.cache instbin.c
	${MAKE} -f instbin.mk SRCLIBDIR=${.CURDIR}/../../../lib all
	strip ${STRIPOPTS} instbin

clean cleandir:
	/bin/rm -f *.core ${IMAGE} instbin instbin.mk instbin.cache \
	    *.o *.lo *.c \
	    lib*.a lib*.olist instbin.map ${CLEANFILES}
	/bin/rm -rf ${IMAGE}.d

.include <bsd.obj.mk>
.include <bsd.subdir.mk>
