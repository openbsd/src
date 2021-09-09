# $OpenBSD: Makefile,v 1.11 2021/09/09 11:48:06 schwarze Exp $
#
# Copyright (c) 2016 Ingo Schwarze <schwarze@openbsd.org>
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

# D: dbm_dump	M: man	O: out	E: err
D_MOE_TESTS =	empty
DO_MO_TESTS =	onepage twopages threepages threemacros
DO_MO_TESTS +=	sortpages sortpages_rev so so_rev sortnames
DO_MO_TESTS +=	twosect twoarch threearch fn in va sh xr
DOE_M_TESTS =	padipage padfpage padmpage 
DOE_M_TESTS +=	padmacros padimacro padmmacro
DOE_M_TESTS +=	padientry padfentry padmentry padfmagic
DOE_MOE_TESTS =	badname0 badname badsect0 badsect badarch0 badarch
DOE_MOE_TESTS += baddesc0 baddesc badfile0 badfile
DE_M_TESTS =	nonexistent short badimagic badversion badmacrosp badendp
DE_M_TESTS +=	badnpages badnmacros badmacrop badfmagic

DO_TESTS =	${DO_MO_TESTS}
DOE_TESTS =	${DOE_M_TESTS} ${DOE_MOE_TESTS}
DE_TESTS =	${DE_M_TESTS}
DOB_TESTS =	${DO_TESTS} ${DOE_TESTS}
DEB_TESTS =	${DOE_TESTS} ${DE_TESTS}
MOB_TESTS =	${DO_MO_TESTS} ${D_MOE_TESTS} ${DOE_MOE_TESTS}
MEB_TESTS =	${D_MOE_TESTS} ${DOE_MOE_TESTS}
ALL_TESTS =	${DO_TESTS} ${DOE_TESTS} ${DE_TESTS}

REGRESS_TARGETS = ${ALL_TESTS} diff.derr diff.mout diff.merr

CLEANFILES =	${ALL_TESTS:C/$/.db/}
CLEANFILES +=	${ALL_TESTS:C/$/.dout/} ${ALL_TESTS:C/$/.derr/} all.derr
CLEANFILES +=	${MOB_TESTS:C/$/.mout/} all.mout
CLEANFILES +=	${MEB_TESTS:C/$/.merr/} all.merr
CLEANFILES +=	binedit dbm_dump makeinodes

# === MAIN REGRESSION TARGETS ==========================================

