# $OpenBSD: Makefile,v 1.1 2016/03/19 15:09:13 vgross Exp $

PROG	   = runtest
TESTIFACE  = vether11
TESTNET   != jot -r -s ':' -w %x 2 0 65535
DESTADDR   = fd00:${TESTNET}::100
FIRSTADDR  = fd00:${TESTNET}::1
BASEADDR   = fd00:${TESTNET}::2
ADDR_3     = fd00:${TESTNET}::3
ADDR_4     = fd00:${TESTNET}::4
ABSENTADDR = fd00:${TESTNET}::5

run-regress-runtest: ${PROG}
	${SUDO} ifconfig ${TESTIFACE} create
	${SUDO} ifconfig ${TESTIFACE} inet6 ${DESTADDR}/64
	${SUDO} ifconfig ${TESTIFACE} inet6 ${FIRSTADDR}/64
	${SUDO} ifconfig ${TESTIFACE} inet6 ${BASEADDR}/64
	${SUDO} ifconfig ${TESTIFACE} inet6 ${ADDR_3}/64
	${SUDO} ifconfig ${TESTIFACE} inet6 ${ADDR_4}/64
	sleep 1
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${ADDR_3} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -m ${ADDR_3} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${BASEADDR} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -m ${BASEADDR} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${ABSENTADDR} -e 49
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -m ${ABSENTADDR} -e 49
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${FIRSTADDR} -e 48
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -m ${FIRSTADDR} -e 48
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b :: -o ${BASEADDR} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b :: -m ${BASEADDR} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b :: -o ${FIRSTADDR} -e 48
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b :: -m ${FIRSTADDR} -e 48
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o :: -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -m :: -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${ADDR_3} -m ${ADDR_4} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${ABSENTADDR} -m ${ADDR_4} -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${ADDR_3} -m :: -e 0
	${.OBJDIR}/${PROG} -d ${DESTADDR} -f ${FIRSTADDR} -b ${BASEADDR} -o ${FIRSTADDR} -m :: -e 0
	${SUDO} ifconfig ${TESTIFACE} destroy

.include <bsd.regress.mk>
