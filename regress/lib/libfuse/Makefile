#       $OpenBSD: Makefile,v 1.1 2013/08/09 16:20:10 syl Exp $

REGRESS_TARGETS=  run-fuse-opt-add-opt
REGRESS_TARGETS+= run-fuse-opt-add-opt-escaped
REGRESS_TARGETS+= run-fuse-opt-add-arg
REGRESS_TARGETS+= run-fuse-opt-insert-arg
REGRESS_TARGETS+= run-fuse-opt-match

LDFLAGS+= -lfuse
CLEANFILES= fuse-opt-add-opt
CLEANFILES+=fuse-opt-add-opt-escaped
CLEANFILES+=fuse-opt-add-arg
CLEANFILES+=fuse-opt-insert-arg
CLEANFILES+=fuse-opt-match

.PHONY: ${REGRESS_TARGETS}

run-fuse-opt-add-opt: fuse-opt-add-opt
	./fuse-opt-add-opt
run-fuse-opt-add-opt-escaped: fuse-opt-add-opt-escaped
	./fuse-opt-add-opt-escaped
run-fuse-opt-add-arg: fuse-opt-add-arg
	./fuse-opt-add-arg
run-fuse-opt-insert-arg: fuse-opt-insert-arg
	./fuse-opt-insert-arg
run-fuse-opt-match: fuse-opt-match
	./fuse-opt-match

.include <bsd.regress.mk>
