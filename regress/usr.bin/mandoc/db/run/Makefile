# $OpenBSD: Makefile,v 1.2 2016/08/04 09:18:53 schwarze Exp $
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

OUT_TESTS =	empty onepage twopages threepages threemacros
OUT_TESTS +=	sortpages sortpages_rev so so_rev sortnames twosect twoarch
OUT_TESTS +=	fn in va sh xr
BOTH_TESTS =	padipage padfpage padmpage padmacros padimacro padmmacro
BOTH_TESTS +=	padientry padfentry padmentry padfmagic
ERR_TESTS =	nonexistent short badimagic badversion badmacrosp badendp
ERR_TESTS +=	badnpages badnmacros badmacrop badfmagic
ALL_TESTS =	${OUT_TESTS} ${BOTH_TESTS} ${ERR_TESTS}

REGRESS_TARGETS = ${ALL_TESTS} diff.derr diff.mout diff.merr

CLEANFILES =	${ALL_TESTS:C/$/.db/}
CLEANFILES +=	${ALL_TESTS:C/$/.dout/} ${ALL_TESTS:C/$/.derr/} all.derr
CLEANFILES +=	${OUT_TESTS:C/$/.mout/} all.mout empty.merr

# === MAIN REGRESSION TARGETS ==========================================

.for NAME in ${OUT_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test \! -s ${NAME}.derr
	@echo diffing ${NAME}.dout
	@diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${BOTH_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test -s ${NAME}.derr
	@echo diffing ${NAME}.dout
	@diff -u ${.CURDIR}/../out/${NAME}.dout ${NAME}.dout
.endfor

.for NAME in ${ERR_TESTS}
${NAME}: ${NAME}.dout ${NAME}.derr
	test -s ${NAME}.derr
	test \! -s ${NAME}.dout
.endfor

diff.derr: all.derr
	@echo diffing all.derr
	@diff -u ${.CURDIR}/../out/all.derr all.derr

diff.mout: all.mout
	@echo diffing all.mout
	@diff -u ${.CURDIR}/../out/all.mout all.mout

diff.merr: empty.merr
	@echo diffing empty.merr
	@diff -u ${.CURDIR}/../out/empty.merr empty.merr

.PHONY: ${REGRESS_TARGETS}

cleandir: cleandir_local

cleandir_local:
	rm -rf man binedit dbm_dump makeinodes


# === RUNNING DBM_DUMP =================================================

.for NAME in ${OUT_TESTS} ${BOTH_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr
.endfor

.for NAME in ${ERR_TESTS}
${NAME}.dout ${NAME}.derr: dbm_dump ${NAME}.db
	@echo dumping ${NAME}.db
	@if ./dbm_dump ${NAME}.db > ${NAME}.dout 2> ${NAME}.derr; \
		then false; else true; fi
.endfor

all.derr: ${BOTH_TESTS:C/$/.derr/} ${ERR_TESTS:C/$/.derr/}
	@echo assembling $@
	@for f in ${BOTH_TESTS} ${ERR_TESTS}; \
		do printf "\n>>> $$f\n"; cat $$f.derr; done > $@


# === RUNNING MAKEWHATIS AND MAN =======================================

empty.db empty.mout empty.merr:
	@echo making empty.db
	@rm -rf man
	@mkdir man
	@makewhatis man
	@if man -kM man something > empty.mout 2> empty.merr; \
		then false; else test $$? -eq 5; fi
	@mv man/mandoc.db empty.db
	@rmdir man

onepage.db onepage.mout: ${.CURDIR}/../man/empty.1
	@echo making onepage.db
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@makewhatis man
	@man -kM man title > onepage.mout
	@mv man/mandoc.db onepage.db
	@rm -rf man

twopages.db twopages.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@cp ${.CURDIR}/../man/empty.1 man/man1/second.1
	@makewhatis man
	@(echo "> second"; man -wM man second | sed 's#.*/man/##'; \
	  echo "> empty"; man -wM man empty | sed 's#.*/man/##') \
	  > twopages.mout
	@mv man/mandoc.db twopages.db
	@rm -rf man

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
	@mv man/mandoc.db threepages.db
	@rm -rf man

threemacros.db threemacros.mout: ${.CURDIR}/../man/empty.1
	@rm -rf man
	@mkdir -p man/man1
	@cp ${.CURDIR}/../man/empty.1 man/man1/
	@printf ".Sh SEE ALSO\n.Xr one 1\n.Xr two 2\n.Xr three 3" \
		>> man/man1/empty.1
	@makewhatis man
	@(echo "> Xr=one"; man -kwM man Xr=one | sed 's#.*/man/##'; \
	  echo "> Xr~^t"; man -kM man -O Xr Xr~^t;) > threemacros.mout
	@mv man/mandoc.db threemacros.db
	@rm -rf man

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

fn.db fn.mout: ${.CURDIR}/../man/fn.3
	@rm -rf man
	@mkdir -p man/man3
	@cp ${.CURDIR}/../man/fn.3 man/man3/
	@makewhatis man
	@(echo "> fn_type"; man -kwM man Ft=fn_type | sed 's#.*/man/##'; \
	  echo "> fn_func"; man -kwM man Fn=fn_func | sed 's#.*/man/##'; \
	  echo "> fn_arg";  man -kwM man Fa=fn_arg  | sed 's#.*/man/##'; \
	  echo "> fo_type"; man -kwM man Ft=fo_type | sed 's#.*/man/##'; \
	  echo "> fo_func"; man -kwM man Fn=fo_func | sed 's#.*/man/##'; \
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

all.mout: ${OUT_TESTS:C/$/.mout/}
	@echo assembling all.mout
	@for f in ${OUT_TESTS}; do printf "\n>>> $$f\n"; \
		test -e $$f.mout && cat $$f.mout || true; done > $@


# === RUNNING BINEDIT ==================================================

padipage.db: binedit twopages.db
	./binedit c16rw1f < twopages.db > $@

padfpage.db: binedit twopages.db
	./binedit c16rw1s20c20w0w0w0w0w0f < twopages.db > $@

padmpage.db: binedit threepages.db
	./binedit c16rw2c20s20c20w0w0w0w0w0f < threepages.db > $@

padmacros.db: binedit onepage.db
	./binedit c16rw0f < onepage.db > $@

padimacro.db: binedit empty.db
	./binedit c8rdwri4wac4s4c136rwi4ww0f < empty.db > $@

padmmacro.db: binedit empty.db
	./binedit c8rdwri4wac8s4c132rwi4ww0f < empty.db > $@

padientry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2f < threemacros.db > $@

padfentry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2s8c16w0w0f < threemacros.db > $@

padmentry.db: binedit threemacros.db
	./binedit c8rdwac4rdwarw2c8s8c8w0w0f < threemacros.db > $@

padfmagic.db: binedit empty.db
	./binedit c12rdi4waw0f < empty.db > $@

nonexistent.db:
	rm -rf nonexistent.db

short.db:
	echo 'sho' > $@

badimagic.db: binedit empty.db
	./binedit riwf < empty.db > $@

badversion.db: binedit empty.db
	./binedit c4riwf < empty.db > $@

badmacrosp.db: binedit empty.db
	./binedit c8rw1000f < empty.db > $@

badendp.db: binedit empty.db
	./binedit c12riwf < empty.db > $@

badnpages.db: binedit empty.db
	./binedit c16rw-1f < empty.db > $@

badnmacros.db: binedit empty.db
	./binedit c8rdwariwf < empty.db > $@

badmacrop.db: binedit empty.db
	./binedit c8rdwac4rw1000f < empty.db > $@

badfmagic.db: binedit empty.db
	./binedit c12rdwariwf < empty.db > $@


# === GETTING ACCESS TO THE UTILITIES ==================================

binedit dbm_dump makeinodes:
	@echo installing $@
	@if [ -x ${.CURDIR}/../$@/obj/$@ ]; \
		then ln -s ${.CURDIR}/../$@/obj/$@; \
		elif [ -x ${.CURDIR}/../$@/$@ ]; \
		then ln -s ${.CURDIR}/../$@/$@; \
		else echo "Cannot find $@ binary; run 'make' in .." 1>&2; \
		exit 1; fi

.include <bsd.regress.mk>
