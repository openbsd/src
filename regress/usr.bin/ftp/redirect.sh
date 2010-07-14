#!/bin/sh

: ${FTP:=ftp}

: ${rport1:=9000}
: ${rport2:=9001}

req1=$1
loc=$2
req2=$3

echo "Testing $req1 => $loc => $req2"

# Be sure to kill any previous nc running on our port
while pkill -fx "nc -l $rport1" && sleep 1; do done

echo "HTTP/1.0 302 Found\r\nLocation: $loc\r\n\r" | nc -l $rport1 >&- &

# Give the "server" some time to start
sleep .1

res=$(${FTP} -o/dev/null $req1 2>&1 | sed '/^Redirected to /{s///;x;};$!d;x')

if [ X"$res" != X"$req2" ]; then
	echo "*** Fail; expected \"$req2\", got \"$res\""
	exit 1
fi
