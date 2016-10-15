SYSDIR != cd ${.CURDIR}/../../../..; pwd
CONFDIR != cd ${.CURDIR}/../../conf; pwd

.if ${.CURDIR} == ${.OBJDIR}
.PHONY: config
config:
	@echo make obj required first >&2
	@false
.elif !exists(${OBJDIR}/Makefile)
.PHONY: config clean
config:
	config -b ${.OBJDIR} -s ${SYSDIR} ${CONFDIR}/${.CURDIR:T}

cleandir clean:

.endif

.include <bsd.obj.mk>

