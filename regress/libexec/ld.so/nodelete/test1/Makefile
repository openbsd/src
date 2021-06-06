# $OpenBSD: Makefile,v 1.3 2021/06/06 22:53:06 bluhm Exp $

.include <bsd.obj.mk>

PROG =	test1

LIBADIR !=	if test -d ${.CURDIR}/../liba/${__objdir}; then	\
			echo "${.CURDIR}/../liba/${__objdir}";	\
		else						\
			echo "${.CURDIR}/../liba";		\
		fi

CFLAGS +=	-DLIBNAME=\"${LIBADIR}/liba.so.0.0\"

.include <bsd.regress.mk>
