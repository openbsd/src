#!/bin/sh
#	$OpenBSD: test_client.sh,v 1.3 2023/07/03 05:31:56 beck Exp $

echo
echo This starts a tls1 mode client to talk to the server run by 
echo ./testserver.sh. You should start the server first. 
echo
echo type in this window after ssl negotiation and your output should
echo be echoed by the server. 
echo
echo
${OPENSSL:-/usr/bin/openssl} s_client
