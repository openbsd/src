#	$OpenBSD: Makefile,v 1.1 2021/06/02 07:32:34 semarie Exp $

PROG =	test1

ADIR !=	if test -d ${.CURDIR}/../liba/${__objdir}; then	\
		echo "${.CURDIR}/../liba/${__objdir}";	\
	else						\
		echo "${.CURDIR}/../liba";		\
	fi

CFLAGS +=	-DLIBNAME=\"${ADIR}/liba.so.0.0\"

.include <bsd.regress.mk>
