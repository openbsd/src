#!/bin/ksh
#	$OpenBSD: maxprefix.sh,v 1.2 2022/03/08 17:20:52 claudio Exp $

set -e

BGPD=$1
BGPDCONFIGDIR=$2
RDOMAIN1=$3
RDOMAIN2=$4
PAIR1=$5
PAIR2=$6

RDOMAINS="${RDOMAIN1} ${RDOMAIN2}"
PAIRS="${PAIR1} ${PAIR2}"
PAIR1IP=10.12.57.1
PAIR2IP=10.12.57.2

error_notify() {
	echo cleanup
	pkill -T ${RDOMAIN1} bgpd || true
	pkill -T ${RDOMAIN2} bgpd || true
	sleep 1
	ifconfig ${PAIR2} destroy || true
	ifconfig ${PAIR1} destroy || true
	route -qn -T ${RDOMAIN1} flush || true
	route -qn -T ${RDOMAIN2} flush || true
	ifconfig lo${RDOMAIN1} destroy || true
	ifconfig lo${RDOMAIN2} destroy || true
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
ifconfig ${PAIR1} rdomain ${RDOMAIN1} ${PAIR1IP}/30 up
ifconfig ${PAIR2} rdomain ${RDOMAIN2} ${PAIR2IP}/30 up
ifconfig ${PAIR1} patch ${PAIR2}
ifconfig lo${RDOMAIN1} inet 127.0.0.1/8
ifconfig lo${RDOMAIN2} inet 127.0.0.1/8

echo run bgpds
route -T ${RDOMAIN1} exec ${BGPD} \
        -v -f ${BGPDCONFIGDIR}/bgpd.maxprefix.rdomain1.conf
sleep 1
route -T ${RDOMAIN2} exec ${BGPD} \
	-v -f ${BGPDCONFIGDIR}/bgpd.maxprefix.rdomain2.conf

sleep 2

echo test1: add two networks
route -T ${RDOMAIN1} exec bgpctl network add 10.12.58.0/24
route -T ${RDOMAIN1} exec bgpctl network add 10.12.59.0/24
sleep 1
route -T ${RDOMAIN1} exec bgpctl show nei | \
	awk '/^  Prefixes/ { if ($2 == "2") { print "ok"; ok=1; exit 0; } }
	     END { if (ok != 1) { print "bad bgpctl output"; exit 2; } }'

echo test1: add another network
route -T ${RDOMAIN1} exec bgpctl network add 10.12.60.0/24
sleep 1
route -T ${RDOMAIN1} exec bgpctl show nei | \
	grep '^  Last error received: Cease, received max-prefix exceeded' 
