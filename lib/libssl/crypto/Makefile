# $OpenBSD: Makefile,v 1.25 2002/09/03 18:59:55 markus Exp $

LIB=	crypto

SSLEAYDIST=	src
SSL_SRC=	${.CURDIR}/../${SSLEAYDIST}
LCRYPTO_SRC=	${SSL_SRC}/crypto

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "mips"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "vax"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "alpha"
# no ENDIAN stuff defined for alpha
.else
CFLAGS+= -DB_ENDIAN
.endif
.endif
.endif
.endif 

CFLAGS+= -DOPENSSL_NO_IDEA -DTERMIOS -DANSI_SOURCE -DNO_ERR -DOPENSSL_NO_ASM
CFLAGS+= -DOPENSSL_NO_RC5 -DOPENSSL_NO_KRB5 -DSO_DLFCN -DHAVE_DLFCN_H 
CFLAGS+= -DNO_WINDOWS_BRAINDEATH
CFLAGS+= -DOPENSSL_NO_HW_CSWIFT -DOPENSSL_NO_HW_NCIPHER -DOPENSSL_NO_HW_ATALLA
CFLAGS+= -DOPENSSL_NO_HW_NURON -DOPENSSL_NO_HW_UBSEC -DOPENSSL_NO_HW_AEP
CFLAGS+= -DOPENSSL_NO_HW_SUREWARE
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST}
CFLAGS+= -I${LCRYPTO_SRC}
SRCS+= o_time.c
SRCS+=	cryptlib.c ex_data.c cpt_err.c mem.c mem_dbg.c tmdiff.c cversion.c uid.c
CFLAGS+= -I${LCRYPTO_SRC}/md2
SRCS+=	md2_dgst.c md2_one.c 					
CFLAGS+= -I${LCRYPTO_SRC}/md5
SRCS+=	md5_dgst.c md5_one.c 					
CFLAGS+= -I${LCRYPTO_SRC}/sha
SRCS+=	sha_dgst.c sha1dgst.c sha_one.c sha1_one.c	
CFLAGS+= -I${LCRYPTO_SRC}/mdc2
SRCS+=	mdc2dgst.c mdc2_one.c					
CFLAGS+= -I${LCRYPTO_SRC}/hmac
SRCS+=	hmac.c							
CFLAGS+= -I${LCRYPTO_SRC}/ripemd
SRCS+=	rmd_dgst.c rmd_one.c				
CFLAGS+= -I${LCRYPTO_SRC}/aes
SRCS+= aes_cbc.c aes_cfb.c aes_ctr.c aes_ecb.c aes_ofb.c aes_misc.c aes_core.c
CFLAGS+= -I${LCRYPTO_SRC}/des
SRCS+=	cbc_cksm.c cbc_enc.c cfb64enc.c cfb_enc.c \
	ecb3_enc.c ecb_enc.c enc_read.c enc_writ.c \
	fcrypt.c ofb64enc.c ofb_enc.c pcbc_enc.c \
	qud_cksm.c rand_key.c rpc_enc.c set_key.c \
	des_enc.c des_old.c fcrypt_b.c \
	fcrypt.c xcbc_enc.c ede_cbcm_enc.c \
	str2key.c cfb64ede.c ofb64ede.c \
	des_old.c read2pwd.c
