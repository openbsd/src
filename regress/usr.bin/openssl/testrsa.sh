#!/bin/sh
#	$OpenBSD: testrsa.sh,v 1.2 2018/02/06 02:31:13 tb Exp $


#Test RSA certificate generation of openssl

cd $1
openssl_bin=${OPENSSL:-/usr/bin/openssl}

# Generate RSA private key
$openssl_bin genrsa -out rsakey.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Generate an RSA certificate
$openssl_bin req -config $2/openssl.cnf -key rsakey.pem -new -x509 -days 365 -out rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
$openssl_bin x509 -text -in rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
