#	$OpenBSD: Makefile,v 1.35 2018/11/11 07:13:03 tb Exp $

SUBDIR += aead
SUBDIR += aeswrap
SUBDIR += asn1
SUBDIR += base64
SUBDIR += bf
SUBDIR += bio
SUBDIR += bn
SUBDIR += cast
SUBDIR += chacha
SUBDIR += cts128
SUBDIR += curve25519
SUBDIR += des
SUBDIR += dh
SUBDIR += dsa
SUBDIR += ec
SUBDIR += ecdh
SUBDIR += ecdsa
SUBDIR += engine
SUBDIR += evp
SUBDIR += exp
SUBDIR += free
SUBDIR += gcm128
SUBDIR += gost
SUBDIR += hkdf
SUBDIR += hmac
SUBDIR += idea
SUBDIR += ige
SUBDIR += init
SUBDIR += md4
SUBDIR += md5
SUBDIR += pbkdf2
SUBDIR += pkcs7
SUBDIR += poly1305
SUBDIR += rand
SUBDIR += rc2
SUBDIR += rc4
SUBDIR += rmd
SUBDIR += rsa
SUBDIR += sha1
SUBDIR += sha2
SUBDIR += sha256
SUBDIR += sha512
SUBDIR += sm3
SUBDIR += utf8
SUBDIR += wycheproof
SUBDIR += x509
# botan tests take 50 seconds, move to the end to fail early in other tests
SUBDIR += botan

install:

.include <bsd.subdir.mk>
