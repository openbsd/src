#!/bin/sh
#       $OpenBSD: kyber768.sh,v 1.7 2023/01/11 02:13:52 djm Exp $
#       Placed in the Public Domain.
#
AUTHOR="supercop-20230530/crypto_kem/kyber768/designers" # FIXME
AUTHOR=/dev/null	# FIXME
# supercop-20230530/crypto_kem/kyber768/ref/api.h
FILES="
	supercop-20230530/crypto_kem/kyber768/ref/params.h
	supercop-20230530/crypto_kem/kyber768/ref/poly.h
	supercop-20230530/crypto_kem/kyber768/ref/polyvec.h
	supercop-20230530/crypto_kem/kyber768/ref/cbd.h
	supercop-20230530/crypto_kem/kyber768/ref/fips202.h
	supercop-20230530/crypto_kem/kyber768/ref/indcpa.h
	supercop-20230530/crypto_kem/kyber768/ref/kem.h
	supercop-20230530/crypto_kem/kyber768/ref/ntt.h
	supercop-20230530/crypto_kem/kyber768/ref/reduce.h
	supercop-20230530/crypto_kem/kyber768/ref/symmetric.h
	supercop-20230530/crypto_kem/kyber768/ref/verify.h
	supercop-20230530/crypto_kem/kyber768/ref/cbd.c
	supercop-20230530/crypto_kem/kyber768/ref/fips202.c
	supercop-20230530/crypto_kem/kyber768/ref/indcpa.c
	supercop-20230530/crypto_kem/kyber768/ref/kem.c
	supercop-20230530/crypto_kem/kyber768/ref/ntt.c
	supercop-20230530/crypto_kem/kyber768/ref/poly.c
	supercop-20230530/crypto_kem/kyber768/ref/polyvec.c
	supercop-20230530/crypto_kem/kyber768/ref/reduce.c
	supercop-20230530/crypto_kem/kyber768/ref/symmetric-shake.c
	supercop-20230530/crypto_kem/kyber768/ref/verify.c
"
###

set -e
cd $1
echo -n '/*  $'
echo 'OpenBSD: $ */'
echo
echo '/*'
echo ' * Public Domain, Authors:'
sed -e '/Alphabetical order:/d' -e 's/^/ * - /' < $AUTHOR
echo ' */'
echo
echo '#include <string.h>'
echo '#include "crypto_api.h"'
echo
# Map the types used in this code to the ones in crypto_api.h.  We use #define
# instead of typedef since some systems have existing intXX types and do not
# permit multiple typedefs even if they do not conflict.
for t in int8 uint8 int16 uint16 int32 uint32 int64 uint64; do
	echo "#define $t crypto_${t}"
done
echo
for i in $FILES; do
	echo "/* from $i */"
	# Changes to all files:
	#  - remove all includes, we inline everything required.
	#  - make functions not required elsewhere static.
	#  - rename the functions we do use.
	#  - remove unnecessary defines and externs.
	sed -e "/#include/d" \
	    -e "s/crypto_kem_/crypto_kem_kyber768_/g" \
	    -e "s/^void /static void /g" \
	    -e "s/^int16 /static int16 /g" \
	    -e "s/^uint16 /static uint16 /g" \
	    -e "/^extern /d" \
	    -e '/define.*_NAMESPACE/d' \
	    -e "/^#define int32 crypto_int32/d" \
	    -e 's/[	 ]*$//' \
	    $i | \
	case "$i" in
	# Remove unused function to prevent warning.
	*/crypto_kem/kyber768/ref/fips202.c)
	    sed \
		-e '/int keccak_absorb(/,/^}$/d' \
		-e '/void keccak_finalize(.*)$/,/^$/d' \
		-e '/void keccak_init(.*)$/,/^$/d' \
		-e '/void shake128(.*)$/,/^}$/d' \
		-e '/void shake128_absorb(.*)$/,/^$/d' \
		-e '/void shake128_finalize(.*)$/,/^$/d' \
		-e '/void shake128_init(.*)$/,/^$/d' \
		-e '/void shake128_squeeze(.*)$/,/^$/d' \
		-e '/void shake256_absorb(.*)$/,/^$/d' \
		-e '/void shake256_finalize(.*)$/,/^$/d' \
		-e '/void shake256_init(.*)$/,/^$/d'
	    ;;
	*/crypto_kem/kyber768/ref/fips202.h)
	    sed \
		-e '/void keccak_finalize(/d' \
		-e '/void keccak_init(/d' \
		-e '/void shake128(/d' \
		-e '/void shake128_absorb(/d' \
		-e '/void shake128_finalize(/d' \
		-e '/void shake128_init(/d' \
		-e '/void shake128_squeeze(/d' \
		-e '/void shake256_absorb(/d' \
		-e '/void shake256_finalize(/d' \
		-e '/void shake256_init(/d'
	    ;;
	# Default: pass through.
	*)
	    cat
	    ;;
	esac
	echo
done
