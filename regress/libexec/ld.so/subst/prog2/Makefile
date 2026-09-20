# $OpenBSD: Makefile,v 1.2 2026/09/20 22:19:13 kurt Exp $

.include <bsd.obj.mk>

AA_DIR=${.CURDIR}/../libaa

AA_OBJDIR!=	if [ -d $(AA_DIR)/${__objdir} ]; then \
			echo "$(AA_DIR)/${__objdir}"; \
		else \
			echo "$(AA_DIR)"; \
		fi

PROG=		prog2
SRCS=		main.c
CPPFLAGS+= 	-I$(AA_DIR)
LDADD+=		-laa
LDFLAGS+=	-Wl,-rpath,'$$ORIGIN/../lib'
LDFLAGS+=	-L$(AA_OBJDIR)

REGRESS_TARGETS = test

.PHONY: test
test: $(PROG)
	mkdir -p lib bin bin2 bin3/bin
	cp $(AA_OBJDIR)/libaa.so* lib
	cp ./$(PROG) bin
	bin/$(PROG)
	ln -sf ../bin bin2/bin
	bin2/bin/$(PROG)
	ln -sf $$(realpath bin/$(PROG)) bin3/bin/$(PROG)
	bin3/bin/$(PROG)

clean:
	rm -f a.out [Ee]rrs mklog *.core y.tab.h \
	    ${PROG} ${OBJS} ${_LEXINTM} ${_YACCINTM} ${CLEANFILES}
	rm -rf lib bin bin2 bin3

.include <bsd.regress.mk>
