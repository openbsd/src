
LIB=	crypto

SSLEAYDIST= src-patent

LCRYPTO_SRC= ${.CURDIR}/../${SSLEAYDIST}/crypto

.if ${MACHINE_ARCH} == "i386"
CFLAGS+= -DL_ENDIAN -DBN_ASM
.else
.if ${MACHINE_ARCH} == "arc"
CFLAGS+= -DL_ENDIAN
.else
.if ${MACHINE_ARCH} == "pmax"
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

CFLAGS+= -DNO_IDEA -DTERMIOS -DANSI_SOURCE 
CFLAGS+= -I${.CURDIR}/../${SSLEAYDIST}
CFLAGS+= -I${LCRYPTO_SRC}
SRCS+=	cryptlib.c mem.c cversion.c ex_data.c cpt_err.c			
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
CFLAGS+= -I${LCRYPTO_SRC}/des
SRCS+=	cbc_cksm.c cbc_enc.c  cfb64enc.c cfb_enc.c  \
	ecb3_enc.c ecb_enc.c  enc_read.c enc_writ.c \
	fcrypt.c ofb64enc.c ofb_enc.c  pcbc_enc.c \
	qud_cksm.c rand_key.c read_pwd.c rpc_enc.c  set_key.c  \
	des_enc.c fcrypt_b.c read2pwd.c \
	fcrypt.c xcbc_enc.c \
	str2key.c  cfb64ede.c ofb64ede.c supp.c
