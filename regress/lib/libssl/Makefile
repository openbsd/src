#	$OpenBSD: Makefile,v 1.8 2002/09/01 16:29:31 markus Exp $

CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

REGRESSTARGETS=ossltests ssl-enc ssl-dsa ssl-rsa 

OPENSSL=/usr/sbin/openssl
CLEAR1=p
CIPHER=cipher
CLEAR2=clear

BNTEST=		bntest
ECTEST=		ectest
EXPTEST=	exptest
SHATEST=	shatest
SHA1TEST=	sha1test
MDC2TEST=	mdc2test
RMDTEST=	rmdtest
MD2TEST=	md2test
MD4TEST=	md4test
MD5TEST=	md5test
HMACTEST=	hmactest
RC2TEST=	rc2test
RC4TEST=	rc4test
BFTEST=		bftest
CASTTEST=	casttest
DESTEST=	destest
RANDTEST=	randtest
DHTEST=		dhtest
DSATEST=	dsatest
RSATEST=	rsa_test
ENGINETEST=	enginetest
EVPTEST=	evp_test


CLEANFILES+=	$(BNTEST).c $(ECTEST).c $(HMACTEST).c \
	$(SHATEST).c $(SHA1TEST).c $(MDC2TEST).c $(RMDTEST).c \
	$(RANDTEST).c $(DHTEST).c $(ENGINETEST).c \
	$(CASTTEST).c $(EXPTEST).c $(DSATEST).c $(RSATEST).c \
	$(EVPTEST).c $(DESTEST).c ${RC2TEST}.c ${RC4TEST}.c ${MD2TEST}.c \
	${MD4TEST}.c ${MD5TEST}.c

CLEANFILES+=	$(BNTEST) $(ECTEST) $(HMACTEST) \
	$(SHATEST) $(SHA1TEST) $(MDC2TEST) $(RMDTEST) \
	$(RANDTEST) $(DHTEST) $(ENGINETEST) \
	$(CASTTEST) $(EXPTEST) $(DSATEST) $(RSATEST) \
	$(EVPTEST) $(DESTEST) ${RC2TEST} ${RC4TEST} ${MD2TEST} ${MD4TEST} \
	${MD5TEST}\

CLEANFILES+= ${BNTEST}.out

OTESTS= ${ENGINETEST}  \
	${EXPTEST} ${RANDTEST} \
	${MD2TEST} ${MD4TEST} ${MD5TEST} \
	${SHATEST} ${SHA1TEST} ${HMACTEST} ${RMDTEST} ${MDC2TEST} \
	${CASTTEST} ${RC2TEST} ${RC4TEST} ${DESTEST} \
	${DHTEST} ${DSATEST} ${RSATEST} \
	${ECTEST}

${CLEAR1}: openssl.cnf
	cat ${.CURDIR}/openssl.cnf > ${CLEAR1}

CLEANFILES+=${CLEAR1}

ENCTARGETS=rc4 des-cfb des-ede-cfb des-ede3-cfb des-ofb des-ede-ofb des-ede3-ofb
ENCTARGETS+=des-ecb des-ede des-ede3 desx des-cbc des-ede-cbc des-ede3-cbc
ENCTARGETS+=rc2-ecb rc2-cfb rc2-ofb rc2-cbc bf-ecb bf-cfb bf-ofb bf-cbc rc4
ENCTARGETS+=cast5-ecb cast5-cfb cast5-ofb cast5-cbc

.for ENC in ${ENCTARGETS}
${CIPHER}.${ENC}: ${CLEAR1}
	${OPENSSL} ${ENC} -bufsize 113 -e -k test < ${CLEAR1} > ${CIPHER}.${ENC}
${CIPHER}.${ENC}.b64: ${CLEAR1}
	${OPENSSL} ${ENC} -bufsize 113 -a -e -k test < ${CLEAR1} > ${CIPHER}.${ENC}.b64

