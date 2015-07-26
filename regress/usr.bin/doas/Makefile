# $OpenBSD: Makefile,v 1.2 2015/07/26 17:31:18 zhuk Exp $

REGRESS_TARGETS = \
	t-okay \
	t-fail-quotes \
	t-permit-1

TEST_RUN_CMD =	doas -C ${.CURDIR}/$@.conf >$@.out 2>$@.err
TEST_ERRORS_CMD = \
	if [ -s $@.err -a ! -s ${.CURDIR}/$@.expected.err ]; then \
		echo "FAIL: unexpected error output:" >&2; \
		cat $@.err >&2; \
		exit 1; \
	elif [ -s ${.CURDIR}/$@.expected.err ]; then \
		diff -u ${.CURDIR}/$@.expected.err $@.err; \
	fi
TEST_OUTPUT_CMD = ${TEST_ERRORS_CMD:C/\.err/.out/:C/error //}

CLEANFILES +=	${REGRESS_TARGETS:=.out}
CLEANFILES +=	${REGRESS_TARGETS:=.err}

.for t in ${REGRESS_TARGETS:N*-fail*:N*-permit*}
${t}:
	@echo '$@'
	@${TEST_RUN_CMD}
	@${TEST_ERRORS_CMD}
	@${TEST_OUTPUT_CMD}
.endfor

.for t in ${REGRESS_TARGETS:M*-fail*}
${t}:
	@echo '$@'
	@ ! ${TEST_RUN_CMD}
	@${TEST_ERRORS_CMD}
	@${TEST_OUTPUT_CMD}
.endfor

.for t in ${REGRESS_TARGETS:M*-permit*}
${t}:
	@echo '$@'
	@rv=true; \
	while read ident cmdline; do \
		read expected; \
		set +e; \
		action=$$(doas -C ${.CURDIR}/$@.conf -u $$ident $$cmdline); \
		ret=$$?; \
		set -e; \
		if [ X"$$action" != X"$$expected" ]; then \
			echo "FAILED: expected '$$expected'," \
			    "but got '$$action'" >&2; \
			echo "        for command: $$cmdline" >&2; \
			rv=false; \
		fi; \
		if [ X"$$action" = Xdeny -a $$ret -eq 0 ]; then \
			echo "FAILED: deny without error return" >&2; \
			echo "        for command: $$cmdline" >&2; \
			rv=false; \
		elif [ X"$$action" != Xdeny -a $$ret -ne 0 ]; then \
			echo "FAILED: permit with error return" >&2; \
			echo "        for command: $$cmdline" >&2; \
			rv=false; \
		fi; \
	done <${.CURDIR}/$@.patterns; \
	$$rv
.endfor

.include <bsd.regress.mk>
