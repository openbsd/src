# $OpenBSD: Makefile,v 1.12 2021/01/12 08:59:03 claudio Exp $

REGRESS_TARGETS	= 	network_statement md5 ovs mrt \
			maxprefix maxprefixout as0

BGPD ?=			/usr/sbin/bgpd

CLEANFILES +=		*.mrt *.out *.conf *.log *.fifo api-exabgp*

api-exabgp: api-exabgp.c

network_statement:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

md5:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

ovs:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

mrt:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11

maxprefix:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

maxprefixout:
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.if ! exists(/usr/local/bin/exabgp)
as0:
	# install exabgp from ports for additional tests
	@echo SKIPPED

med:
	# install exabgp from ports for additional tests
	@echo SKIPPED
.else

.SUFFIXES: .conf .in
.in.conf:
	sed -e 's|##OBJDIR##|${.OBJDIR}|g' \
	    ${.IMPSRC} > ${.TARGET} || \
	(rm ${.TARGET} && exit 1)

AS0_CONFS = 	exabgp.as0.test1.conf exabgp.as0.test2.conf

as0: api-exabgp ${AS0_CONFS}
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

med: api-exabgp exabgp.med.conf
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12

.endif

.include <bsd.regress.mk>
