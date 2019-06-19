#	$OpenBSD: Makefile,v 1.6 2019/04/04 21:29:27 tb Exp $

PROGS += handshake_table
PROGS += valid_handshakes_terminate

.for p in ${PROGS}
REGRESS_TARGETS += run-$p
.endfor

LDADD =		${SSL_INT} -lcrypto
DPADD =		${LIBCRYPTO} ${LIBSSL}
WARNINGS =	Yes
CFLAGS +=	-DLIBRESSL_INTERNAL -Wundef -Werror -I$(BSDSRCDIR)/lib/libssl

print:	handshake_table
	@./handshake_table -C

handshake.gv: handshake_table
	./handshake_table -g > $@.tmp
	mv $@.tmp $@

CLEANFILES += handshake.gv

.for _FMT in png ps svg
handshake.${_FMT}: handshake.gv
	@if [ ! -x /usr/local/bin/dot ]; then			\
		echo "pkg_add graphviz to generate png";	\
		false;						\
	fi
	dot -T${_FMT} handshake.gv -o $@

CLEANFILES += handshake.${_FMT}
.endfor

.for p in ${PROGS}
run-$p: $p
	@echo '\n======== $@ ========'
	./$p

.PHONY: run-$p
.endfor

.PHONY: print

.include <bsd.regress.mk>
