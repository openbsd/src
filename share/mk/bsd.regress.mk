# No man pages for regression tests.
NOMAN=

# No installation.
install:

# If REGRESSTARGETS is defined and PROG is not defined, set a dummy
# PROG
.if defined(REGRESSTARGETS) && !defined(PROG)
PROG=
.endif

.include <bsd.prog.mk>

# XXX - Need full path to REGRESSLOG, otherwise there will be much pain.

REGRESSLOG?=/dev/null
REGRESSNAME=${.CURDIR:S/${BSDSRCDIR}\/regress\///}

.if defined(PROG) && !empty(PROG)
run-regress-${PROG}: ./${PROG}
	./${PROG}
.endif

.if !defined(REGRESSTARGETS)
REGRESSTARGETS=run-regress-${PROG}
.  if defined(REGRESSSKIP)
REGRESSSKIPTARGETS=run-regress-${PROG}
.  endif
.endif

REGRESSSKIPTARGETS?=

regress:
.for RT in ${REGRESSTARGETS} 
.  if ${REGRESSSKIPTARGETS:M${RT}}
	@echo -n "SKIP " >> ${REGRESSLOG}
.  else
	@if cd ${.CURDIR} && ${MAKE} ${RT}; then \
	    echo -n "SUCCESS " >> ${REGRESSLOG} ; \
    	else \
	    echo -n "FAIL " >> ${REGRESSLOG} ; \
	    echo FAILED ; \
	fi
.  endif
	@echo ${REGRESSNAME}/${RT:S/^run-regress-//} >> ${REGRESSLOG}
.endfor

.PHONY: regress
