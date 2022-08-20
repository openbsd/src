# $OpenBSD: Makefile,v 1.1 2022/08/20 14:13:48 sthen Exp $

.include <bsd.obj.mk>

PROG =	test1

LIBADIR !=	if test -d ${.CURDIR}/../liba/${__objdir}; then	\
			echo "${.CURDIR}/../liba/${__objdir}";	\
		else						\
			echo "${.CURDIR}/../liba";		\
		fi

LIBBDIR !=	if test -d ${.CURDIR}/../libb/${__objdir}; then	\
			echo "${.CURDIR}/../libb/${__objdir}";	\
		else						\
			echo "${.CURDIR}/../libb";		\
		fi

LIBANAME =	${LIBADIR}/liba.so.0.0
LIBBNAME =	${LIBBDIR}/libb.so.0.0

CFLAGS +=	-DLIBANAME=\"${LIBANAME}\" \
		-DLIBBNAME=\"${LIBBNAME}\"

REGRESS_TARGETS += run-regress

run-regress: ${PROG}
	if ./${PROG} | grep found; then echo failed; exit 1; fi
	LD_PRELOAD=${LIBANAME} ./${PROG} | grep ${LIBANAME}.found
	LD_PRELOAD=${LIBBNAME} ./${PROG} | grep ${LIBBNAME}.found

.include <bsd.regress.mk>
