#	$OpenBSD: Makefile,v 1.12 2003/06/16 03:15:14 mickey Exp $

CLEANFILES+= testdsa.key testdsa.pem rsakey.pem rsacert.pem dsa512.pem

REGRESS_TARGETS=ossltests ssl-enc ssl-dsa ssl-rsa 

OPENSSL=/usr/sbin/openssl
CLEAR1=p
CIPHER=cipher
CLEAR2=clear

BNTEST=		bntest
ECTEST=		ectest
EXPTEST=	exptest
SHATEST=	shatest
SHA1TEST=	sha1test
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

SSLDIR=	${.CURDIR}/../../../lib/libssl/src/crypto

CFLAGS= -O -I${SSLDIR} -I${SSLDIR}/..

CLEANFILES+=	$(BNTEST).c $(ECTEST).c $(HMACTEST).c \
	$(SHATEST).c $(SHA1TEST).c $(MDC2TEST).c $(RMDTEST).c \
	$(RANDTEST).c $(DHTEST).c $(ENGINETEST).c \
	$(CASTTEST).c $(EXPTEST).c $(DSATEST).c $(RSATEST).c \
	$(EVPTEST).c $(DESTEST).c ${RC2TEST}.c ${RC4TEST}.c ${MD2TEST}.c \
	${MD4TEST}.c ${MD5TEST}.c ${BFTEST}.c

CLEANFILES+=	$(BNTEST) $(ECTEST) $(HMACTEST) \
	$(SHATEST) $(SHA1TEST) $(MDC2TEST) $(RMDTEST) \
	$(RANDTEST) $(DHTEST) $(ENGINETEST) \
	$(CASTTEST) $(EXPTEST) $(DSATEST) $(RSATEST) \
	$(EVPTEST) $(DESTEST) ${RC2TEST} ${RC4TEST} ${MD2TEST} ${MD4TEST} \
	${MD5TEST} ${BFTEST}

CLEANFILES+= ${BNTEST}.out

OTESTS= ${ENGINETEST}  \
	${EXPTEST} ${RANDTEST} \
	${MD2TEST} ${MD4TEST} ${MD5TEST} \
	${SHATEST} ${SHA1TEST} ${HMACTEST} ${RMDTEST} ${MDC2TEST} \
	${CASTTEST} ${BFTEST} ${RC2TEST} ${RC4TEST} ${DESTEST} \
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
	${.OBJDIR}/${EVPTEST} ${SSLDIR}/evp/evptests.txt

$(BNTEST).c: ${SSLDIR}/bn/bntest.c
	cp ${SSLDIR}/bn/bntest.c ${.OBJDIR}

$(BNTEST): ${BNTEST}.c
	cc ${CFLAGS} -o $(BNTEST) ${BNTEST}.c -lcrypto

$(EXPTEST).c: ${SSLDIR}/bn/exptest.c
	cp ${SSLDIR}/bn/exptest.c ${.OBJDIR}

$(EXPTEST): ${EXPTEST}.c
	cc ${CFLAGS} -o $(EXPTEST) ${EXPTEST}.c -lcrypto

$(ECTEST).c: ${SSLDIR}/ec/ectest.c
	cp ${SSLDIR}/ec/ectest.c ${.OBJDIR}

$(ECTEST): ${ECTEST}.c
	cc ${CFLAGS} -o $(ECTEST) ${ECTEST}.c -lcrypto

$(EVPTEST).c: ${SSLDIR}/evp/${EVPTEST}.c
	cp ${SSLDIR}/evp/${EVPTEST}.c ${.OBJDIR}
$(EVPTEST): ${EVPTEST}.c
	cc ${CFLAGS} -o $(EVPTEST) ${EVPTEST}.c -lcrypto

$(SHATEST).c: ${SSLDIR}/sha/${SHATEST}.c
	cp ${SSLDIR}/sha/${SHATEST}.c ${.OBJDIR}
$(SHATEST): ${SHATEST}.c
	cc ${CFLAGS} -o $(SHATEST) ${SHATEST}.c -lcrypto

$(SHA1TEST).c: ${SSLDIR}/sha/${SHA1TEST}.c
	cp ${SSLDIR}/sha/${SHA1TEST}.c ${.OBJDIR}
$(SHA1TEST): ${SHA1TEST}.c
	cc ${CFLAGS} -o $(SHA1TEST) ${SHA1TEST}.c -lcrypto

$(RANDTEST).c: ${SSLDIR}/rand/${RANDTEST}.c
	cp ${SSLDIR}/rand/${RANDTEST}.c ${.OBJDIR}
$(RANDTEST): ${RANDTEST}.c
	cc ${CFLAGS} -o $(RANDTEST) ${RANDTEST}.c -lcrypto

