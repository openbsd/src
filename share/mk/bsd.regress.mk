# $OpenBSD: bsd.regress.mk,v 1.10 2002/09/02 19:56:55 avsm Exp $
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

# Check for deprecated REGRESS* variables and assign them to the
# new versions if the new version is not already defined. 
_REGRESS_DEPRECATED=LOG:LOG SKIPTARGETS:SKIP_TARGETS SKIPSLOW:SKIP_SLOW \
	SKIP:SKIP TARGETS:TARGETS MAXTIME:MAXTIME ROOTTARGETS:ROOT_TARGETS

.for _I in ${_REGRESS_DEPRECATED}
_REGRESS_OLD=REGRESS${_I:C/\:.*//}
_REGRESS_NEW=REGRESS_${_I:C/.*\://}
.  if defined(${_REGRESS_OLD})
ERRORS:= ${ERRORS} "Warning: ${_REGRESS_OLD} is deprecated, use ${_REGRESS_NEW} instead."
.    if !defined(${_REGRESS_NEW})
${_REGRESS_NEW}:=${${_REGRESS_OLD}}
.    endif
.  endif
.endfor

# XXX - Need full path to REGRESS_LOG, otherwise there will be much pain.
REGRESS_LOG?=/dev/null
REGRESS_SKIP_TARGETS?=
REGRESS_SKIP_SLOW?=no

_REGRESS_NAME=${.CURDIR:S/${BSDSRCDIR}\/regress\///}
_REGRESS_TMP?=/dev/null
_REGRESS_OUT= | tee -a ${REGRESS_LOG} ${_REGRESS_TMP} 2>&1 > /dev/null

.if defined(PROG) && !empty(PROG)
run-regress-${PROG}: ${PROG}
	./${PROG}
.endif

.if !defined(REGRESS_TARGETS)
REGRESS_TARGETS=run-regress-${PROG}
.  if defined(REGRESS_SKIP)
REGRESS_SKIP_TARGETS=run-regress-${PROG}
.  endif
.endif

.if defined(REGRESS_SLOW_TARGETS) && !empty(REGRESS_SKIP_SLOW)
REGRESS_SKIP_TARGETS+=${REGRESS_SLOW_TARGETS}
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

regress: .SILENT
.if ! ${REGRESS_LOG:M/*}
	echo =========================================================
	echo REGRESS_LOG must contain an absolute path to the log-file.
	echo It currently points to: ${REGRESS_LOG}
	echo =========================================================
	exit 1
.endif
.for RT in ${REGRESS_TARGETS} 
.  if ${REGRESS_SKIP_TARGETS:M${RT}}
	@echo -n "SKIP " ${_REGRESS_OUT}
.  else
# XXX - we need a better method to see if a test fails due to timeout or just
#       normal failure.
.   if !defined(REGRESS_MAXTIME)
	-if cd ${.CURDIR} && ${MAKE} ${RT}; then \
	    echo -n "SUCCESS " ${_REGRESS_OUT} ; \
	else \
	    echo -n "FAIL " ${_REGRESS_OUT} ; \
	    echo FAILED ; \
	fi
.   else
	-if cd ${.CURDIR} && (ulimit -t ${REGRESS_MAXTIME} ; ${MAKE} ${RT}); then \
	    echo -n "SUCCESS " ${_REGRESS_OUT} ; \
	else \
	    echo -n "FAIL (possible timeout) " ${_REGRESS_OUT} ; \
	    echo FAILED ; \
	fi
.   endif
.  endif
	@echo ${_REGRESS_NAME}/${RT:S/^run-regress-//} ${_REGRESS_OUT}
.endfor

.PHONY: regress
