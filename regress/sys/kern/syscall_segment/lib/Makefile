#	$OpenBSD: Makefile,v 1.1 2019/11/27 17:15:36 mortimer Exp $

PROG= syscall_segment
LIBDIR= ${.CURDIR}/../libgadgetsyscall/${__objdir}
LDADD+= -L${LIBDIR} -lgadgetsyscall

run-regress-${PROG}: ${PROG}
	rm -f ./${PROG}.core
	if LD_LIBRARY_PATH=${LIBDIR} ./${PROG}; then false; else true; fi
	if [ ! -e ./${PROG}.core ]; then echo "No coredump"; false; fi

.include <bsd.regress.mk>
