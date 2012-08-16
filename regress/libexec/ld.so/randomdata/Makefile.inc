.include <bsd.obj.mk>

LIBAADIR!=	if test -d ${.CURDIR}/../libaa/${__objdir} ; then	\
			echo "${.CURDIR}/../libaa/${__objdir}" ;	\
		else							\
			echo "${.CURDIR}/../libaa" ;			\
		fi
LIBAA=		$(LIBAADIR)/libaa.a
