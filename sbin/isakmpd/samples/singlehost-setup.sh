#!/bin/sh
#	$OpenBSD: singlehost-setup.sh,v 1.2 2000/11/23 12:56:25 niklas Exp $
#	$EOM: singlehost-setup.sh,v 1.3 2000/11/23 12:24:43 niklas Exp $

# A script to test single-host VPNs

cleanup () {
  ipf -r -f - <<'  EOF'
  pass out quick on lo2 proto 50 all
  pass out quick on lo2 from 192.168.11.0/24 to any
  pass out quick on lo3 proto 50 all
  pass out quick on lo3 from 192.168.12.0/24 to any
  block out on lo2
  block out on lo3
  EOF
}

ifconfig lo2 192.168.11.1 netmask 0xffffff00
ifconfig lo3 192.168.12.1 netmask 0xffffff00
ifconfig lo4 10.1.0.11 netmask 0xffff0000
ifconfig lo5 10.1.0.12 netmask 0xffff0000

ipf -E -f - <<EOF
pass out quick on lo2 proto 50 all
pass out quick on lo2 from 192.168.11.0/24 to any
pass out quick on lo3 proto 50 all
pass out quick on lo3 from 192.168.12.0/24 to any
block out on lo2
block out on lo3
EOF

trap cleanup 1 2 3 15

isakmpd -c singlehost-east.conf -f east.fifo "$@"
isakmpd -c singlehost-west.conf -f west.fifo "$@"

# Give them some slack...

sleep 10
ping -I 192.168.11.1 -c 30 192.168.12.1

cleanup
