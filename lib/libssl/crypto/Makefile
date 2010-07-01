# $OpenBSD: Makefile,v 1.53 2010/07/01 17:44:57 thib Exp $

LIB=	crypto
WANTLINT=

SSLEAYDIST=	src
SSL_SRC=	${.CURDIR}/../${SSLEAYDIST}
LCRYPTO_SRC=	${SSL_SRC}/crypto
FIPS_SRC=	${SSL_SRC}/fips

# arm and sh default to little endian, mips defaults to big endian
.if ${MACHINE_ARCH} == "alpha" || ${MACHINE_ARCH} == "amd64" || \
    ${MACHINE_ARCH} == "arm" || ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "sh" || \
    ${MACHINE_ARCH} == "vax"
CFLAGS+= -DL_ENDIAN
.else
CFLAGS+= -DB_ENDIAN
.endif 

.include <bsd.own.mk>		# for 'NOPIC' definition
.if !defined(NOPIC)
CFLAGS+= -DDSO_DLFCN -DHAVE_DLFCN_H
.endif

.if ${MACHINE_ARCH} == "sparc"
PICFLAG=-fPIC
.endif

CFLAGS+= -DTERMIOS -DANSI_SOURCE -DNO_ERR -DNO_WINDOWS_BRAINDEATH
# Patented algorithms
CFLAGS+= -DOPENSSL_NO_IDEA
CFLAGS+= -DOPENSSL_NO_RC5
CFLAGS+= -DOPENSSL_NO_KRB5
CFLAGS+= -DOPENSSL_NO_MDC2

# Hardware engines
CFLAGS+= -DOPENSSL_NO_HW_4758_CCA
CFLAGS+= -DOPENSSL_NO_HW_AEP
CFLAGS+= -DOPENSSL_NO_HW_ATALLA
CFLAGS+= -DOPENSSL_NO_CAPIENG
CFLAGS+= -DOPENSSL_NO_HW_CSWIFT
CFLAGS+= -DOPENSSL_NO_HW_NCIPHER
CFLAGS+= -DOPENSSL_NO_HW_NURON
CFLAGS+= -DOPENSSL_NO_HW_PADLOCK # XXX enable this?
CFLAGS+= -DOPENSSL_NO_HW_SUREWARE
CFLAGS+= -DOPENSSL_NO_HW_UBSEC

CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST}
CFLAGS+= -I${LCRYPTO_SRC}
SRCS+= o_time.c o_dir.c o_init.c fips_err.c
SRCS+=	cryptlib.c dyn_lck.c ex_data.c cpt_err.c mem.c mem_dbg.c mem_clr.c
SRCS+=	tmdiff.c cversion.c uid.c
SRCS+=	md2_dgst.c md2_one.c 					
SRCS+=	md5_dgst.c md5_one.c 					
SRCS+=	sha_dgst.c sha1dgst.c sha_one.c sha1_one.c sha256.c sha512.c
#SRCS+=	mdc2dgst.c mdc2_one.c					
SRCS+=	hmac.c							
SRCS+=	rmd_dgst.c rmd_one.c				
SRCS+=	acss_skey.c acss_enc.c
SRCS+=	aes_cfb.c aes_ctr.c aes_ecb.c aes_ofb.c aes_misc.c
SRCS+=	aes_ige.c aes_wrap.c
SRCS+=	cbc_cksm.c cbc_enc.c cfb64enc.c cfb_enc.c \
	ecb3_enc.c ecb_enc.c enc_read.c enc_writ.c \
	ofb64enc.c ofb_enc.c pcbc_enc.c \
	qud_cksm.c rand_key.c rpc_enc.c set_key.c \
	des_lib.c des_enc.c des_old2.c fcrypt_b.c \
	fcrypt.c xcbc_enc.c ede_cbcm_enc.c \
	str2key.c cfb64ede.c ofb64ede.c \
	des_old.c read2pwd.c
