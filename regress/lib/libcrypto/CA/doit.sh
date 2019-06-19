#!/bin/sh
#	$OpenBSD: doit.sh,v 1.2 2018/07/17 17:06:49 tb Exp $

rm -rf root intermediate certs
echo 1000 > rootserial
cat /dev/null > root.txt
echo 1000 > intserial
cat /dev/null > int.txt

# Vanna Vanna make me a root cert
openssl genrsa -out root.key.pem 4096
if [ $? -ne 0 ]; then
   echo "*** Fail;  Can't generate root rsa 4096 key"
   exit 1
fi   

openssl req -batch -config root.cnf -key root.key.pem -new -x509 -days 365 -sha256 -extensions v3_ca -out root.cert.pem
if [ $? -ne 0 ]; then
   echo "*** Fail;  Can't generate root req"
   exit 1
fi   

# Make intermediate
openssl genrsa -out intermediate.key.pem 2048
if [ $? -ne 0 ]; then
   echo "*** Fail;  Can't generate intermediate rsa 2048 key"
   exit 1
fi   

openssl req -batch -config intermediate.cnf -new -sha256 \
      -key intermediate.key.pem \
      -out intermediate.csr.pem
if [ $? -ne 0 ]; then
   echo "*** Fail;  Can't generate intermediate req"
   exit 1
fi   

# Sign intermediate
openssl ca -batch -config root.cnf -extensions v3_intermediate_ca -days 10 -notext -md sha256 -in intermediate.csr.pem -out intermediate.cert.pem 
if [ $? -ne 0 ]; then
   echo "*** Fail;  Can't sign intermediate"
   exit 1
fi   

# Verify Intermediate
openssl verify -CAfile ca.cert.pem intermediate.cert.pem
if [ $? -ne 0]; then
   echo "*** Fail; Intermediate CA does not validate"
   exit 1
fi   

cat intermediate.cert.pem root.cert.pem > chain.pem

# make a server certificate

openssl genrsa -out server.key.pem 2048
if [ $? -ne 0]; then
   echo "*** Fail; genrsa server"
   exit 1
fi   


openssl req -batch -config intermediate.cnf \
      -key server.key.pem \
      -new -sha256 -out server.csr.pem \
      -subj '/CN=server/O=OpenBSD/OU=So and Sos/C=CA'
if [ $? -ne 0]; then
   echo "*** Fail; server req"
   exit 1
fi   

# sign server key
openssl ca -batch -config intermediate.cnf -extensions server_cert -days 5 -notext -md sha256 -in server.csr.pem -out server.cert.pem
if [ $? -ne 0 ]; then
   echo "*** Fail; server sign"
   exit 1
fi   

# make a client certificate

openssl genrsa -out client.key.pem 2048
if [ $? -ne 0]; then
   echo "*** Fail; genrsa client"
   exit 1
fi   

openssl req -batch -config intermediate.cnf \
      -key client.key.pem \
      -new -sha256 -out client.csr.pem \
      -subj '/CN=client/O=OpenBSD/OU=So and Sos/C=CA'
if [ $? -ne 0]; then
   echo "*** Fail; client req"
   exit 1
fi   

# sign client key
openssl ca -batch -config intermediate.cnf -extensions usr_cert -days 5 -notext -md sha256 -in client.csr.pem -out client.cert.pem
if [ $? -ne 0 ]; then
   echo "*** Fail; client sign"
   exit 1
fi

# Verify Intermediate
openssl verify -purpose sslserver -CAfile chain.pem server.cert.pem
if [ $? -ne 0 ]; then
   echo "*** Fail; server cert does not validate"
   exit 1
fi   

# Verify Intermediate
openssl verify -purpose sslclient -CAfile chain.pem client.cert.pem
if [ $? -ne 0 ]; then
   echo "*** Fail; client cert does not validate"
   exit 1
fi   

