#	$OpenBSD: Makefile,v 1.3 2019/07/12 19:31:29 bluhm Exp $

.include <bsd.regress.mk>

# The error tests try to splice unsuitable sockets and check the
# kernel error code.

.for a in ${ARGS}
run-$a: $a
	@echo '\n======== $@ ========'
.if empty (REMOTE_SSH)
	time perl ${PERLINC} ${PERLPATH}error.pl ${PERLPATH}$a
.else
	time perl ${PERLINC} ${PERLPATH}remote.pl ${REMOTE_SSH} ${PERLPATH}$a
.endif
.endfor
