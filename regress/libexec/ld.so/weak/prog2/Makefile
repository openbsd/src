# $OpenBSD: Makefile,v 1.1 2002/11/13 21:52:11 fgsch Exp $

.include <bsd.obj.mk>

PROG=	prog2
SRCS=	main.c

LIBWEAK!=	if test -d ${.CURDIR}/../libweak/${__objdir} ; then	\
			echo "${.CURDIR}/../libweak/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libweak" ;			\
		fi

LDADD+= 	-Wl,-rpath,${LIBWEAK} -L${LIBWEAK} -lweak

.include <bsd.regress.mk>
