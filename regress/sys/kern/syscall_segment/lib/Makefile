# $OpenBSD: Makefile,v 1.3 2020/10/16 21:34:59 bluhm Exp $

PROG=	syscall_library
.if ${.CURDIR} == ${.OBJDIR}
LIBDIR=	../libgadgetsyscall
.else
LIBDIR=	${.CURDIR}/../libgadgetsyscall/${__objdir}
.endif
LDADD+=	-L${LIBDIR} -lgadgetsyscall

run-regress-${PROG}: ${PROG}
	rm -f ${PROG}.core
	! LD_LIBRARY_PATH=${LIBDIR} ./${PROG}
	if [ ! -e ${PROG}.core ]; then echo "No coredump"; false; fi

.include <bsd.regress.mk>