SRCS+=	rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c
SRCS+=	rc2ofb64.c							
#SRCS+=	rc5_skey.c rc5_ecb.c rc5cfb64.c rc5cfb64.c 	
#SRCS+=	rc5ofb64.c rc5_enc.c
#SRCS+=	i_cbc.c i_cfb64.c i_ofb64.c i_ecb.c		
#SRCS+=	i_skey.c							
SRCS+=	bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c
SRCS+=	c_skey.c c_ecb.c c_cfb64.c c_ofb64.c c_enc.c	
SRCS+=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_mul.c
SRCS+=	bn_print.c bn_rand.c bn_shift.c
SRCS+=	bn_word.c bn_blind.c bn_gcd.c bn_prime.c bn_err.c
SRCS+=	bn_sqr.c bn_recp.c bn_mont.c bn_mpi.c bn_mod.c
SRCS+=	bn_exp2.c bn_ctx.c bn_opt.c
SRCS+=	bn_sqrt.c bn_kron.c bn_x931p.c bn_const.c bn_depr.c bn_gf2m.c bn_nist.c
#SRCS+=	camellia.c cmll_cbc.c cmll_cfb.c cmll_ctr.c
#SRCS+=	cmll_ecb.c cmll_misc.c cmll_ofb.cq
#SRCS+=	cms_asn1.c cms_att.c cms_cd.c cms_dd.c cms_enc.c cms_env.c
#SRCS+=	cms_err.c cms_ess.c cms_io.c cms_lib.c cms_sd.c cms_smime.c
SRCS+=	rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c
SRCS+=	rsa_saos.c rsa_err.c rsa_pk1.c rsa_ssl.c
SRCS+=	rsa_none.c rsa_chk.c rsa_oaep.c rsa_null.c rsa_asn1.c
SRCS+=	rsa_pss.c rsa_x931.c rsa_x931g.c rsa_depr.c rsa_eng.c
SRCS+=	dsa_gen.c dsa_key.c dsa_lib.c dsa_vrf.c 	
SRCS+=	dsa_sign.c dsa_err.c dsa_asn1.c dsa_ossl.c dsa_depr.c dsa_utl.c
SRCS+=	dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c dh_asn1.c dh_depr.c
SRCS+=	ec_cvt.c ec_lib.c ecp_mont.c ecp_recp.c
SRCS+=	ec_err.c ec_mult.c ecp_nist.c ecp_smpl.c
SRCS+=	ec2_mult.c ec2_smpl.c ec_asn1.c ec_check.c ec_curve.c
SRCS+=	ec_key.c ec_print.c
SRCS+=	ech_err.c ech_key.c ech_lib.c ech_ossl.c
SRCS+=	ecs_asn1.c ecs_err.c ecs_lib.c ecs_ossl.c ecs_sign.c ecs_vrf.c
SRCS+=	buffer.c buf_str.c buf_err.c				
SRCS+=	bio_lib.c bio_cb.c bio_err.c bss_mem.c
SRCS+=	bss_null.c bss_fd.c bss_file.c bss_sock.c	
SRCS+=	bss_conn.c bf_null.c bf_buff.c 
SRCS+=	b_print.c b_dump.c b_sock.c bss_acpt.c 		
SRCS+=	bf_nbio.c bss_bio.c bss_log.c bss_dgram.c
SRCS+=	stack.c							
SRCS+=	lhash.c lh_stats.c					
SRCS+=	md_rand.c randfile.c rand_lib.c	rand_egd.c rand_eng.c
SRCS+=   rand_err.c rand_unix.c
SRCS+=	err.c err_def.c err_all.c err_prn.c err_str.c err_bio.c
SRCS+=	obj_dat.c obj_lib.c obj_err.c o_names.c
SRCS+= bio_b64.c e_bf.c m_sha.c p_open.c
SRCS+= bio_enc.c e_cast.c e_xcbc_d.c m_dss.c m_sha1.c p_seal.c
SRCS+= bio_md.c e_des.c encode.c m_dss1.c names.c p_sign.c
SRCS+= bio_ok.c e_des3.c dig_eng.c evp_enc.c m_md2.c p_verify.c m_ecdsa.c
#SRCS+= e_camellia.c e_seed.c
SRCS+= c_all.c evp_err.c evp_acnf.c evp_cnf.c m_md4.c p5_crpt.c e_old.c
SRCS+= c_allc.c evp_key.c m_md5.c p5_crpt2.c
SRCS+= c_alld.c e_null.c evp_lib.c p_dec.c
SRCS+= digest.c e_rc2.c enc_min.c evp_pbe.c m_null.c p_enc.c
SRCS+= e_acss.c e_aes.c e_rc4.c evp_pkey.c m_ripemd.c p_lib.c
SRCS+= md4_dgst.c md4_one.c
SRCS+=	pem_sign.c pem_seal.c pem_info.c pem_lib.c pem_pkey.c	
SRCS+=	pem_all.c pem_err.c pem_x509.c pem_pk8.c pem_oth.c pem_xaux.c
SRCS+=	ui_err.c ui_lib.c ui_openssl.c ui_compat.c ui_util.c
SRCS+= a_bitstr.c a_mbstr.c a_utctm.c f_enum.c t_bitst.c x_name.c
SRCS+= a_bool.c a_meth.c a_utf8.c f_int.c t_crl.c tasn_typ.c x_pkey.c
SRCS+= a_bytes.c a_object.c a_verify.c f_string.c t_pkey.c tasn_utl.c x_pubkey.c
SRCS+= a_d2i_fp.c a_octet.c asn1_err.c i2d_pr.c t_req.c x_algor.c x_req.c
SRCS+= a_digest.c a_print.c asn1_lib.c i2d_pu.c t_spki.c x_attrib.c x_sig.c
SRCS+= a_dup.c a_set.c asn1_par.c n_pkey.c t_x509.c x_bignum.c x_spki.c
SRCS+= a_enum.c a_sign.c asn_pack.c nsseq.c t_x509a.c x_val.c
SRCS+= a_gentm.c a_strex.c d2i_pr.c p5_pbe.c tasn_dec.c x_crl.c x_x509.c
SRCS+= a_hdr.c a_strnid.c d2i_pu.c p5_pbev2.c tasn_enc.c x_exten.c x_x509a.c
SRCS+= a_i2d_fp.c a_time.c evp_asn1.c tasn_fre.c x_info.c
SRCS+= a_int.c a_type.c p8_pkey.c tasn_new.c x_long.c asn_moid.c
SRCS+= asn1_gen.c asn_mime.c
SRCS+= x509_d2.c x509_lu.c x509_set.c x509_vfy.c x509spki.c by_dir.c
SRCS+= x509_def.c x509_obj.c x509_trs.c x509cset.c x509type.c by_file.c
SRCS+= x509_att.c x509_err.c x509_r2x.c x509_txt.c x509name.c x_all.c
SRCS+= x509_cmp.c x509_ext.c x509_req.c x509_v3.c x509rset.c
SRCS+= x509_vpm.c
SRCS+=	v3_akey.c v3_alt.c v3_bcons.c v3_bitst.c v3_conf.c v3_cpols.c
SRCS+=	v3_crld.c v3_enum.c v3_extku.c v3_genn.c v3_ia5.c v3_int.c
SRCS+=	v3_lib.c v3_pku.c v3_prn.c v3_skey.c v3_sxnet.c v3_utl.c
SRCS+=	v3err.c v3_info.c v3_purp.c v3_ocsp.c v3_akeya.c v3_pci.c v3_pcia.c
SRCS+=	pcy_cache.c pcy_data.c pcy_int.h pcy_lib.c pcy_map.c pcy_node.c
SRCS+=	pcy_tree.c v3_addr.c v3_asid.c v3_ncons.c v3_pcons.c v3_pmaps.c
SRCS+=	conf_err.c conf_lib.c conf_def.c conf_api.c conf_mod.c conf_mall.c
SRCS+=	conf_sap.c
SRCS+=	txt_db.c							
SRCS+= pk7_lib.c pkcs7err.c
SRCS+= pk7_asn1.c pk7_doit.c pk7_mime.c 
SRCS+= pk7_attr.c pk7_smime.c
SRCS+=	c_rle.c c_zlib.c comp_lib.c comp_err.c
SRCS+=	p12_add.c p12_crpt.c p12_init.c p12_mutl.c p12_p8e.c
SRCS+=	p12_asn.c p12_crt.c p12_key.c p12_npas.c p12_utl.c
SRCS+=	p12_attr.c p12_decr.c p12_kiss.c p12_p8d.c pk12err.c
SRCS+= eng_all.c eng_openssl.c eng_ctrl.c eng_pkey.c eng_aesni.c
SRCS+= eng_dyn.c eng_table.c tb_cipher.c tb_store.c eng_err.c tb_rsa.c
SRCS+= tb_ecdh.c tb_ecdsa.c
SRCS+= hw_cryptodev.c eng_cnf.c
SRCS+= tb_dh.c eng_fat.c tb_digest.c eng_init.c
SRCS+= tb_dsa.c eng_lib.c tb_rand.c eng_list.c
SRCS+= dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c
SRCS+= dso_openssl.c dso_win32.c dso_vms.c
SRCS+= ocsp_asn.c ocsp_err.c ocsp_ht.c ocsp_prn.c ocsp_vfy.c
SRCS+= ocsp_cl.c ocsp_ext.c ocsp_lib.c ocsp_srv.c
SRCS+=	pqueue.c
#SRCS+=	seed.c seed_cbc.c seed_cfb.c seed_ecb.c seed_ofb.c
SRCS+=	str_err.c str_lib.c str_mem.c str_meth.c
# Only used when -DOPENSSL_FIPS is set
#SRCS+= fips.c fips_err_wrapper.c
#SRCS+= fips_aes_core.c fips_aes_selftest.c
#SRCS+= fips_des_enc.c fips_des_selftest.c fips_set_key.c
#SRCS+= fips_dh_check.c fips_dh_gen.c fips_dh_key.c
#SRCS+= fips_dsa_ossl.c fips_dsa_gen.c fips_dsa_selftest.c
#SRCS+= fips_rand.c
#SRCS+= fips_rsa_eay.c fips_rsa_gen.c fips_rsa_selftest.c
#SRCS+= fips_sha1dgst.c fips_sha1_selftest.c

