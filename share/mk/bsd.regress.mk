# $OpenBSD: bsd.regress.mk,v 1.17 2018/12/03 22:30:04 bluhm Exp $
# Documented in bsd.regress.mk(5)

# No man pages for regression tests.
NOMAN=

# No installation.
install:

# If REGRESS_TARGETS is defined and PROG is not defined, set NOPROG
.if defined(REGRESS_TARGETS) && !defined(PROG)
NOPROG=
.endif

.include <bsd.prog.mk>

.MAIN: all
all: regress

# XXX - Need full path to REGRESS_LOG, otherwise there will be much pain.
REGRESS_LOG?=/dev/null
REGRESS_SKIP_TARGETS?=
REGRESS_SKIP_SLOW?=no
REGRESS_FAIL_EARLY?=no

_REGRESS_NAME=${.CURDIR:S/${BSDSRCDIR}\/regress\///}
_REGRESS_TMP?=/dev/null
_REGRESS_OUT= | tee -a ${REGRESS_LOG} ${_REGRESS_TMP} 2>&1 > /dev/null

.if defined(PROG) && !empty(PROG)
run-regress-${PROG}: ${PROG}
	./${PROG}
.endif

.if defined(PROG) && !defined(REGRESS_TARGETS)
REGRESS_TARGETS=run-regress-${PROG}
.  if defined(REGRESS_SKIP)
REGRESS_SKIP_TARGETS=run-regress-${PROG}
.  endif
.endif

.if defined(REGRESS_SLOW_TARGETS) && ${REGRESS_SKIP_SLOW} != no
REGRESS_SKIP_TARGETS+=${REGRESS_SLOW_TARGETS}
.endif

.if ${REGRESS_FAIL_EARLY} != no
_SKIP_FAIL=
.else
_SKIP_FAIL=-
.endif

.if defined(REGRESS_ROOT_TARGETS)
_ROOTUSER!=id -g
SUDO?=
.  if (${_ROOTUSER} != 0) && empty(SUDO)
REGRESS_SKIP_TARGETS+=${REGRESS_ROOT_TARGETS}
.  endif
.endif

.if defined(ERRORS)
.BEGIN:
.  for _m in ${ERRORS}
	@echo 1>&2 ${_m}
.  endfor
.  if !empty(ERRORS:M"Fatal\:*") || !empty(ERRORS:M'Fatal\:*')
	@exit 1
.  endif
.endif

REGRESS_SETUP?=
REGRESS_SETUP_ONCE?=
REGRESS_CLEANUP?=

.if !empty(REGRESS_SETUP)
${REGRESS_TARGETS}: ${REGRESS_SETUP}
.endif

.if !empty(REGRESS_SETUP_ONCE)
CLEANFILES+=${REGRESS_SETUP_ONCE:S/^/stamp-/}
${REGRESS_TARGETS}: ${REGRESS_SETUP_ONCE:S/^/stamp-/}
${REGRESS_SETUP_ONCE:S/^/stamp-/}: .SILENT
	${MAKE} -C ${.CURDIR} ${@:S/^stamp-//}
	date >$@
REGRESS_CLEANUP+=${REGRESS_SETUP_ONCE:S/^/cleanup-stamp-/}
${REGRESS_SETUP_ONCE:S/^/cleanup-stamp-/}: .SILENT
	rm -f ${@:S/^cleanup-//}
.endif

regress: .SILENT
.if ! ${REGRESS_LOG:M/*}
	echo =========================================================
	echo REGRESS_LOG must contain an absolute path to the log-file.
	echo It currently points to: ${REGRESS_LOG}
	echo =========================================================
	exit 1
.endif
.if !empty(REGRESS_SETUP_ONCE)
	rm -f ${REGRESS_SETUP_ONCE:S/^/stamp-/}
.endif
.for RT in ${REGRESS_TARGETS} ${REGRESS_CLEANUP}
.  if ${REGRESS_SKIP_TARGETS:M${RT}}
	@echo -n "SKIP " ${_REGRESS_OUT}
	@echo SKIPPED
.  else
# XXX - we need a better method to see if a test fails due to timeout or just
#       normal failure.
.   if !defined(REGRESS_MAXTIME)
	${_SKIP_FAIL}if cd ${.CURDIR} && ${MAKE} ${RT}; then \
	    echo -n "SUCCESS " ${_REGRESS_OUT} ; \
	else \
	    echo -n "FAIL " ${_REGRESS_OUT} ; \
	    echo FAILED ; \
	    false; \
	fi
.   else
	${_SKIP_FAIL}if cd ${.CURDIR} && \
	    (ulimit -t ${REGRESS_MAXTIME} ; ${MAKE} ${RT}); then \
	    echo -n "SUCCESS " ${_REGRESS_OUT} ; \
	else \
	    echo -n "FAIL (possible timeout) " ${_REGRESS_OUT} ; \
	    echo FAILED ; \
	    false; \
	fi
.   endif
.  endif
	@echo ${_REGRESS_NAME}/${RT:S/^run-regress-//} ${_REGRESS_OUT}
.endfor

.PHONY: regress
