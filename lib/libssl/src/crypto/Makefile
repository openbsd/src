LIB=	crypto
CFLAGS+= -DNO_IDEA -DTERMIOS -DL_ENDIAN -DANSI_SOURCE 
CFLAGS+= -I${.CURDIR}/../include
SRCS+=	cryptlib.c mem.c cversion.c ex_data.c cpt_err.c			
CFLAGS+= -I${.CURDIR}/md2
SRCS+=	md2_dgst.c md2_one.c 					
CFLAGS+= -I${.CURDIR}/md5
SRCS+=	md5_dgst.c md5_one.c 					
CFLAGS+= -I${.CURDIR}/sha
SRCS+=	sha_dgst.c sha1dgst.c sha_one.c sha1_one.c	
CFLAGS+= -I${.CURDIR}/mdc2
SRCS+=	mdc2dgst.c mdc2_one.c					
CFLAGS+= -I${.CURDIR}/hmac
SRCS+=	hmac.c							
CFLAGS+= -I${.CURDIR}/ripemd
SRCS+=	rmd_dgst.c rmd_one.c				
CFLAGS+= -I${.CURDIR}/des
SRCS+=	set_key.c ecb_enc.c cbc_enc.c ecb3_enc.c 	
SRCS+=	cfb64enc.c cfb64ede.c cfb_enc.c ofb64ede.c 	
SRCS+=	enc_read.c enc_writ.c ofb64enc.c ofb_enc.c 	
SRCS+=	str2key.c pcbc_enc.c qud_cksm.c rand_key.c	
SRCS+=	read2pwd.c fcrypt.c xcbc_enc.c read_pwd.c	
SRCS+=	rpc_enc.c cbc_cksm.c supp.c				
CFLAGS+= -I${.CURDIR}/rc2
SRCS+=	rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c 	
SRCS+=	rc2ofb64.c							
CFLAGS+= -I${.CURDIR}/rc4
SRCS+=	rc4_skey.c							
CFLAGS+= -I${.CURDIR}/rc5
SRCS+=	rc5_skey.c rc5_ecb.c rc5cfb64.c rc5cfb64.c 	
SRCS+=	rc5ofb64.c							
CFLAGS+= -I${.CURDIR}/idea
SRCS+=	i_cbc.c i_cfb64.c i_ofb64.c i_ecb.c		
SRCS+=	i_skey.c							
CFLAGS+= -I${.CURDIR}/bf
SRCS+=	bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c		
CFLAGS+= -I${.CURDIR}/cast
SRCS+=	c_skey.c c_ecb.c c_cfb64.c c_ofb64.c	
CFLAGS+= -I${.CURDIR}/bn
SRCS+=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_mod.c	
SRCS+=	bn_mul.c bn_print.c bn_rand.c bn_shift.c bn_sub.c
SRCS+=	bn_word.c bn_blind.c bn_gcd.c bn_prime.c bn_err.c
SRCS+=	bn_sqr.c bn_recp.c bn_mont.c bn_mpi.c		
CFLAGS+= -I${.CURDIR}/rsa
SRCS+=	rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c 	
SRCS+=	rsa_saos.c rsa_err.c rsa_pk1.c rsa_ssl.c 	
SRCS+=	rsa_none.c							
CFLAGS+= -I${.CURDIR}/dsa
SRCS+=	dsa_gen.c dsa_key.c dsa_lib.c dsa_vrf.c 	
SRCS+=	dsa_sign.c dsa_err.c 					
CFLAGS+= -I${.CURDIR}/dh
SRCS+=	dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c	
CFLAGS+= -I${.CURDIR}/buffer
SRCS+=	buffer.c buf_err.c				
CFLAGS+= -I${.CURDIR}/bio
SRCS+=	bio_lib.c bio_cb.c bio_err.c bss_mem.c 		
SRCS+=	bss_null.c bss_fd.c bss_file.c bss_sock.c	
SRCS+=	bss_conn.c bf_null.c bf_buff.c 
SRCS+=	b_print.c b_dump.c b_sock.c bss_acpt.c 		
SRCS+=	bf_nbio.c							
CFLAGS+= -I${.CURDIR}/stack
SRCS+=	stack.c							
CFLAGS+= -I${.CURDIR}/lhash
SRCS+=	lhash.c lh_stats.c					
CFLAGS+= -I${.CURDIR}/rand
SRCS+=	md_rand.c randfile.c					
CFLAGS+= -I${.CURDIR}/err
SRCS+=	err.c err_all.c err_prn.c				
CFLAGS+= -I${.CURDIR}/objects
SRCS+=	obj_dat.c obj_lib.c obj_err.c
CFLAGS+= -I${.CURDIR}/evp
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
CFLAGS+= -I${.CURDIR}/pem
SRCS+=	pem_sign.c pem_seal.c pem_info.c pem_lib.c	
SRCS+=	pem_all.c pem_err.c					
CFLAGS+= -I${.CURDIR}/asn1
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
CFLAGS+= -I${.CURDIR}/x509
SRCS+=	x509_def.c x509_d2.c x509_r2x.c x509_cmp.c	
SRCS+=	x509_obj.c x509_req.c x509_vfy.c x509_set.c	
SRCS+=	x509rset.c x509_err.c x509name.c x509_v3.c	
SRCS+=	x509_ext.c x509pack.c x509type.c x509_lu.c	
SRCS+=	x_all.c x509_txt.c by_file.c by_dir.c	
SRCS+=	v3_net.c v3_x509.c					
CFLAGS+= -I${.CURDIR}/conf
SRCS+=	conf.c conf_err.c					
CFLAGS+= -I${.CURDIR}/txt_db
SRCS+=	txt_db.c							
CFLAGS+= -I${.CURDIR}/pkcs7
SRCS+=	pk7_lib.c pkcs7err.c pk7_doit.c

.PATH:	${.CURDIR}/md2 ${.CURDIR}/md5 ${.CURDIR}/sha ${.CURDIR}/mdc2	\
	${.CURDIR}/hmac ${.CURDIR}/ripemd ${.CURDIR}/des  ${.CURDIR}/rc2 \
	${.CURDIR}/rc4 ${.CURDIR}/rc5 ${.CURDIR}/idea ${.CURDIR}/bf    \
	${.CURDIR}/cast ${.CURDIR}/bn  ${.CURDIR}/rsa ${.CURDIR}/dsa   \
	${.CURDIR}/dh ${.CURDIR}/buffer ${.CURDIR}/bio ${.CURDIR}/stack \
   	${.CURDIR}/lhash ${.CURDIR}/rand ${.CURDIR}/err ${.CURDIR}/objects \
	${.CURDIR}/evp ${.CURDIR}/pem ${.CURDIR}/asn1  ${.CURDIR}/asn1 \
	${.CURDIR}/x509 ${.CURDIR}/conf txt_db/txt_db.c ${.CURDIR}/pkcs7 \
	${.CURDIR}/txt_db

.include <bsd.lib.mk>