.PATH:	${LCRYPTO_SRC}/md2 ${LCRYPTO_SRC}/md5 ${LCRYPTO_SRC}/sha \
	${LCRYPTO_SRC}/mdc2 ${LCRYPTO_SRC}/hmac ${LCRYPTO_SRC}/ripemd \
	${LCRYPTO_SRC}/des ${LCRYPTO_SRC}/rc2 ${LCRYPTO_SRC}/rc4 \
	${LCRYPTO_SRC}/rc5 ${LCRYPTO_SRC}/idea ${LCRYPTO_SRC}/bf \
	${LCRYPTO_SRC}/cast ${LCRYPTO_SRC}/bn ${LCRYPTO_SRC}/rsa \
	${LCRYPTO_SRC}/dsa ${LCRYPTO_SRC}/dh ${LCRYPTO_SRC}/buffer \
	${LCRYPTO_SRC}/bio ${LCRYPTO_SRC}/stack ${LCRYPTO_SRC}/lhash \
	${LCRYPTO_SRC}/rand ${LCRYPTO_SRC}/err ${LCRYPTO_SRC}/objects \
	${LCRYPTO_SRC}/evp ${LCRYPTO_SRC}/pem ${LCRYPTO_SRC}/asn1 \
	${LCRYPTO_SRC}/asn1 ${LCRYPTO_SRC}/x509 ${LCRYPTO_SRC}/conf \
	${LCRYPTO_SRC}/pkcs7 ${LCRYPTO_SRC}/x509v3 ${LCRYPTO_SRC}/pkcs12 \
	${LCRYPTO_SRC}/comp ${LCRYPTO_SRC}/txt_db ${LCRYPTO_SRC}/md4 \
	${LCRYPTO_SRC}/engine ${LCRYPTO_SRC}/dso ${LCRYPTO_SRC}/ui \
	${LCRYPTO_SRC}/ocsp ${LCRYPTO_SRC}/ec ${LCRYPTO_SRC}/aes \
	${LCRYPTO_SRC}/camellia ${LCRYPTO_SRC}/seed ${LCRYPTO_SRC}/cms \
	${LCRYPTO_SRC}/ec ${LCRYPTO_SRC}/ecdh ${LCRYPTO_SRC}/ecdsa \
	${LCRYPTO_SRC}/pqueue ${LCRYPTO_SRC}/store \
	${LCRYPTO_SRC} \
	${LCRYPTO_SRC}/acss ${.CURDIR}/arch/${MACHINE_CPU} \
	${LCRYPTO_SRC}/bn/asm \
	${FIPS_SRC}/ ${FIPS_SRC}/aes ${FIPS_SRC}/des ${FIPS_SRC}/dh \
	${FIPS_SRC}/dsa ${FIPS_SRC}/rand ${FIPS_SRC}/rsa ${FIPS_SRC}/sha1

