#!/bin/sh

echo This starts a tls1 mode server using the DSA certificate in ./server.pem
echo Run ./testclient.sh in another window and type at it, you should 
echo see the results of the ssl negotiation, and stuff you type in the client
echo should echo in this window
echo
echo
/usr/sbin/ssleay s_server -tls1 -key testdsa.key -cert testdsa.pem
