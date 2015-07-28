# $OpenBSD: Makefile,v 1.3 2015/07/28 12:25:27 zhuk Exp $

REGRESS_TARGETS = \
	t-okay \
	t-fail-quotes \
	t-permit-1 \
	t-run-keepenv-path

REGRESS_ROOT_TARGETS =	${REGRESS_TARGETS:M*-run*}

TEST_CONFIG_CMD =	doas -C ${.CURDIR}/$@.conf >$@.out 2>$@.err
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

.for t in ${REGRESS_TARGETS:N*-fail*:N*-permit*:N*-run*}
${t}:
	@echo '$@'
	@${TEST_CONFIG_CMD}
	@${TEST_ERRORS_CMD}
	@${TEST_OUTPUT_CMD}
.endfor

.for t in ${REGRESS_TARGETS:M*-fail*}
${t}:
	@echo '$@'
	@ ! ${TEST_CONFIG_CMD}
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

.for t in ${REGRESS_TARGETS:M*-run*}
${t}:
	@echo '$@'
	@${SUDO} rm -Rf $t.root
	@${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} $t.root/etc
	@${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} $t.root/bin
	@${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} $t.root/usr/bin
	@${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} $t.root/usr/lib
	@${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} $t.root/usr/libexec
	@${SUDO} install -o root -g wheel -m 0444 \
	    ${.CURDIR}/$t.conf $t.root/etc/doas.conf
	@${SUDO} install -o root -g wheel -m 0400 \
	    ${.CURDIR}/master.passwd $t.root/etc/master.passwd
	@${SUDO} pwd_mkdb -d $t.root/etc -p master.passwd
	@${SUDO} install -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
	    /usr/libexec/ld.so $t.root/usr/libexec/ld.so
	@${SUDO} install -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    /usr/lib/libc.so.* $t.root/usr/lib
	@${SUDO} install -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    /bin/echo $t.root/bin/echo
	@${SUDO} install -o ${BINOWN} -g ${BINGRP} -m 4555 \
	    /usr/bin/doas $t.root/usr/bin/doas
	@${SUDO} env MALLOC_OPTIONS=S chroot -u nobody $t.root /usr/bin/doas echo okay
.endfor
# cleanup copied files
afterclean: cleanroots
cleanroots:
.for t in ${REGRESS_TARGETS:M*-run*}
	${SUDO} rm -Rf $t.root
.endfor

.include <bsd.regress.mk>