HDRS=\
	crypto/acss/acss.h \
	crypto/aes/aes.h \
	crypto/asn1/asn1.h \
	crypto/asn1/asn1_mac.h \
	crypto/asn1/asn1t.h \
	crypto/bf/blowfish.h \
	crypto/bio/bio.h \
	crypto/bn/bn.h \
	crypto/buffer/buffer.h \
	crypto/camellia/camellia.h \
	crypto/cast/cast.h \
	crypto/cms/cms.h \
	crypto/comp/comp.h \
	crypto/conf/conf.h \
	crypto/conf/conf_api.h \
	crypto/crypto.h \
	crypto/des/des.h \
	crypto/des/des_old.h \
	crypto/dh/dh.h \
	crypto/dsa/dsa.h \
	crypto/dso/dso.h \
	crypto/ebcdic.h \
	crypto/ec/ec.h \
	crypto/ecdh/ecdh.h \
	crypto/ecdsa/ecdsa.h \
	crypto/engine/engine.h \
	crypto/err/err.h \
	crypto/evp/evp.h \
	crypto/hmac/hmac.h \
	crypto/idea/idea.h \
	crypto/lhash/lhash.h \
	crypto/md2/md2.h \
	crypto/md4/md4.h \
	crypto/md5/md5.h \
	crypto/mdc2/mdc2.h \
	crypto/objects/objects.h \
	crypto/ocsp/ocsp.h \
	crypto/opensslv.h \
	crypto/ossl_typ.h \
	crypto/pem/pem.h \
	crypto/pem/pem2.h \
	crypto/pkcs12/pkcs12.h \
	crypto/pkcs7/pkcs7.h \
	crypto/pqueue/pq_compat.h \
	crypto/pqueue/pqueue.h \
	crypto/rand/rand.h \
	crypto/rc2/rc2.h \
	crypto/rc4/rc4.h \
	crypto/rc5/rc5.h \
	crypto/ripemd/ripemd.h \
	crypto/rsa/rsa.h \
	crypto/seed/seed.h \
	crypto/sha/sha.h \
	crypto/stack/safestack.h \
	crypto/stack/stack.h \
	crypto/store/store.h \
	crypto/symhacks.h \
	crypto/tmdiff.h \
	crypto/txt_db/txt_db.h \
	crypto/ui/ui.h \
	crypto/ui/ui_compat.h \
	crypto/x509/x509.h \
	crypto/x509/x509_vfy.h \
	crypto/x509v3/x509v3.h \
	e_os2.h \
	fips/fips.h \
	fips/rand/fips_rand.h 