CFLAGS+= -I${LCRYPTO_SRC}/rc2
SRCS+=	rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c
SRCS+=	rc2ofb64.c							
CFLAGS+= -I${LCRYPTO_SRC}/rc4
SRCS+=	rc4_skey.c rc4_enc.c
#CFLAGS+= -I${LCRYPTO_SRC}/rc5
#SRCS+=	rc5_skey.c rc5_ecb.c rc5cfb64.c rc5cfb64.c 	
#SRCS+=	rc5ofb64.c rc5_enc.c
#CFLAGS+= -I${LCRYPTO_SRC}/idea
#SRCS+=	i_cbc.c i_cfb64.c i_ofb64.c i_ecb.c		
#SRCS+=	i_skey.c							
CFLAGS+= -I${LCRYPTO_SRC}/bf
SRCS+=	bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c bf_enc.c		
CFLAGS+= -I${LCRYPTO_SRC}/cast
SRCS+=	c_skey.c c_ecb.c c_cfb64.c c_ofb64.c c_enc.c	
CFLAGS+= -I${LCRYPTO_SRC}/bn
SRCS+=	bn_add.c bn_div.c bn_exp.c bn_lib.c
SRCS+=	bn_mul.c bn_print.c bn_rand.c bn_shift.c
SRCS+=	bn_word.c bn_blind.c bn_gcd.c bn_prime.c bn_err.c
SRCS+=	bn_sqr.c bn_recp.c bn_mont.c bn_mpi.c bn_asm.c bn_mod.c
SRCS+=	bn_exp2.c bn_ctx.c
SRCS+=	bn_sqrt.c bn_kron.c
CFLAGS+= -I${LCRYPTO_SRC}/rsa
SRCS+=	rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c
SRCS+=	rsa_saos.c rsa_err.c rsa_pk1.c rsa_ssl.c
SRCS+=	rsa_none.c rsa_chk.c rsa_oaep.c rsa_null.c rsa_asn1.c
CFLAGS+= -I${LCRYPTO_SRC}/dsa
SRCS+=	dsa_gen.c dsa_key.c dsa_lib.c dsa_vrf.c 	
SRCS+=	dsa_sign.c dsa_err.c dsa_asn1.c dsa_ossl.c
CFLAGS+= -I${LCRYPTO_SRC}/dh
SRCS+=	dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c dh_asn1.c
CFLAGS+= -I${LCRYPTO_SRC}/buffer
SRCS+=	buffer.c buf_err.c				
CFLAGS+= -I${LCRYPTO_SRC}/bio
SRCS+=	bio_lib.c bio_cb.c bio_err.c bss_mem.c 		
SRCS+=	bss_null.c bss_fd.c bss_file.c bss_sock.c	
SRCS+=	bss_conn.c bf_null.c bf_buff.c 
SRCS+=	b_print.c b_dump.c b_sock.c bss_acpt.c 		
SRCS+=	bf_nbio.c bss_bio.c bss_log.c
CFLAGS+= -I${LCRYPTO_SRC}/stack
SRCS+=	stack.c							
CFLAGS+= -I${LCRYPTO_SRC}/lhash
SRCS+=	lhash.c lh_stats.c					
CFLAGS+= -I${LCRYPTO_SRC}/rand
SRCS+=	md_rand.c randfile.c rand_lib.c	rand_egd.c rand_err.c rand_unix.c
CFLAGS+= -I${LCRYPTO_SRC}/err
SRCS+=	err.c err_all.c err_prn.c				
CFLAGS+= -I${LCRYPTO_SRC}/objects
SRCS+=	obj_dat.c obj_lib.c obj_err.c o_names.c
CFLAGS+= -I${LCRYPTO_SRC}/evp
SRCS+= bio_b64.c e_bf.c m_sha.c p_open.c
SRCS+= bio_enc.c e_cast.c e_xcbc_d.c m_dss.c m_sha1.c p_seal.c
SRCS+= bio_md.c e_des.c encode.c m_dss1.c names.c p_sign.c
SRCS+= bio_ok.c e_des3.c evp_enc.c m_md2.c p_verify.c
SRCS+= c_all.c evp_err.c m_md4.c p5_crpt.c
SRCS+= c_allc.c evp_key.c m_md5.c p5_crpt2.c
SRCS+= c_alld.c e_null.c evp_lib.c m_mdc2.c p_dec.c
SRCS+= digest.c e_rc2.c evp_pbe.c m_null.c p_enc.c
SRCS+= e_aes.c e_rc4.c evp_pkey.c m_ripemd.c p_lib.c
CFLAGS+= -I${LCRYPTO_SRC}/md4
SRCS+= md4_dgst.c md4_one.c
CFLAGS+= -I${LCRYPTO_SRC}/pem
SRCS+=	pem_sign.c pem_seal.c pem_info.c pem_lib.c pem_pkey.c	
SRCS+=	pem_all.c pem_err.c pem_x509.c pem_pk8.c pem_oth.c pem_xaux.c
CFLAGS+= -I${LCRYPTO_SRC}/ui
SRCS+=	ui_err.c ui_lib.c ui_openssl.c ui_compat.c ui_util.c
CFLAGS+= -I${LCRYPTO_SRC}/asn1
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
SRCS+=a_int.c a_type.c p8_pkey.c tasn_new.c x_long.c asn_moid.c
CFLAGS+= -I${LCRYPTO_SRC}/x509
SRCS+= x509_d2.c x509_lu.c x509_set.c x509_vfy.c x509spki.c by_dir.c
SRCS+= x509_def.c x509_obj.c x509_trs.c x509cset.c x509type.c by_file.c
SRCS+= x509_att.c x509_err.c x509_r2x.c x509_txt.c x509name.c x_all.c
SRCS+= x509_cmp.c x509_ext.c x509_req.c x509_v3.c x509rset.c
CFLAGS+= -I${LCRYPTO_SRC}/x509v3
SRCS+=	v3_akey.c v3_alt.c v3_bcons.c v3_bitst.c v3_conf.c v3_cpols.c
SRCS+=	v3_crld.c v3_enum.c v3_extku.c v3_genn.c v3_ia5.c v3_int.c
SRCS+=	v3_lib.c v3_pku.c v3_prn.c v3_skey.c v3_sxnet.c v3_utl.c
SRCS+=	v3err.c v3_info.c v3_purp.c v3_ocsp.c v3_akeya.c
CFLAGS+= -I${LCRYPTO_SRC}/conf
SRCS+=	conf_err.c conf_lib.c conf_def.c conf_api.c conf_mod.c conf_mall.c
CFLAGS+= -I${LCRYPTO_SRC}/txt_db
SRCS+=	txt_db.c							
CFLAGS+= -I${LCRYPTO_SRC}/pkcs7
SRCS+= pk7_lib.c pkcs7err.c
SRCS+= pk7_asn1.c pk7_doit.c pk7_mime.c 
SRCS+= pk7_attr.c pk7_smime.c
CFLAGS+= -I${LCRYPTO_SRC}/comp
SRCS+=	c_rle.c c_zlib.c comp_lib.c
CFLAGS+= -I${LCRYPTO_SRC}/pkcs12
SRCS+=	p12_add.c p12_crpt.c p12_init.c p12_mutl.c p12_p8e.c
SRCS+=	p12_asn.c p12_crt.c p12_key.c p12_npas.c p12_utl.c
SRCS+=	p12_attr.c p12_decr.c p12_kiss.c p12_p8d.c pk12err.c
CFLAGS+= -I${LCRYPTO_SRC}/engine
SRCS+= eng_all.c eng_openssl.c eng_ctrl.c eng_pkey.c
SRCS+= eng_dyn.c eng_table.c tb_cipher.c eng_err.c tb_rsa.c
SRCS+= hw_cryptodev.c eng_cnf.c
SRCS+= tb_dh.c eng_fat.c tb_digest.c eng_init.c
SRCS+= tb_dsa.c eng_lib.c tb_rand.c eng_list.c
SRCs+= tb_rsa.c
CFLAGS+= -I${LCRYPTO_SRC}/dso
SRCS+= dso_dl.c dso_dlfcn.c dso_err.c dso_lib.c dso_null.c
SRCS+= dso_openssl.c dso_win32.c dso_vms.c
CFLAGS+= -I${LCRYPTO_SRC}/oscp
SRCS+= ocsp_asn.c ocsp_err.c ocsp_ht.c ocsp_prn.c ocsp_vfy.c
SRCS+= ocsp_cl.c ocsp_ext.c ocsp_lib.c ocsp_srv.c
CFLAGS+= -I${LCRYPTO_SRC}/ec
SRCS+= ec_cvt.c ec_lib.c ecp_mont.c ecp_recp.c
SRCS+= ec_err.c ec_mult.c ecp_nist.c ecp_smpl.c

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
	${LCRYPTO_SRC}/ocsp ${LCRYPTO_SRC}/ec ${LCRYPTO_SRC}/aes ${LCRYPTO_SRC}

