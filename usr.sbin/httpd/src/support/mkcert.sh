#!/bin/sh
##
##  mkcert.sh -- Make SSL Certificate Files for `make certificate' command
##  Copyright (c) 1998-2000 Ralf S. Engelschall, All Rights Reserved. 
##

#   parameters
make="$1"
mflags="$2"
openssl="$3"
support="$4"
type="$5"
algo="$6"
crt="$7"
key="$8"
view="$9"

#   we can operate only inside the Apache 1.3 source
#   tree and only when mod_ssl+OpenSSL is actually configured.
if [ ! -f "../README.configure" ]; then
    echo "mkcert.sh:Error: Cannot operate outside the Apache 1.3 source tree." 1>&2
    echo "mkcert.sh:Hint:  You have to stay inside apache_1.3.x/src." 1>&2
    exit 1
fi
if [ ".$openssl" = . ]; then
    echo "mkcert.sh:Error: mod_ssl/OpenSSL has to be configured before using this utility." 1>&2
    echo "mkcert.sh:Hint:  Configure mod_ssl with --enable-module=ssl in APACI, first." 1>&2
    exit 1
fi

#   configuration
#   WE ARE CALLED FROM THE PARENT DIR!
sslcrtdir="../conf/ssl.crt"
sslcsrdir="../conf/ssl.csr"
sslkeydir="../conf/ssl.key"
sslprmdir="../conf/ssl.prm"

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
echo "Copyright (c) 1998-2000 Ralf S. Engelschall, All Rights Reserved."

#   on request view certificates only
if [ ".$view" != . ]; then
    if [ -f "$sslcrtdir/ca.crt" -a -f "$sslkeydir/ca.key" ]; then
        echo ""
        echo "${T_MD}CA X.509 Certificate${T_ME} [ca.crt]"
        echo "______________________________________________________________________"
        $openssl x509 -noout -text -in $sslcrtdir/ca.crt
        echo ""
        if [ ".`$openssl x509 -noout -text -in $sslcrtdir/ca.crt | grep 'Signature Algorithm' | grep -i RSA`" != . ]; then
            echo "${T_MD}CA RSA Private Key${T_ME} [ca.key]"
            echo "______________________________________________________________________"
            $openssl rsa -noout -text -in $sslkeydir/ca.key
        else
            echo "${T_MD}CA DSA Private Key${T_ME} [ca.key]"
            echo "______________________________________________________________________"
            $openssl dsa -noout -text -in $sslkeydir/ca.key
        fi
    fi
    if [ -f "$sslcrtdir/server.crt" -a -f "$sslkeydir/server.key" ]; then
        echo ""
        echo "${T_MD}Server X.509 Certificate${T_ME} [server.crt]"
        echo "______________________________________________________________________"
        $openssl x509 -noout -text -in $sslcrtdir/server.crt
        echo ""
        if [ ".`$openssl x509 -noout -text -in $sslcrtdir/server.crt | grep 'Signature Algorithm' | grep -i RSA`" != . ]; then
            echo "${T_MD}Server RSA Private Key${T_ME} [server.key]"
            echo "______________________________________________________________________"
            $openssl rsa -noout -text -in $sslkeydir/server.key
        else
            echo "${T_MD}Server DSA Private Key${T_ME} [server.key]"
            echo "______________________________________________________________________"
            $openssl dsa -noout -text -in $sslkeydir/server.key
        fi
    fi
    exit 0
fi

#   find some random files
#   (do not use /dev/random here, because this device 
#   doesn't work as expected on all platforms)
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

#   canonicalize parameters
case "x$type" in
    x ) type=test ;;
esac
case "x$algo" in
    xRSA|xrsa ) 
        algo=RSA
        ;;
    xDSA|xdsa ) 
        algo=DSA 
        ;;
    x ) 
        algo=choose
        ;;
    * ) echo "Unknown algorithm \'$algo' (use RSA or DSA!)" 1>&2
        exit 1
        ;;
esac

