# $OpenBSD: Makefile,v 1.7 2021/09/02 07:14:15 jasper Exp $

# initial SUID handling bits taken from regress/sys/kern/setuid/

ALLOWED_MOUNTS = ${.OBJDIR} /tmp
.for d in ${ALLOWED_MOUNTS}
SUID_MOUNTS  +!= mount | grep ^$$(df -P $d | tail -1 | awk '{ print $$1 }') |\
    egrep -vw 'nosuid|noexec' | awk '{ print "$d" }' || true
.endfor

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
		doascmd="doas -C ${.CURDIR}/$@.conf -u $$ident $$cmdline"; \
		if id | grep -q '(wobj)'; then action=$$($$doascmd); \
		else action=$$(su ${BUILDUSER} -c "exec $$doascmd"); fi; \
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
. if empty(SUID_MOUNTS)
	@echo All of directories we are allowed to use for temporary data
	@echo "(${ALLOWED_MOUNTS})"
	@echo lie on nosuid filesystems, so we cannot run doas there.
	@echo SKIPPED
. else
	@echo '$@'
	@mnt=$$(echo '${SUID_MOUNTS}' | cut -d ' ' -f 1); \
	tdir=$$(mktemp -d $$mnt/$t.root.XXXXXXXX); \
	trap "${SUDO} rm -Rf $$tdir" EXIT; \
	chmod g+x $$tdir; \
	${SUDO} chgrp nobody $$tdir; \
	${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    $$tdir/etc; \
	${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    $$tdir/bin; \
	${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    $$tdir/usr/bin; \
	${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    $$tdir/usr/lib; \
	${SUDO} install -d -o ${BINOWN} -g ${BINGRP} -m ${DIRMODE} \
	    $$tdir/usr/libexec; \
	${SUDO} install -o root -g wheel -m 0444 \
	    ${.CURDIR}/$t.conf $$tdir/etc/doas.conf; \
	${SUDO} install -o root -g wheel -m 0400 \
	    ${.CURDIR}/master.passwd $$tdir/etc/master.passwd; \
	${SUDO} pwd_mkdb -d $$tdir/etc -p master.passwd; \
	${SUDO} install -o ${SHAREOWN} -g ${SHAREGRP} -m ${SHAREMODE} \
	    /usr/libexec/ld.so $$tdir/usr/libexec/ld.so; \
	${SUDO} install -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    /usr/lib/libc.so.* $$tdir/usr/lib; \
	${SUDO} install -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} \
	    /bin/echo $$tdir/bin/echo; \
	${SUDO} install -o ${BINOWN} -g ${BINGRP} -m 4555 \
	    /usr/bin/doas $$tdir/usr/bin/doas; \
	${SUDO} chroot -u nobody $$tdir /usr/bin/doas echo okay
. endif
.endfor

.include <bsd.regress.mk>
