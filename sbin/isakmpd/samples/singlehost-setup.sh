#!/bin/sh
#	$OpenBSD: singlehost-setup.sh,v 1.1 1999/03/31 23:45:53 niklas Exp $
#	$EOM: singlehost-setup.sh,v 1.2 1999/03/31 23:45:16 niklas Exp $

# A script to test single-host VPNs

cleanup () {
  ipf -r -f - <<'  EOF'
  pass out quick on lo2 proto 50
  pass out quick on lo2 from 192.168.1.0/24 to any
  pass out quick on lo3 proto 50
  pass out quick on lo3 from 192.168.2.0/24 to any
  block out on lo2
  block out on lo3
  EOF
}

ifconfig lo2 192.168.1.1 netmask 0xffffff00
ifconfig lo3 192.168.2.1 netmask 0xffffff00
ifconfig lo4 10.1.0.1 netmask 0xffff0000
ifconfig lo5 10.1.0.2 netmask 0xffff0000

ipf -E -f - <<EOF
pass out quick on lo2 proto 50
pass out quick on lo2 from 192.168.1.0/24 to any
pass out quick on lo3 proto 50
pass out quick on lo3 from 192.168.2.0/24 to any
block out on lo2
block out on lo3
EOF

trap cleanup 1 2 3 15

isakmpd -c singlehost-east.conf -f east.fifo "$@"
isakmpd -c singlehost-west.conf -f west.fifo "$@"

# Give them some slack...

sleep 10
ping -I 192.168.1.1 -c 30 192.168.2.1

cleanup