.for NAME in ${DO_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test \! -s ${NAME}.derr
	diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${DOE_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test -s ${NAME}.derr
	diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${DE_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test -s ${NAME}.derr
	test \! -s ${NAME}.dout
.endfor

diff.derr: all.derr
	diff -u ${.CURDIR}/../out/all.derr all.derr

diff.mout: all.mout
	diff -u ${.CURDIR}/../out/all.mout all.mout

diff.merr: all.merr
	diff -u ${.CURDIR}/../out/all.merr all.merr

.PHONY: ${REGRESS_TARGETS}


# === RUNNING DBM_DUMP =================================================

.for NAME in ${DOB_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr
.endfor

.for NAME in ${DE_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	if ./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr; \
		then false; else true; fi
.endfor

all.derr: ${DEB_TESTS:C/$/.derr/}
	for f in ${DEB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.derr; done > $@


# === RUNNING MAKEWHATIS AND MAN =======================================

empty.mout empty.merr:
	@echo creating $@
	mkdir -p empty.dir
	makewhatis empty.dir
	if man -kM empty.dir something > empty.mout 2> empty.merr; \
		then false; else test $$? -eq 5; fi

onepage.db onepage.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p onepage.dir/man1
	cp ${.CURDIR}/../man/empty.1 onepage.dir/man1/
	makewhatis onepage.dir
	man -kM onepage.dir title > onepage.mout
	cp onepage.dir/mandoc.db onepage.db

twopages.db twopages.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p twopages.dir/man1
	cp ${.CURDIR}/../man/empty.1 twopages.dir/man1/
	cp ${.CURDIR}/../man/empty.1 twopages.dir/man1/second.1
	makewhatis twopages.dir
	(echo "> second"; man -wM twopages.dir second; \
	  echo "> empty"; man -wM twopages.dir empty) \
	  | sed 's#.*/twopages\.dir/##' > twopages.mout
	cp twopages.dir/mandoc.db twopages.db

threepages.db threepages.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p threepages.dir/man1
	cp ${.CURDIR}/../man/empty.1 threepages.dir/man1/
	cp ${.CURDIR}/../man/empty.1 threepages.dir/man1/second.1
	cp ${.CURDIR}/../man/empty.1 threepages.dir/man1/third.1
	makewhatis threepages.dir
	(echo "> second"; man -wM threepages.dir second; \
	  echo "> third"; man -wM threepages.dir third; \
	  echo "> empty"; man -wM threepages.dir empty) \
	  | sed 's#.*/threepages.dir/##' > threepages.mout
	cp threepages.dir/mandoc.db threepages.db

threemacros.db threemacros.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p threemacros.dir/man1
	cp ${.CURDIR}/../man/empty.1 threemacros.dir/man1/
	printf ".Sh SEE ALSO\n.Xr one 1\n.Xr two 2\n.Xr three 3" \
		>> threemacros.dir/man1/empty.1
	makewhatis threemacros.dir
	(echo "> Xr=one"; \
	  man -kwM threemacros.dir Xr=one | sed 's#.*/threemacros\.dir/##'; \
	  echo "> Xr~^t"; man -kM threemacros.dir -O Xr Xr~^t) \
	  > threemacros.mout
	cp threemacros.dir/mandoc.db threemacros.db

sortpages.db sortpages.mout: makeinodes ${.CURDIR}/../man/empty.1
	@echo creating $@
	./makeinodes sortpages.dir
	cat ${.CURDIR}/../man/empty.1 >> sortpages.dir/man1/one.1
	cat ${.CURDIR}/../man/empty.1 >> sortpages.dir/man1/two.1
	makewhatis sortpages.dir
	man -kM sortpages.dir Nm=empty > sortpages.mout
	cp sortpages.dir/mandoc.db sortpages.db

sortpages_rev.db sortpages_rev.mout: makeinodes ${.CURDIR}/../man/empty.1
	@echo creating $@
	rm -rf sortpages_rev.dir
	./makeinodes -r sortpages_rev.dir
	cat ${.CURDIR}/../man/empty.1 >> sortpages_rev.dir/man1/one.1
	cat ${.CURDIR}/../man/empty.1 >> sortpages_rev.dir/man1/two.1
	makewhatis sortpages_rev.dir
	man -kM sortpages_rev.dir Nm=empty > sortpages_rev.mout
	cp sortpages_rev.dir/mandoc.db sortpages_rev.db

so.db so.mout: makeinodes ${.CURDIR}/../man/empty.1
	@echo creating $@
	./makeinodes so.dir
	cat ${.CURDIR}/../man/empty.1 >> so.dir/man1/one.1
	echo ".so man1/one.1" >> so.dir/man1/two.1
	makewhatis so.dir
	man -wM so.dir two | sed 's#.*/so\.dir/##' > so.mout
	cp so.dir/mandoc.db so.db

so_rev.db so_rev.mout: makeinodes ${.CURDIR}/../man/empty.1
	@echo creating $@
	./makeinodes -r so_rev.dir
	cat ${.CURDIR}/../man/empty.1 >> so_rev.dir/man1/one.1
	echo ".so man1/one.1" >> so_rev.dir/man1/two.1
	makewhatis so_rev.dir
	man -wM so_rev.dir two | sed 's#.*/so_rev\.dir/##' > so_rev.mout
	cp so_rev.dir/mandoc.db so_rev.db

sortnames.db sortnames.mout: ${.CURDIR}/../man/sortnames.1
	@echo creating $@
	mkdir -p sortnames.dir/man1
	cp ${.CURDIR}/../man/sortnames.1 sortnames.dir/man1/
	ln sortnames.dir/man1/sortnames.1 sortnames.dir/man1/link.1
	makewhatis sortnames.dir
	man -kM sortnames.dir Nm=onlysyn > sortnames.mout
	cp sortnames.dir/mandoc.db sortnames.db

twosect.db twosect.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p twosect.dir/man1
	mkdir -p twosect.dir/man8
	cp ${.CURDIR}/../man/empty.1 twosect.dir/man1/
	ln twosect.dir/man1/empty.1 twosect.dir/man8/empty.8
	makewhatis twosect.dir
	man -kM twosect.dir -s 1 title > twosect.mout
	cp twosect.dir/mandoc.db twosect.db

twoarch.db twoarch.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p twoarch.dir/man1/i386
	mkdir -p twoarch.dir/man1/amd64
	cp ${.CURDIR}/../man/empty.1 twoarch.dir/man1/i386
	ln twoarch.dir/man1/i386/empty.1 twoarch.dir/man1/amd64/empty.1
	makewhatis twoarch.dir
	man -kM twoarch.dir -S i386 title > twoarch.mout
	cp twoarch.dir/mandoc.db twoarch.db

threearch.db threearch.mout: ${.CURDIR}/../man/empty.1
	@echo creating $@
	mkdir -p threearch.dir/man1/amd64
	mkdir -p threearch.dir/man1/i386
	mkdir -p threearch.dir/man1/sgi
	cp ${.CURDIR}/../man/empty.1 threearch.dir/man1/amd64/
	cp ${.CURDIR}/../man/empty.1 threearch.dir/man1/i386/
	cp ${.CURDIR}/../man/empty.1 threearch.dir/man1/sgi/
	makewhatis threearch.dir
	(echo "> amd64"; man -wM threearch.dir -S amd64 empty; \
	  echo "> i386"; man -wM threearch.dir -S i386 empty; \
	  echo "> sgi"; man -wM threearch.dir -S sgi empty) \
	  | sed 's#.*/threearch\.dir/##' > threearch.mout
	cp threearch.dir/mandoc.db threearch.db

fn.db fn.mout: ${.CURDIR}/../man/fn.3
	@echo creating $@
	mkdir -p fn.dir/man3
	cp ${.CURDIR}/../man/fn.3 fn.dir/man3/
	makewhatis fn.dir
	(echo "> fn_type"; man -kwM fn.dir Ft=fn_type; \
	  echo "> fn_func"; man -kwM fn.dir Fn=fn_func; \
	  echo "> fn_arg"; man -kwM fn.dir Fa=fn_arg; \
	  echo "> fo_type"; man -kwM fn.dir Ft=fo_type; \
 	  echo "> fo_name"; man -kwM fn.dir Fn=fo_name; \
	  echo "> fo_arg"; man -kwM fn.dir Fa=fo_arg) \
	  | sed 's#.*/fn\.dir/##' > fn.mout
	cp fn.dir/mandoc.db fn.db

in.db in.mout: ${.CURDIR}/../man/in.3
	@echo creating $@
	mkdir -p in.dir/man3
	cp ${.CURDIR}/../man/in.3 in.dir/man3/
	makewhatis in.dir
	(echo "> In"; man -kwM in.dir In=in.h; \
	  echo "> Fd"; man -kwM in.dir In=fd.h) \
	  | sed 's#.*/in\.dir/##' > in.mout
	cp in.dir/mandoc.db in.db

va.db va.mout: ${.CURDIR}/../man/va.3
	@echo creating $@
	mkdir -p va.dir/man3
	cp ${.CURDIR}/../man/va.3 va.dir/man3/
	makewhatis va.dir
	(echo "> b2"; man -kwM va.dir Va='block vt_two'; \
	  echo "> t1"; man -kwM va.dir Vt='vt_one'; \
	  echo "> a1"; man -kwM va.dir Va='va_one'; \
	  echo "> t2"; man -kwM va.dir Vt='struct vt_two'; \
	  echo "> a2"; man -kwM va.dir Va='int va_two') \
	  | sed 's#.*/va\.dir/##' > va.mout
	cp va.dir/mandoc.db va.db

sh.db sh.mout: ${.CURDIR}/../man/sh.1
	@echo creating $@
	mkdir -p sh.dir/man1
	cp ${.CURDIR}/../man/sh.1 sh.dir/man1/
	makewhatis sh.dir
	(echo "> Sh"; man -kwM sh.dir Sh=sh_title; \
	  echo "> Ss"; man -kwM sh.dir Ss=ss_title) \
	  | sed 's#.*/sh\.dir/##' > sh.mout
	cp sh.dir/mandoc.db sh.db

xr.db xr.mout: ${.CURDIR}/../man/xr.1
	@echo creating $@
	mkdir -p xr.dir/man1
	cp ${.CURDIR}/../man/xr.1 xr.dir/man1/
	makewhatis xr.dir
	(echo "> bare"; man -kwM xr.dir 'Xr~^bare$$'; \
	  echo "> page"; man -kwM xr.dir 'Xr=page(1)') \
	  | sed 's#.*/xr\.dir/##' > xr.mout
	cp xr.dir/mandoc.db xr.db

badname0.mout badname0.merr: badname0.db
	@echo creating $@
	cp -pR onepage.dir badname0.dir
	cp badname0.db badname0.dir/mandoc.db
	man -wM badname0.dir empty > badname0.mout 2> badname0.merr
	sed -i 's#.*/badname0\.dir/##' badname0.mout
	sed -i 's#, run makewhatis.*##' badname0.merr

badname.mout badname.merr: badname.db
	@echo creating $@
	cp -pR threepages.dir badname.dir
	cp badname.db badname.dir/mandoc.db
	man -wM badname.dir empty > badname.mout 2> badname.merr
	sed -i 's#.*/badname\.dir/##' badname.mout
	sed -i 's#, run makewhatis.*##' badname.merr

badsect0.mout badsect0.merr: badsect0.db
	@echo creating $@
	cp -pR onepage.dir badsect0.dir
	cp badsect0.db badsect0.dir/mandoc.db
	man -wM badsect0.dir -s 1 empty > badsect0.mout 2> badsect0.merr
	sed -i 's#.*/badsect0\.dir/##' badsect0.mout
	sed -i 's#, run makewhatis.*##' badsect0.merr

badsect.mout badsect.merr: badsect.db
	@echo creating $@
	cp -pR threepages.dir badsect.dir
	cp badsect.db badsect.dir/mandoc.db
	man -wM badsect.dir -s 1 empty > badsect.mout 2> badsect.merr
	sed -i 's#.*/badsect\.dir/##' badsect.mout
	sed -i 's#, run makewhatis.*##' badsect.merr

badarch0.mout badarch0.merr: badarch0.db
	@echo creating $@
	cp -pR threearch.dir badarch0.dir
	cp badarch0.db badarch0.dir/mandoc.db
	man -wM badarch0.dir -S amd64 empty > badarch0.mout 2> badarch0.merr
	sed -i 's#.*/badarch0\.dir/##' badarch0.mout
	sed -i 's#, run makewhatis.*##' badarch0.merr

badarch.mout badarch.merr: badarch.db
	@echo creating $@
	cp -pR threearch.dir badarch.dir
	cp badarch.db badarch.dir/mandoc.db
	man -wM badarch.dir -S i386 empty > badarch.mout 2> badarch.merr
	sed -i 's#.*/badarch\.dir/##' badarch.mout
	sed -i 's#, run makewhatis.*##' badarch.merr

baddesc0.mout baddesc0.merr: baddesc0.db
	@echo creating $@
	cp -pR onepage.dir baddesc0.dir
	cp baddesc0.db baddesc0.dir/mandoc.db
	man -kM baddesc0.dir empty > baddesc0.mout 2> baddesc0.merr

baddesc.mout baddesc0.merr: baddesc.db
	@echo creating $@
	cp -pR threepages.dir baddesc.dir
	cp baddesc.db baddesc.dir/mandoc.db
	man -kM baddesc.dir empty > baddesc.mout 2> baddesc.merr

badfile0.mout badfile0.merr: badfile0.db
	@echo creating $@
	cp -pR onepage.dir badfile0.dir
	cp badfile0.db badfile0.dir/mandoc.db
	man -wM badfile0.dir empty > badfile0.mout 2> badfile0.merr
	sed -i 's#.*/badfile0\.dir/##' badfile0.mout
	sed -i 's#, run makewhatis.*##;s#/.*/mandoc/##' badfile0.merr

badfile.mout badfile.merr: badfile.db
	@echo creating $@
	cp -pR threepages.dir badfile.dir
	cp badfile.db badfile.dir/mandoc.db
	man -wM badfile.dir empty > badfile.mout 2> badfile.merr
	sed -i 's#.*/badfile\.dir/##' badfile.mout
	sed -i 's#, run makewhatis.*##;s#/.*/mandoc/##' badfile.merr

all.mout: ${MOB_TESTS:C/$/.mout/}
	for f in ${MOB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.mout || true; done > $@

all.merr: ${MEB_TESTS:C/$/.merr/}
	for f in ${MEB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.merr || true; done > $@


# === RUNNING BINEDIT ==================================================

padipage.db: binedit twopages.db
	./binedit c16rw1f < twopages.db > $@

padfpage.db: binedit twopages.db
	./binedit c16rw1s20c20w0w0w0w0w0f < twopages.db > $@

padmpage.db: binedit threepages.db
	./binedit c16rw2c20s20c20w0w0w0w0w0f < threepages.db > $@

badname0.db: binedit onepage.db
	./binedit c20rw-1f < onepage.db > $@

badname.db: binedit threepages.db
	./binedit c40rw-1f < threepages.db > $@

badsect0.db: binedit onepage.db
	./binedit c24rw-1f < onepage.db > $@

badsect.db: binedit threepages.db
	./binedit c44rw-1f < threepages.db > $@

badarch0.db: binedit threearch.db
	./binedit c28rw-1f < threearch.db > $@

badarch.db: binedit threearch.db
	./binedit c48rw-1f < threearch.db > $@

baddesc0.db: binedit onepage.db
	./binedit c32rw-1f < onepage.db > $@

baddesc.db: binedit threepages.db
	./binedit c52rw-1f < threepages.db > $@

badfile0.db: binedit onepage.db
	./binedit c36rw-1f < onepage.db > $@

badfile.db: binedit threepages.db
	./binedit c56rw-1f < threepages.db > $@

padmacros.db: binedit onepage.db
	./binedit c16rw0f < onepage.db > $@

padimacro.db: binedit onepage.db
	./binedit c8rdwri4wac4s4c136rwi4ww0f < onepage.db > $@

padmmacro.db: binedit onepage.db
	./binedit c8rdwri4wac8s4c132rwi4ww0f < onepage.db > $@

padientry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2f < threemacros.db > $@

padfentry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2s8c16w0w0f < threemacros.db > $@

padmentry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2c8s8c8w0w0f < threemacros.db > $@

padfmagic.db: binedit onepage.db
	./binedit c12rdi4waw0f < onepage.db > $@

nonexistent.db:
	rm -rf nonexistent.db

short.db:
	echo 'sho' > $@

badimagic.db: binedit onepage.db
	./binedit riwf < onepage.db > $@

badversion.db: binedit onepage.db
	./binedit c4riwf < onepage.db > $@

badmacrosp.db: binedit onepage.db
	./binedit c8rw1000f < onepage.db > $@

badendp.db: binedit onepage.db
	./binedit c12riwf < onepage.db > $@

badnpages.db: binedit onepage.db
	./binedit c16rw-1f < onepage.db > $@

badnmacros.db: binedit onepage.db
	./binedit c8rdwariwf < onepage.db > $@

badmacrop.db: binedit onepage.db
	./binedit c8rdwac4rw1000f < onepage.db > $@

badfmagic.db: binedit onepage.db
	./binedit c12rdwariwf < onepage.db > $@


# === GETTING ACCESS TO THE UTILITIES ==================================

binedit dbm_dump makeinodes:
	cd ${.CURDIR}/../$@ && make all
	ln -sf ${.OBJDIR}/../$@/$@ .

.include <bsd.regress.mk>

# must come after the .include: <bsd.prog.mk> contains .if !target(clean)
clean: clean_local

clean_local:
	rm -rf ${MOB_TESTS:C/$/.dir/}