${CLEAR2}.${ENC}: ${CIPHER}.${ENC}
	${OPENSSL} ${ENC} -bufsize 157 -d -k test < ${CIPHER}.${ENC} > ${CLEAR2}.${ENC}
${CLEAR2}.${ENC}.b64: ${CIPHER}.${ENC}.b64
	${OPENSSL} ${ENC} -bufsize 157 -a -d -k test < ${CIPHER}.${ENC}.b64 > ${CLEAR2}.${ENC}.b64

ssl-enc-${ENC}: ${CLEAR1} ${CLEAR2}.${ENC}
	cmp ${CLEAR1} ${CLEAR2}.${ENC}
ssl-enc-${ENC}.b64: ${CLEAR1} ${CLEAR2}.${ENC}.b64
	cmp ${CLEAR1} ${CLEAR2}.${ENC}.b64

REGRESSTARGETS+=ssl-enc-${ENC} ssl-enc-${ENC}.b64
CLEANFILES+=${CIPHER}.${ENC} ${CIPHER}.${ENC}.b64 ${CLEAR2}.${ENC} ${CLEAR2}.${ENC}.b64 e_os.h .rnd
.endfor

ssl-enc:
	sh ${.CURDIR}/testenc.sh ${.OBJDIR} ${.CURDIR}
ssl-dsa:
	sh ${.CURDIR}/testdsa.sh ${.OBJDIR} ${.CURDIR}
ssl-rsa:
	sh ${.CURDIR}/testrsa.sh ${.OBJDIR} ${.CURDIR}

e_os.h: ${.CURDIR}/../../../lib/libssl/src/e_os.h
	cp ${.CURDIR}/../../../lib/libssl/src/e_os.h ${.OBJDIR}

ossltests: ${OTESTS} ${BNTEST} ${EVPTEST}
	@echo running ${BNTEST}, check ${.OBJDIR}/${BNTEST}.out if this fails.
	${.OBJDIR}/${BNTEST} > ${.OBJDIR}/${BNTEST}.out 2>&1
.for OT in ${OTESTS}
	@echo running ${OT}
	${.OBJDIR}/${OT} 
.endfor
	@echo running ${EVPTEST}
	${.OBJDIR}/${EVPTEST} ${.CURDIR}/../../../lib/libssl/src/crypto/evp/evptests.txt