HDRS=\
	crypto/aes/aes.h \
	crypto/asn1/asn1.h \
	crypto/asn1/asn1_mac.h \
	crypto/asn1/asn1t.h \
	crypto/bf/blowfish.h \
	crypto/bio/bio.h \
	crypto/bn/bn.h \
	crypto/buffer/buffer.h \
	crypto/cast/cast.h \
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
	crypto/rand/rand.h \
	crypto/rc2/rc2.h \
	crypto/rc4/rc4.h \
	crypto/rc5/rc5.h \
	crypto/ripemd/ripemd.h \
	crypto/rsa/rsa.h \
	crypto/sha/sha.h \
	crypto/stack/safestack.h \
	crypto/stack/stack.h \
	crypto/symhacks.h \
	crypto/tmdiff.h \
	crypto/txt_db/txt_db.h \
	crypto/ui/ui.h \
	crypto/ui/ui_compat.h \
	crypto/x509/x509.h \
	crypto/x509/x509_vfy.h \
	crypto/x509v3/x509v3.h \
	e_os2.h

HDRS_GEN=\
	${.CURDIR}/arch/${MACHINE_ARCH}/opensslconf.h \
	${.OBJDIR}/obj_mac.h

includes: obj_mac.h
	@test -d ${DESTDIR}/usr/include/ssl || mkdir ${DESTDIR}/usr/include/ssl
	@d=`mktemp -d /tmp/libsslXXXXXXXXXX`; \
	for i in $(HDRS); do \
		f=`basename $$i`; \
		j="sed 's/<openssl/<ssl/' \
		       ${SSL_SRC}/$$i >$$d/$$f && \
		(cmp -s $$d/$$f ${DESTDIR}/usr/include/ssl/$$f || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
			   $$d/$$f ${DESTDIR}/usr/include/ssl)"; \
		echo $$j; \
		eval "$$j"; \
	done; \
	for i in $(HDRS_GEN); do \
		f=`basename $$i`; \
		j="sed 's/<openssl/<ssl/' \
		       $$i >$$d/$$f && \
		(cmp -s $$d/$$f ${DESTDIR}/usr/include/ssl/$$f || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
			   $$d/$$f ${DESTDIR}/usr/include/ssl)"; \
		echo $$j; \
		eval "$$j"; \
	done; \
	rm -rf $$d

