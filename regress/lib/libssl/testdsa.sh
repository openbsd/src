#!/bin/sh

#Test DSA certificate generation of ssleay

cd $1

# Generate DSA paramter set
ssleay dsaparam 512 -out dsa512.pem
if [ $? != 0 ]; then
        exit 1;
fi


# Denerate a DSA certificate
ssleay req -config $2/ssleay.cnf -x509 -newkey dsa:dsa512.pem -out testdsa.pem -keyout testdsa.key
if [ $? != 0 ]; then
        exit 1;
fi


# Now check the certificate
ssleay x509 -text -in testdsa.pem
if [ $? != 0 ]; then
        exit 1;
fi

exit 0
