
.if ${MACHINE} != "arm64" && \
    ${MACHINE} != "amd64" && \
    ${MACHINE} != "i386" && \
    ${MACHINE} != "octeon"
REGRESS_TARGETS=run-regress-skiparch
run-regress-skiparch:
	# Need stack pivot asm for this arch
	@echo SKIPPED
.include <bsd.regress.mk>
.else
SUBDIR += syscall
SUBDIR += pagefault
.include <bsd.subdir.mk>
.endif