$(RMDTEST).c: ${SSLDIR}/ripemd/${RMDTEST}.c
	cp ${SSLDIR}/ripemd/${RMDTEST}.c ${.OBJDIR}
$(RMDTEST): ${RMDTEST}.c
	cc ${CFLAGS} -o $(RMDTEST) ${RMDTEST}.c -lcrypto

$(DHTEST).c: ${SSLDIR}/dh/${DHTEST}.c
	cp ${SSLDIR}/dh/${DHTEST}.c ${.OBJDIR}

$(DHTEST): ${DHTEST}.c
	cc ${CFLAGS} -o $(DHTEST) ${DHTEST}.c -lcrypto

$(ENGINETEST).c: ${SSLDIR}/engine/${ENGINETEST}.c
	cp ${SSLDIR}/engine/${ENGINETEST}.c ${.OBJDIR}

$(ENGINETEST): ${ENGINETEST}.c
	cc ${CFLAGS} -o $(ENGINETEST) ${ENGINETEST}.c -lcrypto

$(CASTTEST).c: ${SSLDIR}/cast/${CASTTEST}.c
	cp ${SSLDIR}/cast/${CASTTEST}.c ${.OBJDIR}

$(CASTTEST): ${CASTTEST}.c
	cc ${CFLAGS} -o $(CASTTEST) ${CASTTEST}.c -lcrypto

$(RSATEST).c: ${SSLDIR}/rsa/${RSATEST}.c
	cp ${SSLDIR}/rsa/${RSATEST}.c ${.OBJDIR}

$(RSATEST): ${RSATEST}.c
	cc ${CFLAGS} -o $(RSATEST) ${RSATEST}.c -lcrypto

$(DSATEST).c: ${SSLDIR}/dsa/${DSATEST}.c
	cp ${SSLDIR}/dsa/${DSATEST}.c ${.OBJDIR}

$(DSATEST): ${DSATEST}.c
	cc ${CFLAGS} -o $(DSATEST) ${DSATEST}.c -lcrypto


$(HMACTEST).c: ${SSLDIR}/hmac/${HMACTEST}.c
	cp ${SSLDIR}/hmac/${HMACTEST}.c ${.OBJDIR}

$(HMACTEST): ${HMACTEST}.c
	cc ${CFLAGS} -o $(HMACTEST) ${HMACTEST}.c -lcrypto

$(DESTEST).c: ${SSLDIR}/des/${DESTEST}.c
	cp ${SSLDIR}/des/${DESTEST}.c ${.OBJDIR}

$(DESTEST): ${DESTEST}.c
	cc ${CFLAGS} -o $(DESTEST) ${DESTEST}.c -lcrypto

$(BFTEST).c: ${SSLDIR}/bf/${BFTEST}.c
	cp ${SSLDIR}/bf/${BFTEST}.c ${.OBJDIR}

$(BFTEST): ${BFTEST}.c
	cc ${CFLAGS} -o $(BFTEST) ${BFTEST}.c -lcrypto

$(RC2TEST).c: ${SSLDIR}/rc2/${RC2TEST}.c
	cp ${SSLDIR}/rc2/${RC2TEST}.c ${.OBJDIR}

$(RC2TEST): ${RC2TEST}.c
	cc ${CFLAGS} -o $(RC2TEST) ${RC2TEST}.c -lcrypto

$(RC4TEST).c: ${SSLDIR}/rc4/${RC4TEST}.c
	cp ${SSLDIR}/rc4/${RC4TEST}.c ${.OBJDIR}

$(RC4TEST): ${RC4TEST}.c
	cc ${CFLAGS} -o $(RC4TEST) ${RC4TEST}.c -lcrypto

$(MD2TEST).c: ${SSLDIR}/md2/${MD2TEST}.c
	cp ${SSLDIR}/md2/${MD2TEST}.c ${.OBJDIR}

$(MD2TEST): ${MD2TEST}.c
	cc ${CFLAGS} -o $(MD2TEST) ${MD2TEST}.c -lcrypto

$(MD4TEST).c: ${SSLDIR}/md4/${MD4TEST}.c
	cp ${SSLDIR}/md4/${MD4TEST}.c ${.OBJDIR}

$(MD4TEST): ${MD4TEST}.c
	cc ${CFLAGS} -o $(MD4TEST) ${MD4TEST}.c -lcrypto

$(MD5TEST).c: ${SSLDIR}/md5/${MD5TEST}.c
	cp ${SSLDIR}/md5/${MD5TEST}.c ${.OBJDIR}

$(MD5TEST): ${MD5TEST}.c
	cc ${CFLAGS} -o $(MD5TEST) ${MD5TEST}.c -lcrypto

.include <bsd.regress.mk>
