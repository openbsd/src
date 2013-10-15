# $OpenBSD: Makefile,v 1.3 2013/10/15 22:49:03 deraadt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

PROG=		prog1
SRCS=		main.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD+=		-laa
LDFLAGS+=	-Wl,-z,origin,-rpath,'$$ORIGIN/$${OSNAME}/$$OSREL/$${PLATFORM}'
LDFLAGS+=	-L$(AA_OBJDIR)

REGRESS_TARGETS = test

.PHONY: test
test: $(PROG)
	mkdir -p "OpenBSD/`uname -r`/`uname -m`"
	cp $(AA_OBJDIR)/libaa.so* "OpenBSD/`uname -r`/`uname -m`/"
	./$(PROG)

clean:
	rm -f a.out [Ee]rrs mklog *.core y.tab.h \
	    ${PROG} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}
	rm -rf OpenBSD

.include <bsd.regress.mk>
