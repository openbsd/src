# $OpenBSD: Makefile,v 1.1 2016/08/16 22:25:08 vgross Exp $

PROG	   = runtest
DESTADDR   = www.google.com
TESTIFACE  = vether12
TESTNET   != jot -r -s '.' 2 0 255
RESV_ADDR  = 10.${TESTNET}.1
BIND_ADDR  = 10.${TESTNET}.2
CMSG_ADDR  = 10.${TESTNET}.3
NONE_ADDR  = 10.${TESTNET}.4

run-regress-runtest: ${PROG}
	${SUDO} ifconfig ${TESTIFACE} create
	${SUDO} ifconfig ${TESTIFACE} inet ${RESV_ADDR}/24 up
	${SUDO} ifconfig ${TESTIFACE} inet ${BIND_ADDR}/24 alias
	${SUDO} ifconfig ${TESTIFACE} inet ${CMSG_ADDR}/24 alias
	sleep 1
	${SUDO} ifconfig ${TESTIFACE}
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr ${CMSG_ADDR}=cmsg_saddr,wire_saddr
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr ${BIND_ADDR}=cmsg_saddr,wire_saddr
	${.OBJDIR}/${PROG} 49 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr ${NONE_ADDR}=cmsg_saddr
	${.OBJDIR}/${PROG} 48 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr ${RESV_ADDR}=cmsg_saddr
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr 0.0.0.0=bind_saddr ${BIND_ADDR}=cmsg_saddr,wire_saddr
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr 0.0.0.0=bind_saddr ${RESV_ADDR}=cmsg_saddr,wire_saddr
	${.OBJDIR}/${PROG}  0 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr 0.0.0.0=cmsg_saddr
	${.OBJDIR}/${PROG} 22 ${DESTADDR}=destination ${RESV_ADDR}=reserved_saddr ${BIND_ADDR}=bind_saddr ${CMSG_ADDR}=cmsg_saddr fuzz
	${SUDO} ifconfig ${TESTIFACE} destroy

.include <bsd.regress.mk>
