#	$OpenBSD: Makefile,v 1.15 2014/04/16 08:29:22 jsg Exp $

CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

REGRESS_TARGETS=ossltests ssl-enc ssl-dsa ssl-rsa 

OPENSSL=/usr/sbin/openssl
CLEAR1=p
CIPHER=cipher
CLEAR2=clear
LIBCRYPTO=-lcrypto

BFTEST=		bftest
BNTEST=		bntest
CASTTEST=	casttest
DESTEST=	destest
DHTEST=		dhtest
DSATEST=	dsatest
ECDHTEST=	ecdhtest
ECDSATEST=	ecdsatest
ECTEST=		ectest
ENGINETEST=	enginetest
EVPTEST=	evptest
EXPTEST=	exptest
HMACTEST=	hmactest
IGETEST=	igetest
MD4TEST=	md4test
MD5TEST=	md5test
RANDTEST=	randtest
RC2TEST=	rc2test
RC4TEST=	rc4test
RMDTEST=	rmdtest
SHA1TEST=	sha1test
SHA256TEST=	sha256t
SHA512TEST=	sha512t
SHATEST=	shatest

SSLDIR=	${.CURDIR}/../../../lib/libssl/src/crypto
CRYPTODIR= ${.CURDIR}/../libcrypto
CFLAGS+= -Wall -I${SSLDIR} -I${SSLDIR}/..

CLEANFILES+=	$(BNTEST).c $(ECTEST).c $(HMACTEST).c \
	$(SHATEST).c $(SHA1TEST).c $(MDC2TEST).c $(RMDTEST).c \
	$(RANDTEST).c $(DHTEST).c $(ENGINETEST).c \
	$(CASTTEST).c $(EXPTEST).c $(DSATEST).c \
	$(EVPTEST).c $(DESTEST).c ${RC2TEST}.c ${RC4TEST}.c \
	${MD4TEST}.c ${MD5TEST}.c ${BFTEST}.c ${ECDHTEST}.c ${ECDSATEST}.c \
	${IGETEST}.c ${SHA256TEST}.c ${SHA512TEST}.c

CLEANFILES+=	$(BNTEST) $(ECTEST) $(HMACTEST) \
	$(SHATEST) $(SHA1TEST) $(MDC2TEST) $(RMDTEST) \
	$(RANDTEST) $(DHTEST) $(ENGINETEST) \
	$(CASTTEST) $(EXPTEST) $(DSATEST) \
	$(EVPTEST) $(DESTEST) ${RC2TEST} ${RC4TEST} ${MD4TEST} \
	${MD5TEST} ${BFTEST} ${ECDHTEST} ${ECDSATEST} ${IGETEST} \
	${SHA256TEST} ${SHA512TEST}

CLEANFILES+= ${BNTEST}.out

OTESTS= ${ENGINETEST}  \
	${EXPTEST} ${RANDTEST} \
	${MD4TEST} ${MD5TEST} \
	${SHATEST} ${SHA1TEST} ${HMACTEST} ${RMDTEST} ${MDC2TEST} \
	${CASTTEST} ${BFTEST} ${RC2TEST} ${RC4TEST} ${DESTEST} \
	${DHTEST} ${DSATEST} \
	${ECTEST} ${ECDHTEST} ${ECDSATEST} $(IGETEST) \
	$(SHA256TEST) $(SHA512TEST)

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
CLEANFILES+=${CIPHER}.${ENC} ${CIPHER}.${ENC}.b64 ${CLEAR2}.${ENC} ${CLEAR2}.${ENC}.b64 .rnd
.endfor

ssl-enc:
	sh ${.CURDIR}/testenc.sh ${.OBJDIR} ${.CURDIR}
ssl-dsa:
	sh ${.CURDIR}/testdsa.sh ${.OBJDIR} ${.CURDIR}
ssl-rsa:
	sh ${.CURDIR}/testrsa.sh ${.OBJDIR} ${.CURDIR}

ossltests: ${OTESTS} ${BNTEST} ${EVPTEST}
	@echo running ${BNTEST}, check ${.OBJDIR}/${BNTEST}.out if this fails.
	${.OBJDIR}/${BNTEST} > ${.OBJDIR}/${BNTEST}.out 2>&1
.for OT in ${OTESTS}
	@echo running ${OT}
	${.OBJDIR}/${OT} 