HDRS_GEN=\
	${.CURDIR}/arch/${MACHINE_CPU}/opensslconf.h \
	${.OBJDIR}/obj_mac.h

includes: obj_mac.h
	@test -d ${DESTDIR}/usr/include/openssl || \
	    mkdir ${DESTDIR}/usr/include/openssl
	@cd ${SSL_SRC}; \
	for i in $(HDRS); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/openssl/`basename $$i` || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/openssl"; \
	    echo $$j; \
	    eval "$$j"; \
	done; \
	for i in $(HDRS_GEN); do \
	    j="cmp -s $$i ${DESTDIR}/usr/include/openssl/`basename $$i` || \
	    ${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 $$i\
		${DESTDIR}/usr/include/openssl"; \
	    echo $$j; \
	    eval "$$j"; \
	done;

# generated
CFLAGS+= -I${.OBJDIR}

GENERATED=obj_mac.h obj_dat.h
CLEANFILES=${GENERATED} obj_mac.num.tmp
SSL_OBJECTS=${SSL_SRC}/crypto/objects

obj_mac.h: ${SSL_OBJECTS}/objects.h ${SSL_OBJECTS}/obj_mac.num ${SSL_OBJECTS}/objects.txt
	cat ${SSL_OBJECTS}/obj_mac.num > obj_mac.num.tmp
	/usr/bin/perl ${SSL_OBJECTS}/objects.pl ${SSL_OBJECTS}/objects.txt obj_mac.num.tmp obj_mac.h

obj_dat.h: obj_mac.h
	/usr/bin/perl ${SSL_OBJECTS}/obj_dat.pl obj_mac.h obj_dat.h

.if (${MACHINE_ARCH} == "vax")
# egcs bombs optimising these files
a_strnid.o:
	${CC} ${CFLAGS} -O0 ${CPPFLAGS} -c ${.IMPSRC}
a_strnid.po:           
	${CC} ${CFLAGS} -O0 ${CPPFLAGS} -c ${.IMPSRC} -o $@
des_enc.o:             
	${CC} ${CFLAGS} -O1 ${CPPFLAGS} -c ${.IMPSRC}
des_enc.po:            
	${CC} ${CFLAGS} -O1 ${CPPFLAGS} -c ${.IMPSRC} -o $@
.endif  

.if (${MACHINE_ARCH} == "i386")
SRCS+=	rc4_skey.c
CFLAGS+= -DAES_ASM
CFLAGS+= -DMD5_ASM
CFLAGS+= -DSHA1_ASM
CFLAGS+= -DRMD160_ASM
CFLAGS+= -DOPENBSD_DES_ASM
CFLAGS+= -DOPENSSL_BN_ASM_PART_WORDS
CFLAGS+= -DOPENSSL_BN_ASM_MONT
CFLAGS+= -DOPENSSL_CPUID_OBJ
# XXX bad relocation in CAST ASM code leads to TEXTREL for shared libcrypto
#CFLAGS+= -DOPENBSD_CAST_ASM
SSLASM=\
	aes aes-586 \
	bf bf-586 \
	bn bn-586 \
	bn co-586 \
	bn mo-586 \
	des crypt586 \
	des des-586 \
	md5 md5-586 \
	rc4 rc4-586 \
	ripemd rmd-586 \
	sha sha1-586
.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl ${LCRYPTO_SRC}/perlasm/x86unix.pl
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm -I${LCRYPTO_SRC}/${dir}/asm \
		${LCRYPTO_SRC}/${dir}/asm/${f}.pl openbsd-elf 386 > ${.TARGET}
.endfor
SRCS+=	x86cpuid.S
GENERATED+=x86cpuid.S
x86cpuid.S: ${LCRYPTO_SRC}/x86cpuid.pl
	/usr/bin/perl -I${LCRYPTO_SRC}/perlasm \
		${LCRYPTO_SRC}/x86cpuid.pl openbsd-elf 386 > ${.TARGET}
SRCS+= bf_cbc.c	
.elif (${MACHINE_ARCH} == "amd64")
SRCS+= aes_core.c aes_cbc.c
SRCS+= bf_enc.c
SRCS+= x86_64-gcc.c
#CFLAGS+= -DAES_ASM # XXX ASM implementation SEGVs with MALLOC_OPTIONS=AFGJPRX
CFLAGS+= -DMD5_ASM
CFLAGS+= -DSHA1_ASM
CFLAGS+= -DOPENSSL_CPUID_OBJ
SSLASM=\
	bn x86_64-mont \
	md5 md5-x86_64 \
	rc4 rc4-x86_64 \
	sha sha1-x86_64 \
	aes aesni-x86_64
.for dir f in ${SSLASM}
SRCS+=	${f}.S
GENERATED+=${f}.S
${f}.S: ${LCRYPTO_SRC}/${dir}/asm/${f}.pl
	(cd ${LCRYPTO_SRC}/${dir} ; /usr/bin/perl ./asm/${f}.pl) > ${.TARGET}
.endfor
SRCS+=	x86_64cpuid.S
GENERATED+=x86_64cpuid.S
x86_64cpuid.S: ${LCRYPTO_SRC}/x86_64cpuid.pl
	(cd ${LCRYPTO_SRC}/${dir} ; /usr/bin/perl ./x86_64cpuid.pl) > ${.TARGET}
.else
CFLAGS+=-DOPENSSL_NO_ASM
SRCS+= aes_core.c aes_cbc.c
SRCS+= bf_enc.c	
SRCS+= bn_asm.c
SRCS+= rc4_enc.c rc4_skey.c
.endif

all beforedepend: ${GENERATED}

.include <bsd.lib.mk>
