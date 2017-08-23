# $OpenBSD: Makefile,v 1.7 2017/08/23 12:25:02 schwarze Exp $
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
	@test \! -s ${NAME}.derr
	@diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${DOE_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	@test -s ${NAME}.derr
	@diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${DE_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	@test -s ${NAME}.derr
	@test \! -s ${NAME}.dout
.endfor

diff.derr: all.derr
	@diff -u ${.CURDIR}/../out/all.derr all.derr

diff.mout: all.mout
	@diff -u ${.CURDIR}/../out/all.mout all.mout

diff.merr: all.merr
	@diff -u ${.CURDIR}/../out/all.merr all.merr

.PHONY: ${REGRESS_TARGETS}

cleandir: cleandir_local

cleandir_local:
	rm -rf man onepage.dir twopages.dir threepages.dir \
		threemacros.dir threearch.dir


# === RUNNING DBM_DUMP =================================================

.for NAME in ${DOB_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	@./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr
.endfor

.for NAME in ${DE_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	@if ./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr; \
		then false; else true; fi
.endfor

all.derr: ${DEB_TESTS:C/$/.derr/}
	@for f in ${DEB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.derr; done > $@


# === RUNNING MAKEWHATIS AND MAN =======================================

empty.mout empty.merr:
	@rm -rf man
	@mkdir man
	@makewhatis man
	@if man -kM man something > empty.mout 2> empty.merr; \
		then false; else test $$? -eq 5; fi
	@rmdir man

onepage.db onepage.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@makewhatis man
	@man -kM man title > onepage.mout
	@cp man/mandoc.db onepage.db
	@rm -rf onepage.dir
	@mv man onepage.dir

twopages.db twopages.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@cp ${.CURDIR}/../man/empty.1 man/man1/second.1
	@makewhatis man
	@(echo "> second"; man -wM man second | sed 's#.*/man/##'; \
	  echo "> empty"; man -wM man empty | sed 's#.*/man/##') \
	  > twopages.mout
	@cp man/mandoc.db twopages.db
	@rm -rf twopages.dir
	@mv man twopages.dir

threepages.db threepages.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@cp ${.CURDIR}/../man/empty.1 man/man1/second.1
	@cp ${.CURDIR}/../man/empty.1 man/man1/third.1
	@makewhatis man
	@(echo "> second"; man -wM man second | sed 's#.*/man/##'; \
	  echo "> third"; man -wM man third | sed 's#.*/man/##'; \
	  echo "> empty"; man -wM man empty | sed 's#.*/man/##') \
	  > threepages.mout
	@cp man/mandoc.db threepages.db
	@rm -rf threepages.dir
	@mv man threepages.dir

threemacros.db threemacros.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@printf ".Sh SEE ALSO\n.Xr one 1\n.Xr two 2\n.Xr three 3" \
		>> man/man1/empty.1
	@makewhatis man
	@(echo "> Xr=one"; man -kwM man Xr=one | sed 's#.*/man/##'; \
	  echo "> Xr~^t"; man -kM man -O Xr Xr~^t;) > threemacros.mout
	@cp man/mandoc.db threemacros.db
	@rm -rf threemacros.dir
	@mv man threemacros.dir

sortpages.db sortpages.mout: makeinodes ${.CURDIR}/../man/empty.1
	@rm -rf man
	@./makeinodes
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/1
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/2
	@mv man/man1/1 man/man1/one.1
	@mv man/man1/2 man/man1/two.1
	@makewhatis man
	@man -kM man Nm=empty > sortpages.mout
	@mv man/mandoc.db sortpages.db
	@rm -rf man

sortpages_rev.db sortpages_rev.mout: makeinodes ${.CURDIR}/../man/empty.1
	@rm -rf man
	@./makeinodes
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/1
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/2
	@mv man/man1/2 man/man1/one.1
	@mv man/man1/1 man/man1/two.1
	@makewhatis man
	@man -kM man Nm=empty > sortpages_rev.mout
	@mv man/mandoc.db sortpages_rev.db
	@rm -rf man

so.db so.mout: makeinodes ${.CURDIR}/../man/empty.1
	@rm -rf man
	@./makeinodes
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/1
	@echo ".so man1/one.1" >> man/man1/2
	@mv man/man1/1 man/man1/one.1
	@mv man/man1/2 man/man1/two.1
	@makewhatis man
	@man -wM man two | sed 's#.*/man/##' > so.mout
	@mv man/mandoc.db so.db
	@rm -rf man

so_rev.db so_rev.mout: makeinodes ${.CURDIR}/../man/empty.1
	@rm -rf man
	@./makeinodes
	@cat ${.CURDIR}/../man/empty.1 >> man/man1/2
	@echo ".so man1/one.1" >> man/man1/1
	@mv man/man1/2 man/man1/one.1
	@mv man/man1/1 man/man1/two.1
	@makewhatis man
	@man -wM man two | sed 's#.*/man/##' > so_rev.mout
	@mv man/mandoc.db so_rev.db
	@rm -rf man

sortnames.db sortnames.mout: ${.CURDIR}/../man/sortnames.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/sortnames.1 man/man1/
	@ln man/man1/sortnames.1 man/man1/link.1
	@makewhatis man
	@man -kM man Nm=onlysyn > sortnames.mout
	@mv man/mandoc.db sortnames.db
	@rm -rf man

twosect.db twosect.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@mkdir -p man/man8
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@ln man/man1/empty.1 man/man8/empty.8
	@makewhatis man
	@man -kM man -s 1 title > twosect.mout
	@mv man/mandoc.db twosect.db
	@rm -rf man

twoarch.db twoarch.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1/i386
	@mkdir -p man/man1/amd64
	@cp ${.CURDIR}/../man/empty.1 man/man1/i386
	@ln man/man1/i386/empty.1 man/man1/amd64/empty.1
	@makewhatis man
	@man -kM man -S i386 title > twoarch.mout
	@mv man/mandoc.db twoarch.db
	@rm -rf man

threearch.db threearch.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1/amd64
	@mkdir -p man/man1/i386
	@mkdir -p man/man1/sgi
	@cp ${.CURDIR}/../man/empty.1 man/man1/amd64/
	@cp ${.CURDIR}/../man/empty.1 man/man1/i386/
	@cp ${.CURDIR}/../man/empty.1 man/man1/sgi/
	@makewhatis man
	@(echo "> amd64"; man -wM man -S amd64 empty | sed 's#.*/man/##'; \
	  echo "> i386"; man -wM man -S i386 empty | sed 's#.*/man/##'; \
	  echo "> sgi"; man -wM man -S sgi empty | sed 's#.*/man/##') \
	  > threearch.mout
	@cp man/mandoc.db threearch.db
	@rm -rf threearch.dir
	@mv man threearch.dir

fn.db fn.mout: ${.CURDIR}/../man/fn.3
	@rm -rf man
	@mkdir -p man/man3
	@cp ${.CURDIR}/../man/fn.3 man/man3/
	@makewhatis man
	@(echo "> fn_type"; man -kwM man Ft=fn_type | sed 's#.*/man/##'; \
	  echo "> fn_func"; man -kwM man Fn=fn_func | sed 's#.*/man/##'; \
	  echo "> fn_arg";  man -kwM man Fa=fn_arg  | sed 's#.*/man/##'; \
	  echo "> fo_type"; man -kwM man Ft=fo_type | sed 's#.*/man/##'; \
	  echo "> fo_func"; man -kwM man Fn=fo_func 2>&1 | sed 's#.*/man/##'; \
	  echo "> fo_arg";  man -kwM man Fa=fo_arg  | sed 's#.*/man/##') \
	  > fn.mout
	@mv man/mandoc.db fn.db
	@rm -rf man

in.db in.mout: ${.CURDIR}/../man/in.3
	@rm -rf man
	@mkdir -p man/man3
	@cp ${.CURDIR}/../man/in.3 man/man3/
	@makewhatis man
	@(echo "> In"; man -kwM man In=in.h | sed 's#.*/man/##'; \
	  echo "> Fd"; man -kwM man In=fd.h | sed 's#.*/man/##') \
	  > in.mout
	@mv man/mandoc.db in.db
	@rm -rf man

va.db va.mout: ${.CURDIR}/../man/va.3
	@rm -rf man
	@mkdir -p man/man3
	@cp ${.CURDIR}/../man/va.3 man/man3/
	@makewhatis man
	@(echo "> b2"; man -kwM man Va='block vt_two' | sed 's#.*/man/##'; \
	  echo "> t1"; man -kwM man Vt='vt_one' | sed 's#.*/man/##'; \
	  echo "> a1"; man -kwM man Va='va_one'  | sed 's#.*/man/##'; \
	  echo "> t2"; man -kwM man Vt='struct vt_two' | sed 's#.*/man/##'; \
	  echo "> a2"; man -kwM man Va='int va_two'  | sed 's#.*/man/##') \
	  > va.mout
	@mv man/mandoc.db va.db
	@rm -rf man

sh.db sh.mout: ${.CURDIR}/../man/sh.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/sh.1 man/man1/
	@makewhatis man
	@(echo "> Sh"; man -kwM man Sh=sh_title | sed 's#.*/man/##'; \
	  echo "> Ss"; man -kwM man Ss=ss_title | sed 's#.*/man/##') \
	  > sh.mout
	@mv man/mandoc.db sh.db
	@rm -rf man

xr.db xr.mout: ${.CURDIR}/../man/xr.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/xr.1 man/man1/
	@makewhatis man
	@(echo "> bare"; man -kwM man 'Xr~^bare$$' | sed 's#.*/man/##'; \
	  echo "> page"; man -kwM man 'Xr=page(1)' | sed 's#.*/man/##') \
	  > xr.mout
	@mv man/mandoc.db xr.db
	@rm -rf man

badname0.mout badname0.merr: badname0.db
	@rm -rf man
	@cp -pR onepage.dir man
	@cp badname0.db man/mandoc.db
	@man -wM man empty > badname0.mout 2> badname0.merr
	@rm -rf man

badname.mout badname.merr: badname.db
	@rm -rf man
	@cp -pR threepages.dir man
	@cp badname.db man/mandoc.db
	@man -wM man empty > badname.mout 2> badname.merr
	@rm -rf man

badsect0.mout badsect0.merr: badsect0.db
	@rm -rf man
	@cp -pR onepage.dir man
	@cp badsect0.db man/mandoc.db
	@man -wM man -s 1 empty > badsect0.mout 2> badsect0.merr
	@rm -rf man

badsect.mout badsect.merr: badsect.db
	@rm -rf man
	@cp -pR threepages.dir man
	@cp badsect.db man/mandoc.db
	@man -wM man -s 1 empty > badsect.mout 2> badsect.merr
	@rm -rf man

badarch0.mout badarch0.merr: badarch0.db
	@rm -rf man
	@cp -pR threearch.dir man
	@cp badarch0.db man/mandoc.db
	@man -wM man -S amd64 empty > badarch0.mout 2> badarch0.merr
	@rm -rf man

badarch.mout badarch.merr: badarch.db
	@rm -rf man
	@cp -pR threearch.dir man
	@cp badarch.db man/mandoc.db
	@man -wM man -S i386 empty > badarch.mout 2> badarch.merr
	@rm -rf man

baddesc0.mout baddesc0.merr: baddesc0.db
	@rm -rf man
	@cp -pR onepage.dir man
	@cp baddesc0.db man/mandoc.db
	@man -kM man empty > baddesc0.mout 2> baddesc0.merr
	@rm -rf man

baddesc.mout baddesc0.merr: baddesc.db
	@rm -rf man
	@cp -pR threepages.dir man
	@cp baddesc.db man/mandoc.db
	@man -kM man empty > baddesc.mout 2> baddesc.merr
	@rm -rf man

badfile0.mout badfile0.merr: badfile0.db
	@rm -rf man
	@cp -pR onepage.dir man
	@cp badfile0.db man/mandoc.db
	@man -wM man empty > badfile0.mout 2> badfile0.merr
	@rm -rf man

badfile.mout badfile.merr: badfile.db
	@rm -rf man
	@cp -pR threepages.dir man
	@cp badfile.db man/mandoc.db
	@man -wM man empty > badfile.mout 2> badfile.merr
	@rm -rf man

all.mout: ${MOB_TESTS:C/$/.mout/}
	@for f in ${MOB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.mout || true; done > $@

all.merr: ${MEB_TESTS:C/$/.merr/}
	@for f in ${MEB_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.merr || true; done > $@


# === RUNNING BINEDIT ==================================================

padipage.db: binedit twopages.db
	@./binedit c16rw1f < twopages.db > $@

padfpage.db: binedit twopages.db
	@./binedit c16rw1s20c20w0w0w0w0w0f < twopages.db > $@

padmpage.db: binedit threepages.db
	@./binedit c16rw2c20s20c20w0w0w0w0w0f < threepages.db > $@

badname0.db: binedit onepage.db
	@./binedit c20rw-1f < onepage.db > $@

badname.db: binedit threepages.db
	@./binedit c40rw-1f < threepages.db > $@

badsect0.db: binedit onepage.db
	@./binedit c24rw-1f < onepage.db > $@

badsect.db: binedit threepages.db
	@./binedit c44rw-1f < threepages.db > $@

badarch0.db: binedit threearch.db
	@./binedit c28rw-1f < threearch.db > $@

badarch.db: binedit threearch.db
	@./binedit c48rw-1f < threearch.db > $@

baddesc0.db: binedit onepage.db
	@./binedit c32rw-1f < onepage.db > $@

baddesc.db: binedit threepages.db
	@./binedit c52rw-1f < threepages.db > $@

badfile0.db: binedit onepage.db
	@./binedit c36rw-1f < onepage.db > $@

badfile.db: binedit threepages.db
	@./binedit c56rw-1f < threepages.db > $@

padmacros.db: binedit onepage.db
	@./binedit c16rw0f < onepage.db > $@

padimacro.db: binedit onepage.db
	@./binedit c8rdwri4wac4s4c136rwi4ww0f < onepage.db > $@

padmmacro.db: binedit onepage.db
	@./binedit c8rdwri4wac8s4c132rwi4ww0f < onepage.db > $@

padientry.db: binedit threemacros.db
	@./binedit c8rdwac4rdwarw2f < threemacros.db > $@

padfentry.db: binedit threemacros.db
	@./binedit c8rdwac4rdwarw2s8c16w0w0f < threemacros.db > $@

padmentry.db: binedit threemacros.db
	@./binedit c8rdwac4rdwarw2c8s8c8w0w0f < threemacros.db > $@

padfmagic.db: binedit onepage.db
	@./binedit c12rdi4waw0f < onepage.db > $@

nonexistent.db:
	@rm -rf nonexistent.db

short.db:
	@echo 'sho' > $@

badimagic.db: binedit onepage.db
	@./binedit riwf < onepage.db > $@

badversion.db: binedit onepage.db
	@./binedit c4riwf < onepage.db > $@

badmacrosp.db: binedit onepage.db
	@./binedit c8rw1000f < onepage.db > $@

badendp.db: binedit onepage.db
	@./binedit c12riwf < onepage.db > $@

badnpages.db: binedit onepage.db
	@./binedit c16rw-1f < onepage.db > $@

badnmacros.db: binedit onepage.db
	@./binedit c8rdwariwf < onepage.db > $@

badmacrop.db: binedit onepage.db
	@./binedit c8rdwac4rw1000f < onepage.db > $@

badfmagic.db: binedit onepage.db
	@./binedit c12rdwariwf < onepage.db > $@


# === GETTING ACCESS TO THE UTILITIES ==================================

binedit dbm_dump makeinodes:
	cd ${.CURDIR}/../$@ && make all
	ln -sf ${.OBJDIR}/../$@/$@ .

.include <bsd.regress.mk>