$(BNTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/bn/bntest.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/bn/bntest.c ${.OBJDIR}

$(BNTEST): ${BNTEST}.c e_os.h
	cc -O -o $(BNTEST) ${BNTEST}.c -lcrypto

$(EXPTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/bn/exptest.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/bn/exptest.c ${.OBJDIR}

$(EXPTEST): ${EXPTEST}.c e_os.h
	cc -O -o $(EXPTEST) ${EXPTEST}.c -lcrypto

$(ECTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/ec/ectest.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/ec/ectest.c ${.OBJDIR}

$(ECTEST): ${ECTEST}.c e_os.h
	cc -O -o $(ECTEST) ${ECTEST}.c -lcrypto

$(EVPTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/evp/${EVPTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/evp/${EVPTEST}.c ${.OBJDIR}
$(EVPTEST): ${EVPTEST}.c e_os.h
	cc -O -o $(EVPTEST) ${EVPTEST}.c -lcrypto

$(SHATEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/sha/${SHATEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/sha/${SHATEST}.c ${.OBJDIR}
$(SHATEST): ${SHATEST}.c e_os.h
	cc -O -o $(SHATEST) ${SHATEST}.c -lcrypto

$(SHA1TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/sha/${SHA1TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/sha/${SHA1TEST}.c ${.OBJDIR}
$(SHA1TEST): ${SHA1TEST}.c e_os.h
	cc -O -o $(SHA1TEST) ${SHA1TEST}.c -lcrypto

$(RANDTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/rand/${RANDTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/rand/${RANDTEST}.c ${.OBJDIR}
$(RANDTEST): ${RANDTEST}.c e_os.h
	cc -O -o $(RANDTEST) ${RANDTEST}.c -lcrypto

$(MDC2TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/mdc2/${MDC2TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/mdc2/${MDC2TEST}.c ${.OBJDIR}
$(MDC2TEST): ${MDC2TEST}.c e_os.h
	cc -O -o $(MDC2TEST) ${MDC2TEST}.c -lcrypto

$(RMDTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/ripemd/${RMDTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/ripemd/${RMDTEST}.c ${.OBJDIR}
$(RMDTEST): ${RMDTEST}.c e_os.h
	cc -O -o $(RMDTEST) ${RMDTEST}.c -lcrypto

$(DHTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/dh/${DHTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/dh/${DHTEST}.c ${.OBJDIR}

$(DHTEST): ${DHTEST}.c e_os.h
	cc -O -o $(DHTEST) ${DHTEST}.c -lcrypto

$(ENGINETEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/engine/${ENGINETEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/engine/${ENGINETEST}.c ${.OBJDIR}

$(ENGINETEST): ${ENGINETEST}.c e_os.h
	cc -O -o $(ENGINETEST) ${ENGINETEST}.c -lcrypto

$(CASTTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/cast/${CASTTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/cast/${CASTTEST}.c ${.OBJDIR}

$(CASTTEST): ${CASTTEST}.c e_os.h
	cc -O -o $(CASTTEST) ${CASTTEST}.c -lcrypto

$(RSATEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/rsa/${RSATEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/rsa/${RSATEST}.c ${.OBJDIR}

$(RSATEST): ${RSATEST}.c e_os.h
	cc -O -o $(RSATEST) ${RSATEST}.c -lcrypto

$(DSATEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/dsa/${DSATEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/dsa/${DSATEST}.c ${.OBJDIR}

$(DSATEST): ${DSATEST}.c e_os.h
	cc -O -o $(DSATEST) ${DSATEST}.c -lcrypto


$(HMACTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/hmac/${HMACTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/hmac/${HMACTEST}.c ${.OBJDIR}

$(HMACTEST): ${HMACTEST}.c e_os.h
	cc -O -o $(HMACTEST) ${HMACTEST}.c -lcrypto

$(DESTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/des/${DESTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/des/${DESTEST}.c ${.OBJDIR}

$(DESTEST): ${DESTEST}.c e_os.h
	cc -O -o $(DESTEST) ${DESTEST}.c -lcrypto

$(BFTEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/bf/${BFTEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/bf/${BFTEST}.c ${.OBJDIR}

$(BFTEST): ${BFTEST}.c e_os.h
	cc -O -o $(BFTEST) ${BFTEST}.c -lcrypto

$(RC2TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/rc2/${RC2TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/rc2/${RC2TEST}.c ${.OBJDIR}

$(RC2TEST): ${RC2TEST}.c e_os.h
	cc -O -o $(RC2TEST) ${RC2TEST}.c -lcrypto

$(RC4TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/rc4/${RC4TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/rc4/${RC4TEST}.c ${.OBJDIR}

$(RC4TEST): ${RC4TEST}.c e_os.h
	cc -O -o $(RC4TEST) ${RC4TEST}.c -lcrypto

$(MD2TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/md2/${MD2TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/md2/${MD2TEST}.c ${.OBJDIR}

$(MD2TEST): ${MD2TEST}.c e_os.h
	cc -O -o $(MD2TEST) ${MD2TEST}.c -lcrypto

$(MD4TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/md4/${MD4TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/md4/${MD4TEST}.c ${.OBJDIR}

$(MD4TEST): ${MD4TEST}.c e_os.h
	cc -O -o $(MD4TEST) ${MD4TEST}.c -lcrypto

$(MD5TEST).c: ${.CURDIR}/../../../lib/libssl/src/crypto/md5/${MD5TEST}.c
	cp ${.CURDIR}/../../../lib/libssl/src/crypto/md5/${MD5TEST}.c ${.OBJDIR}

$(MD5TEST): ${MD5TEST}.c e_os.h
	cc -O -o $(MD5TEST) ${MD5TEST}.c -lcrypto

.include <bsd.regress.mk>
