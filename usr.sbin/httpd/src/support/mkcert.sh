#!/bin/sh
##
##  mkcert.sh -- Make SSL Certificate Files for `make certificate' command
##  Copyright (c) 1998-1999 Ralf S. Engelschall, All Rights Reserved. 
##

#   parameters
make="$1"
mflags="$2"
ssleay="$3"
support="$4"
type="$5"
crt="$6"
key="$7"

#   we can operate only inside the Apache 1.3 source
#   tree and only when mod_ssl+SSLeay is actually configured.
if [ ! -f "../README.configure" ]; then
    echo "mkcert.sh:Error: Cannot operate outside the Apache 1.3 source tree." 1>&2
    echo "mkcert.sh:Hint:  You have to stay inside apache_1.3.x/src." 1>&2
    exit 1
fi
if [ ".$ssleay" = . ]; then
    echo "mkcert.sh:Error: mod_ssl/SSLeay has to be configured before using this utility." 1>&2
    echo "mkcert.sh:Hint:  Configure mod_ssl with --enable-module=ssl in APACI, first." 1>&2
    exit 1
fi

#   configuration
#   WE ARE CALLED FROM THE PARENT DIR!
sslcrtdir="../conf/ssl.crt"
sslcsrdir="../conf/ssl.csr"
sslkeydir="../conf/ssl.key"

#   some optional terminal sequences
case $TERM in
    xterm|xterm*|vt220|vt220*)
        T_MD=`echo dummy | awk '{ printf("%c%c%c%c", 27, 91, 49, 109); }'`
        T_ME=`echo dummy | awk '{ printf("%c%c%c", 27, 91, 109); }'`
        ;;
    vt100|vt100*)
        T_MD=`echo dummy | awk '{ printf("%c%c%c%c%c%c", 27, 91, 49, 109, 0, 0); }'`
        T_ME=`echo dummy | awk '{ printf("%c%c%c%c%c", 27, 91, 109, 0, 0); }'`
        ;;
    default)
        T_MD=''
        T_ME=''
        ;;
esac

#   display header
echo "${T_MD}SSL Certificate Generation Utility${T_ME} (mkcert.sh)"
echo "Copyright (c) 1998 Ralf S. Engelschall, All Rights Reserved."

#   find some random files
#   (do not use /dev/random here, because this device 
#   doesn't work as expected on all platforms)
echo " + finding random files on your platform"
randfiles=''
for file in /var/log/messages /var/adm/messages \
            /kernel /vmunix /vmlinuz \
            /etc/hosts /etc/resolv.conf; do
    if [ -f $file ]; then
        if [ ".$randfiles" = . ]; then
            randfiles="$file"
        else
            randfiles="${randfiles}:$file"
        fi
    fi
done

#   on-demand compile the ca-fix only
case $type in
    test|custom)
       cd $support
       if [ ! -f ca-fix ]; then
           echo " + building ca-fix auxiliary tool"
           $make $mflags ca-fix >/dev/null 2>&1
           if [ $? -ne 0 ]; then
               $make $mflags ca-fix
               echo "**FAILED"
               exit 1
           fi
       fi
       cd ..
       cafix="$support/ca-fix"
       ;;
esac