# generated
CFLAGS+= -I${.OBJDIR}

GENERATED=obj_mac.h obj_dat.h
CLEANFILES=${GENERATED}
SSL_OBJECTS=${SSL_SRC}/crypto/objects

obj_mac.h: ${SSL_OBJECTS}/objects.h
	/usr/bin/perl ${SSL_OBJECTS}/objects.pl ${SSL_OBJECTS}/objects.txt ${SSL_OBJECTS}/obj_mac.num obj_mac.h
obj_dat.h: obj_mac.h
	/usr/bin/perl ${SSL_OBJECTS}/obj_dat.pl obj_mac.h obj_dat.h

.if (${MACHINE_ARCH} == "vax")
# egcs bombs optimising this file on vax
a_strnid.o:
	${CC} ${CFLAGS} -O0 ${CPPFLAGS} -c ${.IMPSRC}
a_strnid.po:           
	${CC} ${CFLAGS} -O0 ${CPPFLAGS} -c ${.IMPSRC} -o $@
des_enc.o:             
	${CC} ${CFLAGS} -O1 ${CPPFLAGS} -c ${.IMPSRC}
des_enc.po:            
	${CC} ${CFLAGS} -O1 ${CPPFLAGS} -c ${.IMPSRC} -o $@
.endif  

all beforedepend: ${GENERATED}

.include <bsd.lib.mk>
