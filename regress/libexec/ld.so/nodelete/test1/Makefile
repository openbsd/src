#	$OpenBSD: Makefile,v 1.2 2021/06/04 10:01:49 semarie Exp $

.include <bsd.obj.mk>

PROG =	test1

ADIR !=	if test -d ${.CURDIR}/../liba/${__objdir}; then	\
		echo "${.CURDIR}/../liba/${__objdir}";	\
	else						\
		echo "${.CURDIR}/../liba";		\
	fi

CFLAGS +=	-DLIBNAME=\"${ADIR}/liba.so.0.0\"

.include <bsd.regress.mk>
