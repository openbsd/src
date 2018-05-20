#!/bin/sh
#	$OpenBSD: testdsa.sh,v 1.2 2018/02/06 02:31:13 tb Exp $


#Test DSA certificate generation of openssl

cd $1

# Generate DSA paramter set
openssl_bin=${OPENSSL:-/usr/bin/openssl}
$openssl_bin dsaparam 512 -out dsa512.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Denerate a DSA certificate
$openssl_bin req -config $2/openssl.cnf -x509 -newkey dsa:dsa512.pem -out testdsa.pem -keyout testdsa.key
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
$openssl_bin x509 -text -in testdsa.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
