# No man pages for regression tests.
NOMAN=

# No installation.
install:

# If REGRESSTARGETS is defined and PROG is not defined, set NOPROG
.if defined(REGRESSTARGETS) && !defined(PROG)
NOPROG=
.endif

.include <bsd.prog.mk>

# XXX - Need full path to REGRESSLOG, otherwise there will be much pain.

REGRESSLOG?=/dev/null
REGRESSNAME=${.CURDIR:S/${BSDSRCDIR}\/regress\///}

.if defined(PROG) && !empty(PROG)
run-regress-${PROG}: ${PROG}
	./${PROG}
.endif

.if !defined(REGRESSTARGETS)
REGRESSTARGETS=run-regress-${PROG}
.  if defined(REGRESSSKIP)
REGRESSSKIPTARGETS=run-regress-${PROG}
.  endif
.endif

REGRESSSKIPSLOW?=no

.if (${REGRESSSKIPSLOW} == "yes") && defined(REGRESSSLOWTARGETS)
REGRESSSKIPTARGETS+=${REGRESSSLOWTARGETS}
.endif

REGRESSSKIPTARGETS?=

regress:
.for RT in ${REGRESSTARGETS} 
.  if ${REGRESSSKIPTARGETS:M${RT}}
	@echo -n "SKIP " >> ${REGRESSLOG}
.  else
# XXX - we need a better method to see if a test fails due to timeout or just
#       normal failure.
.   if !defined(REGRESSMAXTIME)
	@if cd ${.CURDIR} && ${MAKE} ${RT}; then \
	    echo -n "SUCCESS " >> ${REGRESSLOG} ; \
	else \
	    echo -n "FAIL " >> ${REGRESSLOG} ; \
	    echo FAILED ; \
	fi
.   else
	@if cd ${.CURDIR} && (ulimit -t ${REGRESSMAXTIME} ; ${MAKE} ${RT}); then \
	    echo -n "SUCCESS " >> ${REGRESSLOG} ; \
	else \
	    echo -n "FAIL (possible timeout) " >> ${REGRESSLOG} ; \
	    echo FAILED ; \
	fi
.   endif
.  endif
	@echo ${REGRESSNAME}/${RT:S/^run-regress-//} >> ${REGRESSLOG}
.endfor

.PHONY: regress
