#!/bin/sh
#	$OpenBSD: test_client.sh,v 1.3 2001/01/29 02:05:48 niklas Exp $


echo
echo This starts a tls1 mode client to talk to the server run by 
echo ./testserver.sh. You should start the server first. 
echo
echo type in this window after ssl negotiation and your output should
echo be echoed by the server. 
echo
echo
/usr/sbin/openssl s_client -tls1
