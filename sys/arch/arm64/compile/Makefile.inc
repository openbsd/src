SYSDIR != cd ${.CURDIR}/../../../..; pwd
CONFDIR != cd ${.CURDIR}/../../conf; pwd

.if ${.CURDIR} == ${.OBJDIR}
.PHONY: config
config:
	@echo make obj required first >&2
	@false
.else
.PHONY: config clean
config:
	config -b ${.OBJDIR} -s ${SYSDIR} ${CONFDIR}/${.CURDIR:T}
.endif

cleandir clean:

.include <bsd.obj.mk>

