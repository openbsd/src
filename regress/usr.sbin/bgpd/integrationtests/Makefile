# $OpenBSD: Makefile,v 1.11 2020/01/26 00:54:47 claudio Exp $

REGRESS_TARGETS	= 	network_statement md5 ovs mrt \
			maxprefix maxprefixout as0

BGPD ?=			/usr/sbin/bgpd

CLEANFILES +=		*.mrt *.out *.conf *.log

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
.else

.SUFFIXES: .conf .in
.in.conf:
	sed 's|##BGPDCONFIGDIR##|${.CURDIR}|' ${.IMPSRC} > ${.TARGET} || \
	(rm ${.TARGET} && exit 1)

AS0_CONFS = 	exabgp.as0.test1.conf \
	exabgp.as0.test2.conf exabgp.as0.test2_2.conf exabgp.as0.test2_3.conf

as0: ${AS0_CONFS}
	${SUDO} ksh ${.CURDIR}/$@.sh ${BGPD} ${.CURDIR} 11 12 pair11 pair12
.endif

.include <bsd.regress.mk>
