#!/bin/ksh
#	$OpenBSD: network_statement.sh,v 1.2 2019/12/26 17:13:27 denis Exp $
set -e

OSPF6D=$1
OSPF6DCONFIGDIR=$2
RDOMAIN1=$3
RDOMAIN2=$4
PAIR1=$5
PAIR2=$6

RDOMAINS="${RDOMAIN1} ${RDOMAIN2}"
PAIRS="${PAIR1} ${PAIR2}"
PAIR1IP=2001:db8::${RDOMAIN1}
PAIR2IP=2001:db8::${RDOMAIN2}
PAIR1PREFIX=2001:db8:${RDOMAIN1}::
PAIR2PREFIX=2001:db8:${RDOMAIN2}::
PAIR2PREFIX2=2001:db8:${RDOMAIN2}:${RDOMAIN2}::

error_notify() {
	echo cleanup
	pkill -T ${RDOMAIN1} ospf6d || true
	pkill -T ${RDOMAIN2} ospf6d || true
	sleep 1
	ifconfig ${PAIR2} destroy || true
	ifconfig ${PAIR1} destroy || true
	ifconfig vether${RDOMAIN1} destroy || true
	ifconfig vether${RDOMAIN2} destroy || true
	route -qn -T ${RDOMAIN1} flush || true
	route -qn -T ${RDOMAIN2} flush || true
	ifconfig lo${RDOMAIN1} destroy || true
	ifconfig lo${RDOMAIN2} destroy || true
	rm ${OSPF6DCONFIGDIR}/ospf6d.1.conf ${OSPF6DCONFIGDIR}/ospf6d.2.conf
	if [ $1 -ne 0 ]; then
		echo FAILED
		exit 1
	else
		echo SUCCESS
	fi
}

if [ "$(id -u)" -ne 0 ]; then
	echo need root privileges >&2
	exit 1
fi

trap 'error_notify $?' EXIT

echo check if rdomains are busy
for n in ${RDOMAINS}; do
	if /sbin/ifconfig | grep -v "^lo${n}:" | grep " rdomain ${n} "; then
		echo routing domain ${n} is already used >&2
		exit 1
	fi
done

echo check if interfaces are busy
for n in ${PAIRS}; do
	/sbin/ifconfig "${n}" >/dev/null 2>&1 && \
	    ( echo interface ${n} is already used >&2; exit 1 )
done

set -x

echo setup
ifconfig ${PAIR1} inet6 rdomain ${RDOMAIN1} ${PAIR1IP}/64 up
ifconfig ${PAIR2} inet6 rdomain ${RDOMAIN2} ${PAIR2IP}/64 up
ifconfig ${PAIR1} patch ${PAIR2}
ifconfig lo${RDOMAIN1} inet 127.0.0.1/8
ifconfig lo${RDOMAIN2} inet 127.0.0.1/8
ifconfig vether${RDOMAIN1} inet6 rdomain ${RDOMAIN1} ${PAIR1PREFIX}/64 up
ifconfig vether${RDOMAIN2} inet6 rdomain ${RDOMAIN2} ${PAIR2PREFIX}/64 up
ifconfig vether${RDOMAIN2} inet6 rdomain ${RDOMAIN2} ${PAIR2PREFIX2}/64 up
sed "s/{RDOMAIN1}/${RDOMAIN1}/g;s/{PAIR1}/${PAIR1}/g" \
    ${OSPF6DCONFIGDIR}/ospf6d.network_statement.rdomain1.conf \
    > ${OSPF6DCONFIGDIR}/ospf6d.1.conf
chmod 0600 ${OSPF6DCONFIGDIR}/ospf6d.1.conf
sed "s/{RDOMAIN2}/${RDOMAIN2}/g;s/{PAIR2}/${PAIR2}/g" \
    ${OSPF6DCONFIGDIR}/ospf6d.network_statement.rdomain2.conf \
    > ${OSPF6DCONFIGDIR}/ospf6d.2.conf
chmod 0600 ${OSPF6DCONFIGDIR}/ospf6d.2.conf

echo add routes
route -T ${RDOMAIN2} add -inet6 default ${PAIR2PREFIX}1
route -T ${RDOMAIN2} add 2001:db8:ffff::/126 ${PAIR2PREFIX}2
route -T ${RDOMAIN2} add 2001:db8:fffe::/64 ${PAIR2PREFIX}3 -label toOSPF

echo start ospf6d
route -T ${RDOMAIN1} exec ${OSPF6D} \
    -v -f ${OSPF6DCONFIGDIR}/ospf6d.1.conf
route -T ${RDOMAIN2} exec ${OSPF6D} \
    -v -f ${OSPF6DCONFIGDIR}/ospf6d.2.conf

sleep 50

echo tests
route -T ${RDOMAIN1} exec ospf6ctl sh fib
route -T ${RDOMAIN1} exec ospf6ctl sh rib | \
    grep ${PAIR2PREFIX}/64
route -T ${RDOMAIN1} exec ospf6ctl sh rib | \
    grep ${PAIR2PREFIX2}/64
route -T ${RDOMAIN1} exec ospf6ctl sh rib | \
    grep "2001:db8:ffff::/126"
route -T ${RDOMAIN1} exec ospf6ctl sh rib | \
    grep "::/0"
route -T ${RDOMAIN1} exec ospf6ctl sh rib | \
    grep "2001:db8:fffe::/64"

exit 0
