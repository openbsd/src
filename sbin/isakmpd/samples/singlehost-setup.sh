#!/bin/sh
#	$OpenBSD: singlehost-setup.sh,v 1.3 2002/06/12 21:32:43 ho Exp $
#	$EOM: singlehost-setup.sh,v 1.3 2000/11/23 12:24:43 niklas Exp $

# A script to test single-host VPNs

# For the 'pf' variable
. /etc/rc.conf

# Default paths
PF_CONF=/etc/pf.conf
PFCTL=/sbin/pfctl
ISAKMPD=/sbin/isakmpd

# Called on script exit
cleanup () {
    if [ "X${pf}" = "xYES" -a -f ${PF_CONF} ]; then
	${PFCTL} -R -f ${PF_CONF}
    else
	${PFCTL} -qd
    fi

    USER=`id -p | grep ^login | cut -f2`
    chown $USER singlehost-east.conf singlehost-west.conf policy
    chmod 644   singlehost-east.conf singlehost-west.conf policy

    [ -f east.pid ] && kill `cat east.pid`
    [ -f west.pid ] && kill `cat west.pid`
    rm -f east.pid west.pid east.fifo west.fifo
}

# Start by initializing interfaces
/sbin/ifconfig lo2 192.168.11.1 netmask 0xffffff00 up
/sbin/ifconfig lo3 192.168.12.1 netmask 0xffffff00 up
/sbin/ifconfig lo4 10.1.0.11 netmask 0xffff0000 up
/sbin/ifconfig lo5 10.1.0.12 netmask 0xffff0000 up

# Add rules
(
    cat <<EOF
pass out quick on lo2 proto 50 all
pass out quick on lo2 from 192.168.11.0/24 to any
pass out quick on lo3 proto 50 all
pass out quick on lo3 from 192.168.12.0/24 to any
block out on lo2 all
block out on lo3 all
EOF
    if [ "X${pf}" = "xYES" -a -f ${PF_CONF} ]; then
	cat ${PF_CONF} | egrep -v '^(scrub|rdr|binat|nat)'
    else
	pfctl -qe >/dev/null
    fi
) | tee /tmp/aa | pfctl -R -f -

trap cleanup 1 2 3 15

# The configuration files needs proper owners and modes
USER=`id -p | grep ^uid | cut -f2`
chown $USER singlehost-east.conf singlehost-west.conf policy
chmod 600   singlehost-east.conf singlehost-west.conf policy

# Start the daemons
rm -f east.pid west.pid east.fifo west.fifo
${ISAKMPD} -c singlehost-east.conf -f east.fifo -i east.pid "$@"
${ISAKMPD} -c singlehost-west.conf -f west.fifo -i west.pid "$@"

# Give them some time to negotiate their stuff...
sleep 10
ping -I 192.168.11.1 -c 30 192.168.12.1

cleanup
