
PROG=	stackpivot

run-regress-${PROG}: ${PROG}
	rm -f ./${PROG}.core
	if ./${PROG}; then false; else true; fi
	if [ ! -e ./${PROG}.core ]; then echo "No coredump"; false; fi

.if ${MACHINE} != "amd64" && \
    ${MACHINE} != "i386"
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# Need stack pivot asm for this arch
	@echo SKIPPED
.endif


.include <bsd.regress.mk>