.endfor
	@echo running ${EVPTEST}
	${.OBJDIR}/${EVPTEST} ${CRYPTODIR}/evp/evptests.txt

$(BNTEST).c: ${CRYPTODIR}/bn/bntest.c
	cp ${CRYPTODIR}/bn/bntest.c ${.OBJDIR}

$(BNTEST): ${BNTEST}.c
	cc ${CFLAGS} -o $(BNTEST) ${BNTEST}.c $(LIBCRYPTO)

$(EXPTEST).c: ${CRYPTODIR}/exp/exptest.c
	cp ${CRYPTODIR}/exp/exptest.c ${.OBJDIR}

$(EXPTEST): ${EXPTEST}.c
	cc ${CFLAGS} -o $(EXPTEST) ${EXPTEST}.c $(LIBCRYPTO)

$(ECTEST).c: ${CRYPTODIR}/ec/ectest.c
	cp ${CRYPTODIR}/ec/ectest.c ${.OBJDIR}

$(ECTEST): ${ECTEST}.c
	cc ${CFLAGS} -o $(ECTEST) ${ECTEST}.c $(LIBCRYPTO)

$(EVPTEST).c: ${CRYPTODIR}/evp/${EVPTEST}.c
	cp ${CRYPTODIR}/evp/${EVPTEST}.c ${.OBJDIR}
$(EVPTEST): ${EVPTEST}.c
	cc ${CFLAGS} -o $(EVPTEST) ${EVPTEST}.c $(LIBCRYPTO)

$(SHATEST).c: ${CRYPTODIR}/sha/${SHATEST}.c
	cp ${CRYPTODIR}/sha/${SHATEST}.c ${.OBJDIR}
$(SHATEST): ${SHATEST}.c
	cc ${CFLAGS} -o $(SHATEST) ${SHATEST}.c $(LIBCRYPTO)

$(SHA1TEST).c: ${CRYPTODIR}/sha1/${SHA1TEST}.c
	cp ${CRYPTODIR}/sha1/${SHA1TEST}.c ${.OBJDIR}
$(SHA1TEST): ${SHA1TEST}.c
	cc ${CFLAGS} -o $(SHA1TEST) ${SHA1TEST}.c $(LIBCRYPTO)

$(RANDTEST).c: ${CRYPTODIR}/rand/${RANDTEST}.c
	cp ${CRYPTODIR}/rand/${RANDTEST}.c ${.OBJDIR}
$(RANDTEST): ${RANDTEST}.c
	cc ${CFLAGS} -o $(RANDTEST) ${RANDTEST}.c $(LIBCRYPTO)

$(RMDTEST).c: ${CRYPTODIR}/rmd/${RMDTEST}.c
	cp ${CRYPTODIR}/rmd/${RMDTEST}.c ${.OBJDIR}
$(RMDTEST): ${RMDTEST}.c
	cc ${CFLAGS} -o $(RMDTEST) ${RMDTEST}.c $(LIBCRYPTO)

$(DHTEST).c: ${CRYPTODIR}/dh/${DHTEST}.c
	cp ${CRYPTODIR}/dh/${DHTEST}.c ${.OBJDIR}

$(DHTEST): ${DHTEST}.c
	cc ${CFLAGS} -o $(DHTEST) ${DHTEST}.c $(LIBCRYPTO)

$(ENGINETEST).c: ${SSLDIR}/engine/${ENGINETEST}.c
	cp ${SSLDIR}/engine/${ENGINETEST}.c ${.OBJDIR}

$(ENGINETEST): ${ENGINETEST}.c
	cc ${CFLAGS} -o $(ENGINETEST) ${ENGINETEST}.c $(LIBCRYPTO)

$(CASTTEST).c: ${CRYPTODIR}/cast/${CASTTEST}.c
	cp ${CRYPTODIR}/cast/${CASTTEST}.c ${.OBJDIR}

$(CASTTEST): ${CASTTEST}.c
	cc ${CFLAGS} -o $(CASTTEST) ${CASTTEST}.c $(LIBCRYPTO)

$(DSATEST).c: ${CRYPTODIR}/dsa/${DSATEST}.c
	cp ${CRYPTODIR}/dsa/${DSATEST}.c ${.OBJDIR}

$(DSATEST): ${DSATEST}.c
	cc ${CFLAGS} -o $(DSATEST) ${DSATEST}.c $(LIBCRYPTO)