#   processing
case $type in

    dummy)
        echo ""
        echo "${T_MD}Generating self-signed Snake Oil certificate [DUMMY]${T_ME}"
        echo "______________________________________________________________________"
        echo ""
        if [ ".$algo" = .choose ]; then
            algo=RSA
        fi
        if [ ".$algo" = .RSA ]; then
            cp $sslcrtdir/snakeoil-rsa.crt $sslcrtdir/server.crt
            cp $sslkeydir/snakeoil-rsa.key $sslkeydir/server.key
        else
            cp $sslcrtdir/snakeoil-dsa.crt $sslcrtdir/server.crt
            cp $sslkeydir/snakeoil-dsa.key $sslkeydir/server.key
        fi
        chmod 600 $sslkeydir/server.key
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded $algo private key file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 certificate file which you configure"
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
        if [ ".$algo" = .choose ]; then
            echo "______________________________________________________________________"
            echo ""
            echo "${T_MD}STEP 0: Decide the signature algorithm used for certificate${T_ME}"
            echo "The generated X.509 CA certificate can contain either"
            echo "RSA or DSA based ingredients. Select the one you want to use."
            def1=R def2=r def=RSA
            prompt="Signature Algorithm ((R)SA or (D)SA) [$def1]:"
            while [ 1 ]; do
                echo dummy | awk '{ printf("%s", prompt); }' "prompt=$prompt"
                read algo
                if [ ".$algo" = ".$def1" -o ".$algo" = ".$def2" -o ".$algo" = . ]; then
                    algo=$def
                    break
                elif [ ".$algo" = ".R" -o ".$algo" = ".r" ]; then
                    algo=RSA
                    break
                elif [ ".$algo" = ".D" -o ".$algo" = ".d" ]; then
                    algo=DSA
                    break
                else
                    echo "mkcert.sh:Warning: Invalid selection" 1>&2
                fi
            done
        fi
        if [ ".$algo" = ".DSA" ]; then
            echo ""
            echo "${T_MD}WARNING!${T_ME} You're generating a DSA based certificate/key pair."
            echo "         This implies that RSA based ciphers won't be available later,"
            echo "         which for your web server currently still means that mostly all"
            echo "         popular web browsers cannot connect to it. At least not until"
            echo "         you also generate an additional RSA based certificate/key pair"
            echo "         and configure them in parallel."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 1: Generating $algo private key (1024 bit) [server.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$algo" = .RSA ]; then
            if [ ".$randfiles" != . ]; then
                $openssl genrsa -rand $randfiles -out $sslkeydir/server.key 1024
            else
                $openssl genrsa -out $sslkeydir/server.key 1024
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
                exit 1
            fi
        else
            echo "Generating DSA private key via SnakeOil CA DSA parameters"
            if [ ".$randfiles" != . ]; then
                $openssl gendsa -rand $randfiles -out $sslkeydir/server.key $sslprmdir/snakeoil-ca-dsa.prm
            else
                $openssl gendsa -out $sslkeydir/server.key $sslprmdir/snakeoil-ca-dsa.prm
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate DSA private key" 1>&2
                exit 1
            fi
        fi
        chmod 600 $sslkeydir/server.key
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
        $openssl req -config .mkcert.cfg \
                     -new \
                     -key $sslkeydir/server.key \
                     -out $sslcsrdir/server.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        prompt="8. Certificate Validity     (days)          [365]:"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=$prompt"
        read days
        if [ ".$days" = . ]; then
            days=365
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 3: Generating X.509 certificate signed by Snake Oil CA [server.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        extfile=""
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            extfile="-extfile .mkcert.cfg"
            cat >.mkcert.cfg <<EOT
extensions = x509v3
[ x509v3 ]
subjectAltName   = email:copy
nsComment        = "mod_ssl generated test server certificate"
nsCertType       = server
EOT
        fi
        if [ ! -f .mkcert.serial ]; then
            echo '01' >.mkcert.serial
        fi
        if [ ".$algo" = .RSA ]; then
            $openssl x509 $extfile \
                          -days $days \
                          -CAserial .mkcert.serial \
                          -CA $sslcrtdir/snakeoil-ca-rsa.crt \
                          -CAkey $sslkeydir/snakeoil-ca-rsa.key \
                          -in $sslcsrdir/server.csr -req \
                          -out $sslcrtdir/server.crt
        else
            $openssl x509 $extfile \
                          -days $days \
                          -CAserial .mkcert.serial \
                          -CA $sslcrtdir/snakeoil-ca-dsa.crt \
                          -CAkey $sslkeydir/snakeoil-ca-dsa.key \
                          -in $sslcsrdir/server.csr -req \
                          -out $sslcrtdir/server.crt
        fi
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate X.509 certificate" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "Verify: matching certificate & key modulus"
        modcrt=`$openssl x509 -noout -modulus -in $sslcrtdir/server.crt | sed -e 's;.*Modulus=;;'`
        if [ ".$algo" = .RSA ]; then
            modkey=`$openssl rsa -noout -modulus -in $sslkeydir/server.key | sed -e 's;.*Modulus=;;'`
        else
            modkey=`$openssl dsa -noout -modulus -in $sslkeydir/server.key | sed -e 's;.*Key=;;'`
        fi
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        if [ ".$algo" = .RSA ]; then
            $openssl verify -CAfile $sslcrtdir/snakeoil-ca-rsa.crt $sslcrtdir/server.crt
        else
            $openssl verify -CAfile $sslcrtdir/snakeoil-ca-dsa.crt $sslcrtdir/server.crt
        fi
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 4: Enrypting $algo private key with a pass phrase for security [server.key]${T_ME}"
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
            if [ ".$algo" = .RSA ]; then
                $openssl rsa -des3 \
                             -in  $sslkeydir/server.key \
                             -out $sslkeydir/server.key.crypt
            else
                $openssl dsa -des3 \
                             -in  $sslkeydir/server.key \
                             -out $sslkeydir/server.key.crypt
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt $algo private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/server.key.crypt $sslkeydir/server.key
            rm -f $sslkeydir/server.key.crypt
            chmod 600 $sslkeydir/server.key
            echo "Fine, you're using an encrypted $algo private key."
        else
            echo "Warning, you're using an unencrypted $algo private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded $algo private key file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 certificate file which you configure"
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
        if [ ".$algo" = .choose ]; then
            echo "______________________________________________________________________"
            echo ""
            echo "${T_MD}STEP 0: Decide the signature algorithm used for certificates${T_ME}"
            echo "The generated X.509 certificates can contain either"
            echo "RSA or DSA based ingredients. Select the one you want to use."
            def1=R def2=r def=RSA
            prompt="Signature Algorithm ((R)SA or (D)SA) [$def1]:"
            while [ 1 ]; do
                echo dummy | awk '{ printf("%s", prompt); }' "prompt=$prompt"
                read algo
                if [ ".$algo" = ".$def1" -o ".$algo" = ".$def2" -o ".$algo" = . ]; then
                    algo=$def
                    break
                elif [ ".$algo" = ".R" -o ".$algo" = ".r" ]; then
                    algo=RSA
                    break
                elif [ ".$algo" = ".D" -o ".$algo" = ".d" ]; then
                    algo=DSA
                    break
                else
                    echo "mkcert.sh:Warning: Invalid selection" 1>&2
                fi
            done
        fi
        if [ ".$algo" = ".DSA" ]; then
            echo ""
            echo "${T_MD}WARNING!${T_ME} You're generating DSA based certificate/key pairs."
            echo "         This implies that RSA based ciphers won't be available later,"
            echo "         which for your web server currently still means that mostly all"
            echo "         popular web browsers cannot connect to it. At least not until"
            echo "         you also generate an additional RSA based certificate/key pair"
            echo "         and configure them in parallel."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 1: Generating $algo private key for CA (1024 bit) [ca.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$algo" = .RSA ]; then
            if [ ".$randfiles" != . ]; then
                $openssl genrsa -rand $randfiles -out $sslkeydir/ca.key 1024
            else
                $openssl genrsa -out $sslkeydir/ca.key 1024
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
                exit 1
            fi
        else
            if [ ".$randfiles" != . ]; then
                $openssl dsaparam -rand $randfiles -out $sslprmdir/ca.prm 1024
                echo "Generating DSA private key:"
                $openssl gendsa   -rand $randfiles -out $sslkeydir/ca.key $sslprmdir/ca.prm
            else
                $openssl dsaparam -out $sslprmdir/ca.prm 1024
                echo "Generating DSA private key:"
                $openssl gendsa   -out $sslkeydir/ca.key $sslprmdir/ca.prm
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate DSA private key" 1>&2
                exit 1
            fi
        fi
        chmod 600 $sslkeydir/ca.key
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
        $openssl req -config .mkcert.cfg \
                     -new \
                     -key $sslkeydir/ca.key \
                     -out $sslcsrdir/ca.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        prompt="8. Certificate Validity     (days)          [365]:"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=$prompt"
        read days
        if [ ".$days" = . ]; then
            days=365
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 3: Generating X.509 certificate for CA signed by itself [ca.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        extfile=""
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            extfile="-extfile .mkcert.cfg"
            cat >.mkcert.cfg <<EOT
extensions = x509v3
[ x509v3 ]
subjectAltName   = email:copy
basicConstraints = CA:true,pathlen:0
nsComment        = "mod_ssl generated custom CA certificate"
nsCertType       = sslCA
EOT
        fi
        $openssl x509 $extfile \
                      -days $days \
                      -signkey $sslkeydir/ca.key \
                      -in      $sslcsrdir/ca.csr -req \
                      -out     $sslcrtdir/ca.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate self-signed CA certificate" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "Verify: matching certificate & key modulus"
        modcrt=`$openssl x509 -noout -modulus -in $sslcrtdir/ca.crt | sed -e 's;.*Modulus=;;'`
        if [ ".$algo" = .RSA ]; then
            modkey=`$openssl rsa -noout -modulus -in $sslkeydir/ca.key | sed -e 's;.*Modulus=;;'`
        else
            modkey=`$openssl dsa -noout -modulus -in $sslkeydir/ca.key | sed -e 's;.*Key=;;'`
        fi
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        $openssl verify $sslcrtdir/ca.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 4: Generating $algo private key for SERVER (1024 bit) [server.key]${T_ME}"
        if [ ! -f $HOME/.rnd ]; then
            touch $HOME/.rnd
        fi
        if [ ".$algo" = .RSA ]; then
            if [ ".$randfiles" != . ]; then
                $openssl genrsa -rand $randfiles -out $sslkeydir/server.key 1024
            else
                $openssl genrsa -out $sslkeydir/server.key 1024
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate RSA private key" 1>&2
                exit 1
            fi
        else
            if [ ".$randfiles" != . ]; then
                $openssl gendsa -rand $randfiles -out $sslkeydir/server.key $sslprmdir/ca.prm
            else
                $openssl gendsa -out $sslkeydir/server.key $sslprmdir/ca.prm
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to generate DSA private key" 1>&2
                exit 1
            fi
        fi
        chmod 600 $sslkeydir/server.key
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
        $openssl req -config .mkcert.cfg \
                     -new \
                     -key $sslkeydir/server.key \
                     -out $sslcsrdir/server.csr
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate certificate signing request" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        prompt="8. Certificate Validity     (days)          [365]:"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=$prompt"
        read days
        if [ ".$days" = . ]; then
            days=365
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 6: Generating X.509 certificate signed by own CA [server.crt]${T_ME}"
        echo dummy | awk '{ printf("%s", prompt); }' "prompt=Certificate Version (1 or 3) [3]:"
        read certversion
        extfile=""
        if [ ".$certversion" = .3 -o ".$certversion" = . ]; then
            extfile="-extfile .mkcert.cfg"
            cat >.mkcert.cfg <<EOT
extensions = x509v3
[ x509v3 ]
subjectAltName   = email:copy
nsComment        = "mod_ssl generated custom server certificate"
nsCertType       = server
EOT
        fi
        if [ ! -f .mkcert.serial ]; then
            echo '01' >.mkcert.serial
        fi
        $openssl x509 $extfile \
                      -days $days \
                      -CAserial .mkcert.serial \
                      -CA    $sslcrtdir/ca.crt \
                      -CAkey $sslkeydir/ca.key \
                      -in    $sslcsrdir/server.csr -req \
                      -out   $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to generate X.509 certificate" 1>&2
            exit 1
        fi
        rm -f .mkcert.cfg
        echo "Verify: matching certificate & key modulus"
        modcrt=`$openssl x509 -noout -modulus -in $sslcrtdir/server.crt | sed -e 's;.*Modulus=;;'`
        if [ ".$algo" = .RSA ]; then
            modkey=`$openssl rsa -noout -modulus -in $sslkeydir/server.key | sed -e 's;.*Modulus=;;'`
        else
            modkey=`$openssl dsa -noout -modulus -in $sslkeydir/server.key | sed -e 's;.*Key=;;'`
        fi
        if [ ".$modcrt" != ".$modkey" ]; then
            echo "mkcert.sh:Error: Failed to verify modulus on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "Verify: matching certificate signature"
        $openssl verify -CAfile $sslcrtdir/ca.crt $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to verify signature on resulting X.509 certificate" 1>&2
            exit 1
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 7: Enrypting $algo private key of CA with a pass phrase for security [ca.key]${T_ME}"
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
            if [ ".$algo" = .RSA ]; then
                $openssl rsa -des3 \
                             -in  $sslkeydir/ca.key \
                             -out $sslkeydir/ca.key.crypt
            else
                $openssl dsa -des3 \
                             -in  $sslkeydir/ca.key \
                             -out $sslkeydir/ca.key.crypt
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt $algo private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/ca.key.crypt $sslkeydir/ca.key
            rm -f $sslkeydir/ca.key.crypt
            chmod 600 $sslkeydir/ca.key
            echo "Fine, you're using an encrypted private key."
        else
            echo "Warning, you're using an unencrypted private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}STEP 8: Enrypting $algo private key of SERVER with a pass phrase for security [server.key]${T_ME}"
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
            if [ ".$algo" = .RSA ]; then
                $openssl rsa -des3 \
                             -in  $sslkeydir/server.key \
                             -out $sslkeydir/server.key.crypt
            else
                $openssl dsa -des3 \
                             -in  $sslkeydir/server.key \
                             -out $sslkeydir/server.key.crypt
            fi
            if [ $? -ne 0 ]; then
                echo "mkcert.sh:Error: Failed to encrypt $algo private key" 1>&2
                exit 1
            fi
            cp $sslkeydir/server.key.crypt $sslkeydir/server.key
            rm -f $sslkeydir/server.key.crypt
            chmod 600 $sslkeydir/server.key
            echo "Fine, you're using an encrypted $algo private key."
        else
            echo "Warning, you're using an unencrypted $algo private key."
            echo "Please notice this fact and do this on your own risk."
        fi
        echo "______________________________________________________________________"
        echo ""
        echo "${T_MD}RESULT: CA and Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/ca.key${T_ME}"
        echo "   The PEM-encoded $algo private key file of the CA which you can"
        echo "   use to sign other servers or clients. ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/ca.crt${T_ME}"
        echo "   The PEM-encoded X.509 certificate file of the CA which you use to"
        echo "   sign other servers or clients. When you sign clients with it (for"
        echo "   SSL client authentication) you can configure this file with the"
        echo "   'SSLCACertificateFile' directive."
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded $algo private key file of the server which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 certificate file of the server which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "o  ${T_MD}conf/ssl.csr/server.csr${T_ME}"
        echo "   The PEM-encoded X.509 certificate signing request of the server file which" 
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
            sed -e '/-----BEGIN ... PRIVATE KEY/,/-----END ... PRIVATE KEY/p' -e '/.*/d' \
                <$key >$sslkeydir/server.key
        fi
        chmod 600 $sslkeydir/server.key
        $openssl x509 -noout -in $sslcrtdir/server.crt
        if [ $? -ne 0 ]; then
            echo "mkcert.sh:Error: Failed to check certificate contents: $crt" 1>&2
            exit 1
        fi
        if [ ".`grep 'PRIVATE KEY' $sslkeydir/server.key | grep RSA`" != . ]; then
            algo=RSA
        else
            algo=DSA
        fi
        echo "${T_MD}RESULT: Server Certification Files${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.key/server.key${T_ME}"
        echo "   The PEM-encoded $algo private key file which you configure"
        echo "   with the 'SSLCertificateKeyFile' directive (automatically done"
        echo "   when you install via APACI). ${T_MD}KEEP THIS FILE PRIVATE!${T_ME}"
        echo ""
        echo "o  ${T_MD}conf/ssl.crt/server.crt${T_ME}"
        echo "   The PEM-encoded X.509 certificate file which you configure"
        echo "   with the 'SSLCertificateFile' directive (automatically done"
        echo "   when you install via APACI)."
        echo ""
        echo "Congratulations that you establish your server with real certificates."
        echo ""
        ;;

esac

##EOF##