#   processing
case $type in

    dummy)
        echo ""
        echo "${T_MD}Generating self-signed Snake Oil certificate [DUMMY]${T_ME}"
        echo "______________________________________________________________________"
        echo ""
        cp $sslcrtdir/snakeoil.crt $sslcrtdir/server.crt
        cp $sslkeydir/snakeoil.key $sslkeydir/server.key
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded RSA private certificate file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 server certificate file which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "WARNING: Do not use this for real-life/production systems"
        echo ""
        ;;

    test)
        echo ""
        echo "${T_MD}Generating test certificate signed by Snake Oil CA [TEST]${T_ME}"
        echo "WARNING: Do not use this for real-life/production systems"
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 1: Generating RSA private key (1024 bit) [server.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$randfiles" != . ]; then
            $ssleay genrsa -rand $randfiles \
                           -out $sslkeydir/server.key \
                           1024
        else
            $ssleay genrsa -out $sslkeydir/server.key \
                           1024
        fi
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 2: Generating X.509 certificate signing request [server.csr]${T_ME}"
        cat >.mkcert.cfg <<EOT
[ req ]
default_bits                    = 1024
distinguished_name              = req_DN
[ req_DN ]
countryName                     = "1. Country Name             (2 letter code)"
countryName_default             = XY
countryName_min                 = 2
countryName_max                 = 2
stateOrProvinceName             = "2. State or Province Name   (full name)    "
stateOrProvinceName_default     = Snake Desert
localityName                    = "3. Locality Name            (eg, city)     "
localityName_default            = Snake Town
0.organizationName              = "4. Organization Name        (eg, company)  "
0.organizationName_default      = Snake Oil, Ltd
organizationalUnitName          = "5. Organizational Unit Name (eg, section)  "
organizationalUnitName_default  = Webserver Team
commonName                      = "6. Common Name              (eg, FQDN)     "
commonName_max                  = 64
commonName_default              = www.snakeoil.dom
emailAddress                    = "7. Email Address            (eg, name@FQDN)"
emailAddress_max                = 40
emailAddress_default            = www@snakeoil.dom
EOT
        $ssleay req -config .mkcert.cfg \
                    -new \
                    -key $sslkeydir/server.key \
                    -out $sslcsrdir/server.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 3: Generating X.509 certificate signed by Snake Oil CA [server.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            certversion=3
        else
            certversion=1
        fi
        if [ ! -f .mkcert.serial ]; then
            echo '01' >.mkcert.serial
        fi
        $ssleay x509 -days 365 \
                     -CAserial .mkcert.serial \
                     -CA $sslcrtdir/snakeoil-ca.crt \
                     -CAkey $sslkeydir/snakeoil-ca.key \
                     -in $sslcsrdir/server.csr -req \
                     -out $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate X.509 certificate" 1>&2
            exit 1
        fi
        if [ ".$certversion" = .3 ]; then
            echo "Converting X.509 v1 to v3 certificate"
            $cafix -nscertype 0x40 \
                   -nobscrit \
                   -nosign \
                   -in    $sslcrtdir/server.crt \
                   -inkey $sslkeydir/server.key \
                   -out   $sslcrtdir/server.crt.fixed
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to fixup X.509 certificate contents" 1>&2
                exit 1
            fi
            cp $sslcrtdir/server.crt.fixed $sslcrtdir/server.crt
            rm -f $sslcrtdir/server.crt.fixed
            $cafix -inkey $sslkeydir/snakeoil-ca.key \
                   -in    $sslcrtdir/server.crt \
                   -out   $sslcrtdir/server.crt.fixed
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to fixup X.509 certificate signature" 1>&2
                exit 1
            fi
            cp $sslcrtdir/server.crt.fixed $sslcrtdir/server.crt
            rm -f $sslcrtdir/server.crt.fixed
        fi
        echo "Verify: matching certificate & key modulus"
        modcrt=`$ssleay x509 -noout -modulus -in $sslcrtdir/server.crt`
        modkey=`$ssleay rsa  -noout -modulus -in $sslkeydir/server.key`
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        $ssleay verify -CAfile $sslcrtdir/snakeoil-ca.crt $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 4: Enrypting RSA private key with a pass phrase for security [server.key]${T_ME}"
        echo "The contents of the server.key file (the generated private key) has to be"
        echo "kept secret. So we strongly recommend you to encrypt the server.key file"
        echo "with a Triple-DES cipher and a Pass Phrase."
        while [ 1 ]; do
            echo dummy | awk '{ printf("Encrypt the private key now? [Y/n]: "); }'
            read rc
            if [ ".$rc" = .n -o  ".$rc" = .N ]; then
                rc="n"
                break
            fi
            if [ ".$rc" = .y -o  ".$rc" = .Y -o ".$rc" = . ]; then
                rc="y"
                break
            fi
        done
        if [ ".$rc" = .y ]; then
            $ssleay rsa -des3 \
                        -in  $sslkeydir/server.key \
                        -out $sslkeydir/server.key.crypt
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt RSA private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/server.key.crypt $sslkeydir/server.key
            rm -f $sslkeydir/server.key.crypt
            echo "Fine, you're using an encrypted private key."
        else
            echo "Warning, you're using an unencrypted private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded RSA private certificate file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 server certificate file which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "o  ${T_MD}conf/ssl.csr/server.csr${T_ME}"
        echo "   The PEM-encoded X.509 certificate signing request file which" 
        echo "   you can send to an official Certificate Authority (CA) in order"
        echo "   to request a real server certificate (signed by this CA instead"
        echo "   of our demonstration-only Snake Oil CA) which later can replace"
        echo "   the conf/ssl.crt/server.crt file."
        echo ""
        echo "WARNING: Do not use this for real-life/production systems"
        echo ""
        ;;

    custom)
        echo ""
        echo "${T_MD}Generating custom certificate signed by own CA [CUSTOM]${T_ME}"
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 1: Generating RSA private key for CA (1024 bit) [ca.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$randfiles" != . ]; then
            $ssleay genrsa -rand $randfiles \
                           -out $sslkeydir/ca.key \
                           1024
        else
            $ssleay genrsa -out $sslkeydir/ca.key \
                           1024
        fi
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 2: Generating X.509 certificate signing request for CA [ca.csr]${T_ME}"
        cat >.mkcert.cfg <<EOT
[ req ]
default_bits                    = 1024
distinguished_name              = req_DN
[ req_DN ]
countryName                     = "1. Country Name             (2 letter code)"
countryName_default             = XY
countryName_min                 = 2
countryName_max                 = 2
stateOrProvinceName             = "2. State or Province Name   (full name)    "
stateOrProvinceName_default     = Snake Desert
localityName                    = "3. Locality Name            (eg, city)     "
localityName_default            = Snake Town
0.organizationName              = "4. Organization Name        (eg, company)  "
0.organizationName_default      = Snake Oil, Ltd
organizationalUnitName          = "5. Organizational Unit Name (eg, section)  "
organizationalUnitName_default  = Certificate Authority
commonName                      = "6. Common Name              (eg, CA name)  "
commonName_max                  = 64
commonName_default              = Snake Oil CA
emailAddress                    = "7. Email Address            (eg, name@FQDN)"
emailAddress_max                = 40
emailAddress_default            = ca@snakeoil.dom
EOT
        $ssleay req -config .mkcert.cfg \
                    -new \
                    -key $sslkeydir/ca.key \
                    -out $sslcsrdir/ca.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 3: Generating X.509 certificate for CA signed by itself [ca.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            certversion=3
        else
            certversion=1
        fi
        $ssleay x509 -days 365 \
                     -signkey $sslkeydir/ca.key \
                     -in      $sslcsrdir/ca.csr -req \
                     -out     $sslcrtdir/ca.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate self-signed CA certificate" 1>&2
            exit 1
        fi
        if [ ".$certversion" = .3 ]; then
            $cafix -caset \
                   -nscertype 0x07 \
                   -pathlen 0 \
                   -nobscrit \
                   -in    $sslcrtdir/ca.crt \
                   -inkey $sslkeydir/ca.key \
                   -out   $sslcrtdir/ca.crt.fixed
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to fixup X.509 CA certificate" 1>&2
                exit 1
            fi
            cp $sslcrtdir/ca.crt.fixed $sslcrtdir/ca.crt
            rm -f $sslcrtdir/ca.crt.fixed
        fi
        echo "Verify: matching certificate & key modulus"
        modcrt=`$ssleay x509 -noout -modulus -in $sslcrtdir/ca.crt`
        modkey=`$ssleay rsa  -noout -modulus -in $sslkeydir/ca.key`
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        $ssleay verify $sslcrtdir/ca.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 4: Generating RSA private key for SERVER (1024 bit) [server.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$randfiles" != . ]; then
            $ssleay genrsa -rand $randfiles \
                           -out $sslkeydir/server.key \
                           1024
        else
            $ssleay genrsa -out $sslkeydir/server.key \
                           1024
        fi
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 5: Generating X.509 certificate signing request for SERVER [server.csr]${T_ME}"
        cat >.mkcert.cfg <<EOT
[ req ]
default_bits                    = 1024
distinguished_name              = req_DN
[ req_DN ]
countryName                     = "1. Country Name             (2 letter code)"
countryName_default             = XY
countryName_min                 = 2
countryName_max                 = 2
stateOrProvinceName             = "2. State or Province Name   (full name)    "
stateOrProvinceName_default     = Snake Desert
localityName                    = "3. Locality Name            (eg, city)     "
localityName_default            = Snake Town
0.organizationName              = "4. Organization Name        (eg, company)  "
0.organizationName_default      = Snake Oil, Ltd
organizationalUnitName          = "5. Organizational Unit Name (eg, section)  "
organizationalUnitName_default  = Webserver Team
commonName                      = "6. Common Name              (eg, FQDN)     "
commonName_max                  = 64
commonName_default              = www.snakeoil.dom
emailAddress                    = "7. Email Address            (eg, name@fqdn)"
emailAddress_max                = 40
emailAddress_default            = www@snakeoil.dom
EOT
        $ssleay req -config .mkcert.cfg \
                    -new \
                    -key $sslkeydir/server.key \
                    -out $sslcsrdir/server.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 6: Generating X.509 certificate signed by own CA [server.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            certversion=3
        else
            certversion=1
        fi
        if [ ! -f .mkcert.serial ]; then
            echo '01' >.mkcert.serial
        fi
        $ssleay x509 -days 365 \
                     -CAserial .mkcert.serial \
                     -CA    $sslcrtdir/ca.crt \
                     -CAkey $sslkeydir/ca.key \
                     -in    $sslcsrdir/server.csr -req \
                     -out   $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate X.509 certificate" 1>&2
            exit 1
        fi
        if [ ".$certversion" = .3 ]; then
            $cafix -nscertype 0x40 \
                   -nobscrit \
                   -nosign \
                   -in    $sslcrtdir/server.crt \
                   -inkey $sslkeydir/server.key \
                   -out   $sslcrtdir/server.crt.fixed
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to fixup X.509 certificate contents" 1>&2
                exit 1
            fi
            cp $sslcrtdir/server.crt.fixed $sslcrtdir/server.crt
            rm -f $sslcrtdir/server.crt.fixed
            $cafix -inkey $sslkeydir/ca.key \
                   -in    $sslcrtdir/server.crt \
                   -out   $sslcrtdir/server.crt.fixed
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to fixup X.509 certificate signature" 1>&2
                exit 1
            fi
            cp $sslcrtdir/server.crt.fixed $sslcrtdir/server.crt
            rm -f $sslcrtdir/server.crt.fixed
        fi
        echo "Verify: matching certificate & key modulus"
        modcrt=`$ssleay x509 -noout -modulus -in $sslcrtdir/server.crt`
        modkey=`$ssleay rsa  -noout -modulus -in $sslkeydir/server.key`
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        $ssleay verify -CAfile $sslcrtdir/ca.crt $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 7: Enrypting RSA private key of CA with a pass phrase for security [ca.key]${T_ME}"
        echo "The contents of the ca.key file (the generated private key) has to be"
        echo "kept secret. So we strongly recommend you to encrypt the server.key file"
        echo "with a Triple-DES cipher and a Pass Phrase."
        while [ 1 ]; do
            echo dummy | awk '{ printf("Encrypt the private key now? [Y/n]: "); }'
            read rc
            if [ ".$rc" = .n -o  ".$rc" = .N ]; then
                rc="n"
                break
            fi
            if [ ".$rc" = .y -o  ".$rc" = .Y -o ".$rc" = . ]; then
                rc="y"
                break
            fi
        done
        if [ ".$rc" = .y ]; then
            $ssleay rsa -des3 \
                        -in  $sslkeydir/ca.key \
                        -out $sslkeydir/ca.key.crypt
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt RSA private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/ca.key.crypt $sslkeydir/ca.key
            rm -f $sslkeydir/ca.key.crypt
            echo "Fine, you're using an encrypted private key."
        else
            echo "Warning, you're using an unencrypted private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 8: Enrypting RSA private key of SERVER with a pass phrase for security [server.key]${T_ME}"
        echo "The contents of the server.key file (the generated private key) has to be"
        echo "kept secret. So we strongly recommend you to encrypt the server.key file"
        echo "with a Triple-DES cipher and a Pass Phrase."
        while [ 1 ]; do
            echo dummy | awk '{ printf("Encrypt the private key now? [Y/n]: "); }'
            read rc
            if [ ".$rc" = .n -o  ".$rc" = .N ]; then
                rc="n"
                break
            fi
            if [ ".$rc" = .y -o  ".$rc" = .Y -o ".$rc" = . ]; then
                rc="y"
                break
            fi
        done
        if [ ".$rc" = .y ]; then
            $ssleay rsa -des3 \
                        -in  $sslkeydir/server.key \
                        -out $sslkeydir/server.key.crypt
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt RSA private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/server.key.crypt $sslkeydir/server.key
            rm -f $sslkeydir/server.key.crypt
            echo "Fine, you're using an encrypted private key."
        else
            echo "Warning, you're using an unencrypted private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}RESULT: Server and CA Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/ca.key${T_ME}"
        echo "   The PEM-encoded RSA private certificate file of the CA which you can"
        echo "   use to sign other servers or clients. ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/ca.crt${T_ME}"
        echo "   The PEM-encoded X.509 CA server certificate file which you use to"
        echo "   sign other servers or clients. When you sign clients with it (for"
        echo "   SSL client authentication) you can configure this file with the"
        echo "   'SSLCACertificateFile' directive."
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded RSA private certificate file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 server certificate file which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "o  ${T_MD}conf/ssl.csr/server.csr${T_ME}"
        echo "   The PEM-encoded X.509 certificate signing request file which" 
        echo "   you can send to an official Certificate Authority (CA) in order"
        echo "   to request a real server certificate (signed by this CA instead"
        echo "   of our own CA) which later can replace the conf/ssl.crt/server.crt"
        echo "   file."
        echo ""
        echo "Congratulations that you establish your server with real certificates."
        echo ""
        ;;

    existing)
        echo ""
        echo "${T_MD}Using existing custom certificate [EXISTING]${T_ME}"
        echo "______________________________________________________________________"
        echo ""
        if [ ".$crt" = . ]; then
            echo "mkcert.sh: No certificate file given" 1>&2
            exit 1
        fi
        if [ ! -f "$crt" ]; then
            echo "mkcert.sh: Cannot find certificate file: $crt" 1>&2
            exit 1
        fi
        if [ ".$key" != . ]; then
            if [ ! -f "$key" ]; then
                echo "mkcert.sh: Cannot find private key file: $key" 1>&2
                exit 1
            fi
            cp $crt $sslcrtdir/server.crt
            cp $key $sslkeydir/server.key
        else
            key=$crt
            sed -e '/-----BEGIN CERTIFICATE/,/-----END CERTIFICATE/p' -e '/.*/d' \
                <$crt >$sslcrtdir/server.crt
            sed -e '/-----BEGIN RSA PRIVATE KEY/,/-----END RSA PRIVATE KEY/p' -e '/.*/d' \
                <$key >$sslkeydir/server.key
        fi
        $ssleay x509 -noout -in $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to check certificate contents: $crt" 1>&2
            exit 1
        fi
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded RSA private certificate file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 server certificate file which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "Congratulations that you establish your server with real certificates."
        echo ""
        ;;

esac

##EOF##
