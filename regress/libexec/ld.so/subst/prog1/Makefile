# $OpenBSD: Makefile,v 1.4 2026/09/11 22:30:47 kurt Exp $

.include <bsd.obj.mk>

PROG_OBJDIR!=	if [ -d ${.CURDIR}/${__objdir} ]; then \
			echo "${.CURDIR}/${__objdir}"; \
		else \
			echo "${.CURDIR})"; \
		fi

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

ORIGIN_DIR!=	echo "OpenBSD/`uname -r`/`uname -m`"

PROG=		prog1
SRCS=		main.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD+=		-laa
LDFLAGS+=	-Wl,-z,origin,-rpath,'$$ORIGIN/$${OSNAME}/$$OSREL/$${PLATFORM}'
LDFLAGS+=	-L$(AA_OBJDIR)

REGRESS_TARGETS = test1

.PHONY: test1 test2

${ORIGIN_DIR}:
	mkdir -p ${ORIGIN_DIR}
	cp $(AA_OBJDIR)/libaa.so* ${ORIGIN_DIR}/

test1: $(PROG) ${ORIGIN_DIR}
	./$(PROG)

test2: $(PROG) ${ORIGIN_DIR}
	cd /tmp && PATH=${PROG_OBJDIR} $(PROG)

clean:
	rm -f a.out [Ee]rrs mklog *.core y.tab.h \
	    ${PROG} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}
	rm -rf OpenBSD

.include <bsd.regress.mk>
