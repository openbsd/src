#	$OpenBSD: Makefile,v 1.4 2019/01/24 16:32:29 tb Exp $

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

.for _FMT in png svg ps
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
.endfor

.include <bsd.regress.mk>
