# $OpenBSD: Makefile,v 1.3 2026/09/17 16:53:28 kurt Exp $

.include <bsd.obj.mk>

PROG_OBJDIR!=	if [ -d ${.CURDIR}/${__objdir} ]; then \
			echo "${.CURDIR}/${__objdir}"; \
		else \
			echo "${.CURDIR}"; \
		fi

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

YY_DIR=${.CURDIR}/../libyy

YY_OBJDIR!=	if [ -d $(YY_DIR)/${__objdir} ]; then \
			echo "$(YY_DIR)/${__objdir}"; \
		else \
			echo "$(YY_DIR)"; \
		fi

YY_ORIGIN_DIR=	libyy
AA_ORIGIN_DIR!=	echo "${YY_ORIGIN_DIR}/OpenBSD/`uname -r`/`uname -m`"

PROG=		prog3
SRCS=		main.c
CPPFLAGS+= 	-I$(YY_DIR)
LDADD+=		-lyy
LDFLAGS+=	-Wl,-z,origin,-rpath,'$$ORIGIN/libyy'
LDFLAGS+=	-L$(YY_OBJDIR)

REGRESS_TARGETS = test1 test2

.PHONY: test1 test2

${YY_ORIGIN_DIR}:
	mkdir -p ${YY_ORIGIN_DIR}
	cp $(YY_OBJDIR)/libyy.so* ${YY_ORIGIN_DIR}/

${AA_ORIGIN_DIR}: ${YY_ORIGIN_DIR}
	mkdir -p ${AA_ORIGIN_DIR}
	cp $(AA_OBJDIR)/libaa.so* ${AA_ORIGIN_DIR}/

test1: $(PROG) ${AA_ORIGIN_DIR}
	./$(PROG)

test2: $(PROG) ${AA_ORIGIN_DIR}
	cd /tmp && PATH=${PROG_OBJDIR} $(PROG)

clean:
	rm -f a.out [Ee]rrs mklog *.core y.tab.h \
	    ${PROG} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}
	rm -rf ${YY_ORIGIN_DIR}

.include <bsd.regress.mk>
