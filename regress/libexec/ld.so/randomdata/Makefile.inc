# $OpenBSD: Makefile.inc,v 1.2 2021/06/06 22:53:06 bluhm Exp $

.include <bsd.obj.mk>

LIBAADIR!=	if test -d ${.CURDIR}/../libaa/${__objdir} ; then	\
			echo "${.CURDIR}/../libaa/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libaa" ;			\
		fi
LIBAA=		$(LIBAADIR)/libaa.a
