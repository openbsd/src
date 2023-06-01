#	$OpenBSD: Makefile,v 1.9 2023/06/01 09:46:00 tb Exp $

SUBDIR= options x509

CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem
CLEANFILES+= appstest_dir

REGRESS_TARGETS=ssl-enc ssl-dsa ssl-rsa appstest

OPENSSL=/usr/bin/openssl
CLEAR1=p
CIPHER=cipher
CLEAR2=clear
LIBCRYPTO=-lcrypto

${CLEAR1}: openssl.cnf
	cat ${.CURDIR}/openssl.cnf > ${CLEAR1}

CLEANFILES+=${CLEAR1}

ENCTARGETS=aes-128-cbc aes-128-cfb aes-128-cfb1 aes-128-cfb8
ENCTARGETS+=aes-128-ecb aes-128-ofb aes-192-cbc aes-192-cfb
ENCTARGETS+=aes-192-cfb1 aes-192-cfb8 aes-192-ecb aes-192-ofb
ENCTARGETS+=aes-256-cbc aes-256-cfb aes-256-cfb1 aes-256-cfb8
ENCTARGETS+=aes-256-ecb aes-256-ofb
ENCTARGETS+=bf-cbc bf-cfb bf-ecb bf-ofb
ENCTARGETS+=cast-cbc cast5-cbc cast5-cfb cast5-ecb cast5-ofb
ENCTARGETS+=des-cbc des-cfb des-cfb8 des-ecb des-ede
ENCTARGETS+=des-ede-cbc des-ede-cfb des-ede-ofb des-ede3
ENCTARGETS+=des-ede3-cbc des-ede3-cfb des-ede3-ofb des-ofb desx-cbc
ENCTARGETS+=rc2-40-cbc rc2-64-cbc rc2-cbc rc2-cfb rc2-ecb rc2-ofb
ENCTARGETS+=rc4 rc4-40

.for ENC in ${ENCTARGETS}
${CIPHER}.${ENC}: ${CLEAR1}
	${OPENSSL} enc -${ENC} -bufsize 113 -e -k test < ${CLEAR1} > ${CIPHER}.${ENC}
${CIPHER}.${ENC}.b64: ${CLEAR1}
	${OPENSSL} enc -${ENC} -bufsize 113 -a -e -k test < ${CLEAR1} > ${CIPHER}.${ENC}.b64

${CLEAR2}.${ENC}: ${CIPHER}.${ENC}
	${OPENSSL} enc -${ENC} -bufsize 157 -d -k test < ${CIPHER}.${ENC} > ${CLEAR2}.${ENC}
${CLEAR2}.${ENC}.b64: ${CIPHER}.${ENC}.b64
	${OPENSSL} enc -${ENC} -bufsize 157 -a -d -k test < ${CIPHER}.${ENC}.b64 > ${CLEAR2}.${ENC}.b64

ssl-enc-${ENC}: ${CLEAR1} ${CLEAR2}.${ENC}
	cmp ${CLEAR1} ${CLEAR2}.${ENC}
ssl-enc-${ENC}.b64: ${CLEAR1} ${CLEAR2}.${ENC}.b64
	cmp ${CLEAR1} ${CLEAR2}.${ENC}.b64

REGRESS_TARGETS+=ssl-enc-${ENC} ssl-enc-${ENC}.b64
CLEANFILES+=${CIPHER}.${ENC} ${CIPHER}.${ENC}.b64 ${CLEAR2}.${ENC} ${CLEAR2}.${ENC}.b64
.endfor

ssl-enc:
	env OPENSSL=${OPENSSL} sh ${.CURDIR}/testenc.sh ${.OBJDIR} ${.CURDIR}
ssl-dsa:
	env OPENSSL=${OPENSSL} sh ${.CURDIR}/testdsa.sh ${.OBJDIR} ${.CURDIR}
ssl-rsa:
	env OPENSSL=${OPENSSL} sh ${.CURDIR}/testrsa.sh ${.OBJDIR} ${.CURDIR}
appstest:
	env OPENSSL=${OPENSSL} sh ${.CURDIR}/appstest.sh -q

clean:
	rm -rf ${CLEANFILES}

.include <bsd.regress.mk>