CFLAGS+= -I${LCRYPTO_SRC}/rc2
SRCS+=	rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c 	
SRCS+=	rc2ofb64.c							
CFLAGS+= -I${LCRYPTO_SRC}/rc4
SRCS+=	rc4_skey.c rc4_enc.c
CFLAGS+= -I${LCRYPTO_SRC}/rc5
SRCS+=	rc5_skey.c rc5_ecb.c rc5cfb64.c rc5cfb64.c 	
SRCS+=	rc5ofb64.c rc5_enc.c
CFLAGS+= -I${LCRYPTO_SRC}/idea
SRCS+=	i_cbc.c i_cfb64.c i_ofb64.c i_ecb.c		
SRCS+=	i_skey.c							
CFLAGS+= -I${LCRYPTO_SRC}/bf
SRCS+=	bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c bf_enc.c		
CFLAGS+= -I${LCRYPTO_SRC}/cast
SRCS+=	c_skey.c c_ecb.c c_cfb64.c c_ofb64.c c_enc.c	
CFLAGS+= -I${LCRYPTO_SRC}/bn
SRCS+=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_mod.c	
SRCS+=	bn_mul.c bn_print.c bn_rand.c bn_shift.c bn_sub.c
SRCS+=	bn_word.c bn_blind.c bn_gcd.c bn_prime.c bn_err.c
SRCS+=	bn_sqr.c bn_recp.c bn_mont.c bn_mpi.c		
SRCS+=  bn_mulw.c 
CFLAGS+= -I${LCRYPTO_SRC}/rsa
SRCS+=	rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c 	
SRCS+=	rsa_saos.c rsa_err.c rsa_pk1.c rsa_ssl.c 	
SRCS+=	rsa_none.c							
CFLAGS+= -I${LCRYPTO_SRC}/dsa
SRCS+=	dsa_gen.c dsa_key.c dsa_lib.c dsa_vrf.c 	
SRCS+=	dsa_sign.c dsa_err.c 					
CFLAGS+= -I${LCRYPTO_SRC}/dh
SRCS+=	dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c	
CFLAGS+= -I${LCRYPTO_SRC}/buffer
SRCS+=	buffer.c buf_err.c				
CFLAGS+= -I${LCRYPTO_SRC}/bio
SRCS+=	bio_lib.c bio_cb.c bio_err.c bss_mem.c 		
SRCS+=	bss_null.c bss_fd.c bss_file.c bss_sock.c	
SRCS+=	bss_conn.c bf_null.c bf_buff.c 
SRCS+=	b_print.c b_dump.c b_sock.c bss_acpt.c 		
SRCS+=	bf_nbio.c							
CFLAGS+= -I${LCRYPTO_SRC}/stack
SRCS+=	stack.c							
CFLAGS+= -I${LCRYPTO_SRC}/lhash
SRCS+=	lhash.c lh_stats.c					
CFLAGS+= -I${LCRYPTO_SRC}/rand
SRCS+=	md_rand.c randfile.c					
CFLAGS+= -I${LCRYPTO_SRC}/err
SRCS+=	err.c err_all.c err_prn.c				
CFLAGS+= -I${LCRYPTO_SRC}/objects
SRCS+=	obj_dat.c obj_lib.c obj_err.c
CFLAGS+= -I${LCRYPTO_SRC}/evp
SRCS+=	encode.c digest.c evp_enc.c evp_key.c 		
SRCS+=	e_ecb_d.c e_cbc_d.c e_cfb_d.c e_ofb_d.c		
SRCS+=	e_ecb_i.c e_cbc_i.c e_cfb_i.c e_ofb_i.c		
SRCS+=	e_ecb_3d.c e_cbc_3d.c e_rc4.c names.c		
SRCS+=	e_cfb_3d.c e_ofb_3d.c e_xcbc_d.c e_ecb_r2.c	
SRCS+=	e_cbc_r2.c e_cfb_r2.c e_ofb_r2.c e_ecb_bf.c	
SRCS+=	e_cbc_bf.c e_cfb_bf.c e_ofb_bf.c e_ecb_c.c	
SRCS+=	e_cbc_c.c e_cfb_c.c e_ofb_c.c e_ecb_r5.c	
SRCS+=	e_cbc_r5.c e_cfb_r5.c e_ofb_r5.c m_null.c	
SRCS+=	m_md2.c m_md5.c m_sha.c m_sha1.c m_dss.c	
SRCS+=	m_dss1.c m_mdc2.c m_ripemd.c p_open.c 		
SRCS+=	p_seal.c p_sign.c p_verify.c p_lib.c p_enc.c
SRCS+=	p_dec.c bio_md.c bio_b64.c bio_enc.c		
SRCS+=	evp_err.c e_null.c c_all.c evp_lib.c
CFLAGS+= -I${LCRYPTO_SRC}/pem
SRCS+=	pem_sign.c pem_seal.c pem_info.c pem_lib.c	
SRCS+=	pem_all.c pem_err.c					
CFLAGS+= -I${LCRYPTO_SRC}/asn1
SRCS+=	a_object.c a_bitstr.c a_utctm.c a_int.c	
SRCS+=	a_octet.c a_print.c a_type.c a_set.c	
SRCS+=	a_dup.c a_d2i_fp.c a_i2d_fp.c a_sign.c	
SRCS+=	a_digest.c a_verify.c x_algor.c x_val.c	
SRCS+=	x_pubkey.c x_sig.c x_req.c x_attrib.c	
SRCS+=	x_name.c x_cinf.c x_x509.c x_crl.c		
SRCS+=	x_info.c x_spki.c d2i_r_pr.c i2d_r_pr.c	
SRCS+=	d2i_r_pu.c i2d_r_pu.c d2i_s_pr.c i2d_s_pr.c	
SRCS+=	d2i_s_pu.c i2d_s_pu.c d2i_pu.c d2i_pr.c 	
SRCS+=	i2d_pu.c i2d_pr.c t_req.c t_x509.c 		
SRCS+=	t_pkey.c p7_i_s.c p7_signi.c p7_signd.c	
SRCS+=	p7_recip.c p7_enc_c.c p7_evp.c p7_dgst.c	
SRCS+=	p7_s_e.c p7_enc.c p7_lib.c f_int.c 		
SRCS+=	f_string.c i2d_dhp.c i2d_dsap.c d2i_dhp.c	
SRCS+=	d2i_dsap.c n_pkey.c a_hdr.c x_pkey.c 	
SRCS+=	a_bool.c x_exten.c asn1_par.c asn1_lib.c	
SRCS+=	asn1_err.c a_meth.c a_bytes.c evp_asn1.c	
CFLAGS+= -I${LCRYPTO_SRC}/x509
SRCS+=	x509_def.c x509_d2.c x509_r2x.c x509_cmp.c	
SRCS+=	x509_obj.c x509_req.c x509_vfy.c x509_set.c	
SRCS+=	x509rset.c x509_err.c x509name.c x509_v3.c	
SRCS+=	x509_ext.c x509pack.c x509type.c x509_lu.c	
SRCS+=	x_all.c x509_txt.c by_file.c by_dir.c	
SRCS+=	v3_net.c v3_x509.c					
CFLAGS+= -I${LCRYPTO_SRC}/conf
SRCS+=	conf.c conf_err.c					
CFLAGS+= -I${LCRYPTO_SRC}/txt_db
SRCS+=	txt_db.c							
CFLAGS+= -I${LCRYPTO_SRC}/pkcs7
SRCS+=	pk7_lib.c pkcs7err.c pk7_doit.c

