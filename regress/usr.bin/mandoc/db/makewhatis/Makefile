# $OpenBSD: Makefile,v 1.1 2026/08/25 20:54:33 schwarze Exp $
#
# Copyright (c) 2026 Ingo Schwarze <schwarze@openbsd.org>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

FULL_TESTS =	dirarg dopt uopt
DB_TESTS =	${FULL_TESTS} Copt dopt_re
REGRESS_TARGETS = ${DB_TESTS} nopt topt

CLEANFILES +=	${DB_TESTS:C/$/.db/}
CLEANFILES +=	${REGRESS_TARGETS:C/$/.dout/}
CLEANFILES +=	dbm_dump man.conf


# === MAIN REGRESSION TARGETS ==========================================

.for NAME in ${FULL_TESTS} nopt topt
${NAME}: ${NAME}.dout
	diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

Copt: Copt.dout
	diff -u ${.CURDIR}/../out/dopt.dout Copt.dout

dopt_re: dopt_re.dout
	diff -u ${.CURDIR}/../out/dopt.dout dopt_re.dout


# === RUNNING DBM_DUMP =================================================

.for NAME in ${DB_TESTS}
${NAME}.dout: dbm_dump ${NAME}.db
	./dbm_dump ${NAME}.db > ${NAME}.dout
.endfor


# === RUNNING MAKEWHATIS ===============================================

dirarg.db: ${.CURDIR}/../man/sh.1
	@echo creating $@
	mkdir -p dirarg.dir/man1
	cp ${.CURDIR}/../man/sh.1 dirarg.dir/man1/
	makewhatis dirarg.dir
	cp dirarg.dir/mandoc.db dirarg.db

dopt.db: dirarg.db ${.CURDIR}/../man/fn.3
	@echo creating $@
	cp -pR dirarg.dir dopt.dir
	mkdir -p dopt.dir/man3
	cp ${.CURDIR}/../man/fn.3 dopt.dir/man3/
	makewhatis -d dopt.dir man3/fn.3
	cp dopt.dir/mandoc.db dopt.db

uopt.db: dopt.db
	@echo creating $@
	cp -pR dopt.dir uopt.dir
	makewhatis -u uopt.dir man1/sh.1
	cp uopt.dir/mandoc.db uopt.db

Copt.db: uopt.db
	@echo creating $@
	cp -pR uopt.dir Copt.dir
	rm Copt.dir/mandoc.db
	echo manpath ${.OBJDIR}/Copt.dir > man.conf
	makewhatis -C man.conf
	cp Copt.dir/mandoc.db Copt.db

dopt_re.db: uopt.db
	@echo creating $@
	cp -pR uopt.dir dopt_re.dir
	rm dopt_re.dir/mandoc.db
	makewhatis -d dopt_re.dir man3/fn.3
	cp dopt_re.dir/mandoc.db dopt_re.db

nopt.dout: dopt.db
	makewhatis -Dn dopt.dir > nopt.dout

topt.dout: ${.CURDIR}/../man/empty.1
	makewhatis -DDt ${.CURDIR}/../man/empty.1 > topt.dout


# === GETTING ACCESS TO THE UTILITY ====================================

dbm_dump:
	cd ${.CURDIR}/../$@ && make all
	ln -sf ${.OBJDIR}/../$@/$@ .


.include <bsd.regress.mk>

# must come after the .include: <bsd.prog.mk> contains .if !target(clean)
clean: clean_local

clean_local:
	rm -rf ${DB_TESTS:C/$/.dir/}
