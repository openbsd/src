#	$Id: Makefile,v 1.1.1.1 1995/10/18 08:38:05 deraadt Exp $

SUBDIR= libg++ genclass

INCLUDEDIRS=	iostream libg++ g++-include

beforeinstall:
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/g++
	install -d -o ${BINOWN} -g ${BINGRP} -m 755 \
		${DESTDIR}/usr/include/g++/gen
	@-for i in ${INCLUDEDIRS}; do \
		echo installing includes from $$i ; \
		(cd $$i; for j in *.[ih]; do \
			cmp -s $$j ${DESTDIR}/usr/include/g++/$$j || \
			install -c -o ${BINOWN} -g ${BINGRP} -m 644 $$j \
				${DESTDIR}/usr/include/g++/$$j; \
		done); \
	done
	@echo installing includes from g++-include/gen
	@(cd g++-include/gen ; for j in *.*P; do \
		cmp -s $$j ${DESTDIR}/usr/include/g++/gen/$$j || \
		install -c -o ${BINOWN} -g ${BINGRP} -m 444 $$j \
			${DESTDIR}/usr/include/g++/gen/$$j; \
	done)

.include <bsd.subdir.mk>