HDRS= 	asn1/asn1.h asn1/asn1_mac.h bf/blowfish.h bio/bio.h \
	bn/bn.h buffer/buffer.h cast/cast.h conf/conf.h des/des.h dh/dh.h \
	dsa/dsa.h err/err.h evp/evp.h hmac/hmac.h idea/idea.h lhash/lhash.h \
	md2/md2.h md5/md5.h mdc2/mdc2.h objects/objects.h pem/pem.h \
	pkcs7/pkcs7.h rand/rand.h rc2/rc2.h rc4/rc4.h rc5/rc5.h \
	ripemd/ripemd.h rsa/rsa.h sha/sha.h stack/stack.h txt_db/txt_db.h \
	x509/x509.h x509/x509_vfy.h crypto.h cryptall.h ../e_os.h

.PATH:	${LCRYPTO_SRC}/md2 ${LCRYPTO_SRC}/md5 ${LCRYPTO_SRC}/sha ${LCRYPTO_SRC}/mdc2	\
	${LCRYPTO_SRC}/hmac ${LCRYPTO_SRC}/ripemd ${LCRYPTO_SRC}/des  ${LCRYPTO_SRC}/rc2 \
	${LCRYPTO_SRC}/rc4 ${LCRYPTO_SRC}/rc5 ${LCRYPTO_SRC}/idea ${LCRYPTO_SRC}/bf    \
	${LCRYPTO_SRC}/cast ${LCRYPTO_SRC}/bn  ${LCRYPTO_SRC}/rsa ${LCRYPTO_SRC}/dsa   \
	${LCRYPTO_SRC}/dh ${LCRYPTO_SRC}/buffer ${LCRYPTO_SRC}/bio ${LCRYPTO_SRC}/stack \
   	${LCRYPTO_SRC}/lhash ${LCRYPTO_SRC}/rand ${LCRYPTO_SRC}/err ${LCRYPTO_SRC}/objects \
	${LCRYPTO_SRC}/evp ${LCRYPTO_SRC}/pem ${LCRYPTO_SRC}/asn1  ${LCRYPTO_SRC}/asn1 \
	${LCRYPTO_SRC}/x509 ${LCRYPTO_SRC}/conf txt_db/txt_db.c ${LCRYPTO_SRC}/pkcs7 \
	${LCRYPTO_SRC}/txt_db ${LCRYPTO_SRC}

includes:
	@cd ${LCRYPTO_SRC}; for i in $(HDRS); do \
		j="cmp -s ${LCRYPTO_SRC}/$$i \
		${DESTDIR}/usr/include/ssl/`basename $$i` || \
		${INSTALL} ${INSTALL_COPY} -o ${BINOWN} -g ${BINGRP} -m 444 \
		${LCRYPTO_SRC}/$$i ${DESTDIR}/usr/include/ssl"; \
		echo $$j; \
		eval "$$j"; \
	done


.include <bsd.lib.mk>

