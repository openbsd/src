#!/bin/sh
#	$OpenBSD: testrsa.sh,v 1.6 2002/01/02 15:13:18 art Exp $


#Test RSA certificate generation of openssl

cd $1

# Generate RSA private key
openssl genrsa -out rsakey.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Generate an RSA certificate
openssl req -config $2/openssl.cnf -key rsakey.pem -new -x509 -days 365 -out rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
openssl x509 -text -in rsacert.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
