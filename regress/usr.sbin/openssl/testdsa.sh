#!/bin/sh
#	$OpenBSD: testdsa.sh,v 1.1 2014/05/24 13:32:46 jsing Exp $


#Test DSA certificate generation of openssl

cd $1

# Generate DSA paramter set
openssl dsaparam 512 -out dsa512.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Denerate a DSA certificate
openssl req -config $2/openssl.cnf -x509 -newkey dsa:dsa512.pem -out testdsa.pem -keyout testdsa.key
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
openssl x509 -text -in testdsa.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
