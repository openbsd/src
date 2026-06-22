# $OpenBSD: Makefile,v 1.1 2026/06/22 15:52:51 tim Exp $

REGRESS_TARGETS=	t.0 t.0a t.1 t.1.x t.2 t.2.x t.3 t.3.x t.4 t.4.x \
			t.5.x t.6 t.6.x t.6a t.6b t.8.x t.8.y t.NF t.a \
			t.addops t.aeiou t.aeiouy t.arith t.array t.array1 \
			t.array2 t.assert t.avg t.b.x t.be t.beginexit \
			t.beginnext t.break t.break1 t.break2 t.break3 t.bug1 \
			t.builtins t.cat t.cat1 t.cat2 t.cmp t.coerce \
			t.coerce2 t.comment t.comment1 t.concat t.cond \
			t.contin t.count t.crlf t.cum t.d.x t.delete0 \
			t.delete1 t.delete2 t.delete3 t.do t.e t.else t.exit1 \
			t.f t.f.x t.f0 t.f1 t.f2 t.f3 t.f4 t.for t.for1 \
			t.for2 t.for3 t.format4 t.fun t.fun0 t.fun1 t.fun2 \
			t.fun3 t.fun4 t.fun5 t.getline1 t.getval t.gsub \
			t.gsub1 t.gsub3 t.gsub4 t.i.x t.if t.in t.in1 t.in2 \
			t.in3 t.incr t.incr2 t.incr3 t.index t.intest \
			t.intest2 t.j.x t.longstr t.makef t.match t.match1 \
			t.max t.mod t.monotone t.nameval t.next t.not t.null0 \
			t.ofmt t.ofs t.ors t.pat t.pipe t.pp t.pp1 t.pp2 \
			t.printf t.printf2 t.quote t.randk t.re1 t.re1a t.re2 \
			t.re3 t.re4 t.re5 t.re7 t.reFS t.rec t.redir1 t.reg \
			t.roff t.sep t.seqno t.set0 t.set0a t.set0b t.set1 \
			t.set2 t.set3 t.split1 t.split2 t.split2a t.split3 \
			t.split4 t.split8 t.split9 t.split9a t.stately \
			t.strcmp t.strcmp1 t.strnum t.sub0 t.sub1 t.sub2 \
			t.sub3 t.substr t.substr1 t.time t.vf t.vf1 t.vf2 \
			t.vf3 t.x

CLEANFILES=		*.out
TESTFILE=		${.CURDIR}/../test.data

# XXX Needed for t.a
.SUFFIXES:

.SUFFIXES: .awk

.awk:
	LC_CTYPE=en_US.UTF-8 ${AWK} -f $< ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out

t.redir1:
	${AWK} -f ${.CURDIR}/$@.awk ${TESTFILE} >$@.out
	cmp -s ${.CURDIR}/$@.ok $@.out
	cmp -s ${.CURDIR}/$@.even.ok $@.even.out
	cmp -s ${.CURDIR}/$@.odd.ok $@.odd.out

.include <bsd.regress.mk>