$(HMACTEST).c: ${CRYPTODIR}/hmac/${HMACTEST}.c
	cp ${CRYPTODIR}/hmac/${HMACTEST}.c ${.OBJDIR}

$(HMACTEST): ${HMACTEST}.c
	cc ${CFLAGS} -o $(HMACTEST) ${HMACTEST}.c $(LIBCRYPTO)

$(DESTEST).c: ${CRYPTODIR}/des/${DESTEST}.c
	cp ${CRYPTODIR}/des/${DESTEST}.c ${.OBJDIR}

$(DESTEST): ${DESTEST}.c
	cc ${CFLAGS} -o $(DESTEST) ${DESTEST}.c $(LIBCRYPTO)

$(BFTEST).c: ${CRYPTODIR}/bf/${BFTEST}.c
	cp ${CRYPTODIR}/bf/${BFTEST}.c ${.OBJDIR}

$(BFTEST): ${BFTEST}.c
	cc ${CFLAGS} -o $(BFTEST) ${BFTEST}.c $(LIBCRYPTO)

$(RC2TEST).c: ${CRYPTODIR}/rc2/${RC2TEST}.c
	cp ${CRYPTODIR}/rc2/${RC2TEST}.c ${.OBJDIR}

$(RC2TEST): ${RC2TEST}.c
	cc ${CFLAGS} -o $(RC2TEST) ${RC2TEST}.c $(LIBCRYPTO)

$(RC4TEST).c: ${CRYPTODIR}/rc4/${RC4TEST}.c
	cp ${CRYPTODIR}/rc4/${RC4TEST}.c ${.OBJDIR}

$(RC4TEST): ${RC4TEST}.c
	cc ${CFLAGS} -o $(RC4TEST) ${RC4TEST}.c $(LIBCRYPTO)

$(MD4TEST).c: ${CRYPTODIR}/md4/${MD4TEST}.c
	cp ${CRYPTODIR}/md4/${MD4TEST}.c ${.OBJDIR}

$(MD4TEST): ${MD4TEST}.c
	cc ${CFLAGS} -o $(MD4TEST) ${MD4TEST}.c $(LIBCRYPTO)

$(MD5TEST).c: ${CRYPTODIR}/md5/${MD5TEST}.c
	cp ${CRYPTODIR}/md5/${MD5TEST}.c ${.OBJDIR}

$(MD5TEST): ${MD5TEST}.c
	cc ${CFLAGS} -o $(MD5TEST) ${MD5TEST}.c $(LIBCRYPTO)

$(ECDHTEST).c: ${CRYPTODIR}/ecdh/${ECDHTEST}.c
	cp ${CRYPTODIR}/ecdh/${ECDHTEST}.c ${.OBJDIR}

$(ECDHTEST): ${ECDHTEST}.c
	cc ${CFLAGS} -o $(ECDHTEST) ${ECDHTEST}.c $(LIBCRYPTO)

$(ECDSATEST).c: ${CRYPTODIR}/ecdsa/${ECDSATEST}.c
	cp ${CRYPTODIR}/ecdsa/${ECDSATEST}.c ${.OBJDIR}

$(ECDSATEST): ${ECDSATEST}.c
	cc ${CFLAGS} -o $(ECDSATEST) ${ECDSATEST}.c $(LIBCRYPTO)

$(IGETEST).c: ${SSLDIR}/../test/${IGETEST}.c
	cp ${SSLDIR}/../test/${IGETEST}.c ${.OBJDIR}

$(IGETEST): ${IGETEST}.c
	cc ${CFLAGS} -o $(IGETEST) ${IGETEST}.c $(LIBCRYPTO)

$(SHA256TEST).c: ${SSLDIR}/sha/${SHA256TEST}.c
	cp ${SSLDIR}/sha/${SHA256TEST}.c ${.OBJDIR}

$(SHA256TEST): ${SHA256TEST}.c
	cc ${CFLAGS} -o $(SHA256TEST) ${SHA256TEST}.c $(LIBCRYPTO)

$(SHA512TEST).c: ${SSLDIR}/sha/${SHA512TEST}.c
	cp ${SSLDIR}/sha/${SHA512TEST}.c ${.OBJDIR}

$(SHA512TEST): ${SHA512TEST}.c
	cc ${CFLAGS} -o $(SHA512TEST) ${SHA512TEST}.c $(LIBCRYPTO)

.include <bsd.regress.mk>
