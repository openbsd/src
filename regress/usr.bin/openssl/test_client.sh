#!/bin/sh
#	$OpenBSD: test_client.sh,v 1.1 2014/08/26 17:50:07 jsing Exp $

echo
echo This starts a tls1 mode client to talk to the server run by 
echo ./testserver.sh. You should start the server first. 
echo
echo type in this window after ssl negotiation and your output should
echo be echoed by the server. 
echo
echo
/usr/bin/openssl s_client -tls1
