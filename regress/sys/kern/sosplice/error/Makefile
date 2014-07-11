#	$OpenBSD: Makefile,v 1.2 2014/07/11 20:41:20 bluhm Exp $

.include <bsd.regress.mk>

# The error tests try to splice unsuitable sockets and check the
# kernel error code.

.for a in ${ARGS}
run-regress-$a: $a
	@echo '\n======== $@ ========'
.if empty (REMOTE_SSH)
	time perl ${PERLINC} ${PERLPATH}error.pl ${PERLPATH}$a
.else
	time perl ${PERLINC} ${PERLPATH}remote.pl ${REMOTE_SSH} ${PERLPATH}$a
.endif
.endfor
