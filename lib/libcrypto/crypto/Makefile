# $OpenBSD: Makefile,v 1.57 2015/04/11 16:16:15 deraadt Exp $

LIB=	crypto

SSL_SRC=	${.CURDIR}/../../libssl/src
LCRYPTO_SRC=	${SSL_SRC}/crypto

CFLAGS+= -Wall -Werror

.include <bsd.own.mk>		# for 'NOPIC' definition
.if !defined(NOPIC)
CFLAGS+= -DDSO_DLFCN -DHAVE_DLFCN_H -DHAVE_FUNOPEN
.endif

.if ${MACHINE_ARCH} == "sparc"
PICFLAG=-fPIC
.endif

CFLAGS+= -DLIBRESSL_INTERNAL
CFLAGS+= -DTERMIOS
# Hardware engines
CFLAGS+= -DOPENSSL_NO_HW_PADLOCK # XXX enable this?

CFLAGS+= -I${SSL_SRC}
CFLAGS+= -I${LCRYPTO_SRC}
CFLAGS+= -I${LCRYPTO_SRC}/modes -I${LCRYPTO_SRC}/asn1 -I${LCRYPTO_SRC}/evp

# crypto/
SRCS+= cryptlib.c malloc-wrapper.c mem_dbg.c cversion.c ex_data.c cpt_err.c
SRCS+= o_time.c o_str.c o_init.c
SRCS+= mem_clr.c

# aes/
SRCS+= aes_misc.c aes_ecb.c aes_cfb.c aes_ofb.c
SRCS+= aes_ctr.c aes_ige.c aes_wrap.c

# asn1/
SRCS+= a_object.c a_bitstr.c a_utctm.c a_gentm.c a_time.c a_int.c a_octet.c
SRCS+= a_print.c a_type.c a_dup.c a_d2i_fp.c a_i2d_fp.c
SRCS+= a_enum.c a_utf8.c a_sign.c a_digest.c a_verify.c a_mbstr.c a_strex.c
SRCS+= x_algor.c x_val.c x_pubkey.c x_sig.c x_req.c x_attrib.c x_bignum.c
SRCS+= x_long.c x_name.c x_x509.c x_x509a.c x_crl.c x_info.c x_spki.c nsseq.c
SRCS+= x_nx509.c d2i_pu.c d2i_pr.c i2d_pu.c i2d_pr.c
SRCS+= t_req.c t_x509.c t_x509a.c t_crl.c t_pkey.c t_spki.c t_bitst.c
SRCS+= tasn_new.c tasn_fre.c tasn_enc.c tasn_dec.c tasn_utl.c tasn_typ.c
SRCS+= tasn_prn.c ameth_lib.c
SRCS+= f_int.c f_string.c n_pkey.c
SRCS+= f_enum.c x_pkey.c a_bool.c x_exten.c bio_asn1.c bio_ndef.c asn_mime.c
SRCS+= asn1_gen.c asn1_par.c asn1_lib.c asn1_err.c a_bytes.c a_strnid.c
SRCS+= evp_asn1.c asn_pack.c p5_pbe.c p5_pbev2.c p8_pkey.c asn_moid.c
SRCS+= a_set.c

# bf/
SRCS+= bf_skey.c bf_ecb.c bf_cfb64.c bf_ofb64.c

# bio/
SRCS+= bio_lib.c bio_cb.c bio_err.c
SRCS+= bss_mem.c bss_null.c bss_fd.c
SRCS+= bss_file.c bss_sock.c bss_conn.c
SRCS+= bf_null.c bf_buff.c b_print.c b_dump.c
SRCS+= b_posix.c b_sock.c bss_acpt.c bf_nbio.c bss_log.c bss_bio.c
SRCS+= bss_dgram.c

# bn/
SRCS+= bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mul.c bn_mod.c
SRCS+= bn_print.c bn_rand.c bn_shift.c bn_word.c bn_blind.c
SRCS+= bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c bn_err.c bn_sqr.c
SRCS+= bn_recp.c bn_mont.c bn_mpi.c bn_exp2.c bn_gf2m.c bn_nist.c
SRCS+= bn_depr.c bn_const.c bn_x931p.c

# buffer/
SRCS+= buffer.c buf_err.c buf_str.c

# camellia/
SRCS+= cmll_cfb.c cmll_ctr.c cmll_ecb.c cmll_ofb.c

# cast/
SRCS+= c_skey.c c_ecb.c c_enc.c c_cfb64.c c_ofb64.c

# chacha/
SRCS+= chacha.c

# cmac/
SRCS+= cmac.c cm_ameth.c cm_pmeth.c

# cms/
#SRCS+= cms_lib.c cms_asn1.c cms_att.c cms_io.c cms_smime.c cms_err.c
#SRCS+= cms_sd.c cms_dd.c cms_cd.c cms_env.c cms_enc.c cms_ess.c
#SRCS+= cms_pwri.c

# comp/
SRCS+= comp_lib.c comp_err.c c_rle.c c_zlib.c

# conf/
SRCS+= conf_err.c conf_lib.c conf_api.c conf_def.c conf_mod.c
SRCS+= conf_mall.c conf_sap.c

# des/
SRCS+= cbc_cksm.c cbc_enc.c cfb64enc.c cfb_enc.c
SRCS+= ecb3_enc.c ecb_enc.c  enc_read.c enc_writ.c
SRCS+= fcrypt.c ofb64enc.c ofb_enc.c  pcbc_enc.c
SRCS+= qud_cksm.c rand_key.c set_key.c xcbc_enc.c
SRCS+= str2key.c  cfb64ede.c ofb64ede.c ede_cbcm_enc.c

# dh/
SRCS+= dh_asn1.c dh_gen.c dh_key.c dh_lib.c dh_check.c dh_err.c dh_depr.c
SRCS+= dh_ameth.c dh_pmeth.c dh_prn.c

# dsa/
SRCS+= dsa_gen.c dsa_key.c dsa_lib.c dsa_asn1.c dsa_vrf.c dsa_sign.c
SRCS+= dsa_err.c dsa_ossl.c dsa_depr.c dsa_ameth.c dsa_pmeth.c dsa_prn.c

# dso/
SRCS+= dso_dlfcn.c dso_err.c dso_lib.c dso_null.c
SRCS+= dso_openssl.c

# ec/
SRCS+= ec_lib.c ecp_smpl.c ecp_mont.c ecp_nist.c ec_cvt.c ec_mult.c
SRCS+= ec_err.c ec_curve.c ec_check.c ec_print.c ec_asn1.c ec_key.c
SRCS+= ec2_smpl.c ec2_mult.c ec_ameth.c ec_pmeth.c eck_prn.c
SRCS+= ecp_nistp224.c ecp_nistp256.c ecp_nistp521.c ecp_nistputil.c
SRCS+= ecp_oct.c ec2_oct.c ec_oct.c

# ecdh/
SRCS+= ech_lib.c ech_ossl.c ech_key.c ech_err.c

# ecdsa/
SRCS+= ecs_lib.c ecs_asn1.c ecs_ossl.c ecs_sign.c ecs_vrf.c ecs_err.c

# engine/
SRCS+= eng_err.c eng_lib.c eng_list.c eng_init.c eng_ctrl.c
SRCS+= eng_table.c eng_pkey.c eng_fat.c eng_all.c
SRCS+= tb_rsa.c tb_dsa.c tb_ecdsa.c tb_dh.c tb_ecdh.c tb_rand.c tb_store.c
SRCS+= tb_cipher.c tb_digest.c tb_pkmeth.c tb_asnmth.c
SRCS+= eng_openssl.c eng_cnf.c eng_dyn.c
SRCS+= eng_rsax.c
# XXX unnecessary? handled in EVP now...
# SRCS+= eng_aesni.c # local addition

# err/
SRCS+= err.c err_all.c err_prn.c

# evp/
SRCS+= encode.c digest.c evp_enc.c evp_key.c
SRCS+= e_des.c e_bf.c e_idea.c e_des3.c e_camellia.c
SRCS+= e_rc4.c e_aes.c names.c
SRCS+= e_xcbc_d.c e_rc2.c e_cast.c
SRCS+= m_null.c m_md4.c m_md5.c m_sha.c m_sha1.c m_wp.c
SRCS+= m_dss.c m_dss1.c m_mdc2.c m_ripemd.c m_ecdsa.c
SRCS+= p_open.c p_seal.c p_sign.c p_verify.c p_lib.c p_enc.c p_dec.c
SRCS+= bio_md.c bio_b64.c bio_enc.c evp_err.c e_null.c
SRCS+= c_all.c evp_lib.c
SRCS+= evp_pkey.c evp_pbe.c p5_crpt.c p5_crpt2.c
SRCS+= e_old.c pmeth_lib.c pmeth_fn.c pmeth_gn.c m_sigver.c
SRCS+= e_aes_cbc_hmac_sha1.c e_rc4_hmac_md5.c
SRCS+= e_chacha.c evp_aead.c e_chacha20poly1305.c
SRCS+= e_gost2814789.c m_gost2814789.c m_gostr341194.c m_streebog.c

# gost/
SRCS+= gost2814789.c gost89_keywrap.c gost89_params.c gost89imit_ameth.c
SRCS+= gost89imit_pmeth.c gost_asn1.c gost_err.c gostr341001.c
SRCS+= gostr341001_ameth.c gostr341001_key.c gostr341001_params.c
SRCS+= gostr341001_pmeth.c gostr341194.c streebog.c

# hmac/
SRCS+= hmac.c hm_ameth.c hm_pmeth.c

# idea/
SRCS+= i_cbc.c i_cfb64.c i_ofb64.c i_ecb.c i_skey.c

# krb5/
SRCS+= krb5_asn.c

# lhash/
SRCS+= lhash.c lh_stats.c

# md4/
SRCS+= md4_dgst.c md4_one.c

# md5/
SRCS+= md5_dgst.c md5_one.c

# mdc2/
SRCS+= mdc2dgst.c mdc2_one.c

# modes/
SRCS+= cbc128.c ctr128.c cts128.c cfb128.c ofb128.c gcm128.c ccm128.c xts128.c

# objects/
SRCS+= o_names.c obj_dat.c obj_lib.c obj_err.c obj_xref.c

# ocsp/
SRCS+= ocsp_asn.c ocsp_ext.c ocsp_ht.c ocsp_lib.c ocsp_cl.c
SRCS+= ocsp_srv.c ocsp_prn.c ocsp_vfy.c ocsp_err.c

# pem/
SRCS+= pem_sign.c pem_seal.c pem_info.c pem_lib.c pem_all.c pem_err.c
SRCS+= pem_x509.c pem_xaux.c pem_oth.c pem_pk8.c pem_pkey.c pvkfmt.c

# pkcs12/
SRCS+= p12_add.c p12_asn.c p12_attr.c p12_crpt.c p12_crt.c p12_decr.c
SRCS+= p12_init.c p12_key.c p12_kiss.c p12_mutl.c
SRCS+= p12_utl.c p12_npas.c pk12err.c p12_p8d.c p12_p8e.c

# pkcs7/
SRCS+= pk7_asn1.c pk7_lib.c pkcs7err.c pk7_doit.c pk7_smime.c pk7_attr.c
SRCS+= pk7_mime.c bio_pk7.c

# poly1305/
SRCS+= poly1305.c

# rand/
SRCS+= randfile.c rand_lib.c rand_err.c

# rc2/
SRCS+= rc2_ecb.c rc2_skey.c rc2_cbc.c rc2cfb64.c rc2ofb64.c

# ripemd/
SRCS+= rmd_dgst.c rmd_one.c

# rsa/
SRCS+= rsa_eay.c rsa_gen.c rsa_lib.c rsa_sign.c rsa_saos.c rsa_err.c
SRCS+= rsa_pk1.c rsa_ssl.c rsa_none.c rsa_oaep.c rsa_chk.c
SRCS+= rsa_pss.c rsa_x931.c rsa_asn1.c rsa_depr.c rsa_ameth.c rsa_prn.c
SRCS+= rsa_pmeth.c rsa_crpt.c

# sha/
SRCS+= sha_dgst.c sha1dgst.c sha_one.c sha1_one.c sha256.c sha512.c

# stack/
SRCS+= stack.c

# ts/
SRCS+= ts_err.c ts_req_utils.c ts_req_print.c ts_rsp_utils.c ts_rsp_print.c
SRCS+= ts_rsp_sign.c ts_rsp_verify.c ts_verify_ctx.c ts_lib.c ts_conf.c
SRCS+= ts_asn1.c

# txt_db/
SRCS+=txt_db.c

# ui/
SRCS+= ui_err.c ui_lib.c ui_openssl.c ui_util.c

# whrlpool/
SRCS+= wp_dgst.c

# x509/
SRCS+= x509_def.c x509_d2.c x509_r2x.c x509_cmp.c
SRCS+= x509_obj.c x509_req.c x509spki.c x509_vfy.c
SRCS+= x509_set.c x509cset.c x509rset.c x509_err.c
SRCS+= x509name.c x509_v3.c x509_ext.c x509_att.c
SRCS+= x509type.c x509_lu.c x_all.c x509_txt.c
SRCS+= x509_trs.c by_file.c by_dir.c by_mem.c x509_vpm.c

# x509v3/
SRCS+= v3_bcons.c v3_bitst.c v3_conf.c v3_extku.c v3_ia5.c v3_lib.c
SRCS+= v3_prn.c v3_utl.c v3err.c v3_genn.c v3_alt.c v3_skey.c v3_akey.c v3_pku.c
SRCS+= v3_int.c v3_enum.c v3_sxnet.c v3_cpols.c v3_crld.c v3_purp.c v3_info.c
SRCS+= v3_ocsp.c v3_akeya.c v3_pmaps.c v3_pcons.c v3_ncons.c v3_pcia.c v3_pci.c
SRCS+= pcy_cache.c pcy_node.c pcy_data.c pcy_map.c pcy_tree.c pcy_lib.c

.PATH:	${.CURDIR}/arch/${MACHINE_CPU} \
	${LCRYPTO_SRC} \
	${LCRYPTO_SRC}/aes \
	${LCRYPTO_SRC}/asn1 \
	${LCRYPTO_SRC}/bf \
	${LCRYPTO_SRC}/bio \
	${LCRYPTO_SRC}/bn \
	${LCRYPTO_SRC}/bn/asm \
	${LCRYPTO_SRC}/buffer \
	${LCRYPTO_SRC}/camellia \
	${LCRYPTO_SRC}/cast \
	${LCRYPTO_SRC}/chacha \
	${LCRYPTO_SRC}/cmac \
	${LCRYPTO_SRC}/cms \
	${LCRYPTO_SRC}/comp \
	${LCRYPTO_SRC}/conf \
	${LCRYPTO_SRC}/des \
	${LCRYPTO_SRC}/dh \
	${LCRYPTO_SRC}/dsa \
	${LCRYPTO_SRC}/dso \
	${LCRYPTO_SRC}/ec \
	${LCRYPTO_SRC}/ecdh \
	${LCRYPTO_SRC}/ecdsa \
	${LCRYPTO_SRC}/engine \
	${LCRYPTO_SRC}/err \
	${LCRYPTO_SRC}/evp \
	${LCRYPTO_SRC}/gost \
	${LCRYPTO_SRC}/hmac \
	${LCRYPTO_SRC}/idea \
	${LCRYPTO_SRC}/krb5 \
	${LCRYPTO_SRC}/lhash \
	${LCRYPTO_SRC}/md4 \
	${LCRYPTO_SRC}/md5 \
	${LCRYPTO_SRC}/mdc2 \
	${LCRYPTO_SRC}/modes \
	${LCRYPTO_SRC}/objects \
	${LCRYPTO_SRC}/ocsp \
	${LCRYPTO_SRC}/pem \
	${LCRYPTO_SRC}/perlasm \
	${LCRYPTO_SRC}/pkcs12 \
	${LCRYPTO_SRC}/pkcs7 \
	${LCRYPTO_SRC}/poly1305 \
	${LCRYPTO_SRC}/rand \
	${LCRYPTO_SRC}/rc2 \
	${LCRYPTO_SRC}/rc4 \
	${LCRYPTO_SRC}/ripemd \
	${LCRYPTO_SRC}/rsa \
	${LCRYPTO_SRC}/sha \
	${LCRYPTO_SRC}/stack \
	${LCRYPTO_SRC}/threads \
	${LCRYPTO_SRC}/ts \
	${LCRYPTO_SRC}/txt_db \
	${LCRYPTO_SRC}/ui \
	${LCRYPTO_SRC}/whrlpool \
	${LCRYPTO_SRC}/x509 \
	${LCRYPTO_SRC}/x509v3

HDRS=\
	e_os2.h \
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
	crypto/chacha/chacha.h \
	crypto/cmac/cmac.h \
	crypto/cms/cms.h \
	crypto/comp/comp.h \
	crypto/conf/conf.h \
	crypto/conf/conf_api.h \
	crypto/crypto.h \
	crypto/des/des.h \
	crypto/dh/dh.h \
	crypto/dsa/dsa.h \
	crypto/dso/dso.h \
	crypto/ec/ec.h \
	crypto/ecdh/ecdh.h \
	crypto/ecdsa/ecdsa.h \
	crypto/engine/engine.h \
	crypto/err/err.h \
	crypto/evp/evp.h \
	crypto/gost/gost.h \
	crypto/hmac/hmac.h \
	crypto/idea/idea.h \
	crypto/krb5/krb5_asn.h \
	crypto/lhash/lhash.h \
	crypto/md4/md4.h \
	crypto/md5/md5.h \
	crypto/mdc2/mdc2.h \
	crypto/modes/modes.h \
	crypto/objects/objects.h \
	crypto/ocsp/ocsp.h \
	crypto/opensslfeatures.h \
	crypto/opensslv.h \
	crypto/ossl_typ.h \
	crypto/pem/pem.h \
	crypto/pem/pem2.h \
	crypto/pkcs12/pkcs12.h \
	crypto/pkcs7/pkcs7.h \
	crypto/poly1305/poly1305.h \
	crypto/rand/rand.h \
	crypto/rc2/rc2.h \
	crypto/rc4/rc4.h \
	crypto/ripemd/ripemd.h \
	crypto/rsa/rsa.h \
	crypto/sha/sha.h \
	crypto/stack/safestack.h \
	crypto/stack/stack.h \
	crypto/ts/ts.h \
	crypto/txt_db/txt_db.h \
	crypto/ui/ui.h \
	crypto/ui/ui_compat.h \
	crypto/whrlpool/whrlpool.h \
	crypto/x509/x509.h \
	crypto/x509/x509_vfy.h \
	crypto/x509v3/x509v3.h

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

.if exists (${.CURDIR}/arch/${MACHINE_CPU}/Makefile.inc)
.include "${.CURDIR}/arch/${MACHINE_CPU}/Makefile.inc"
.else
CFLAGS+=-DOPENSSL_NO_ASM
SRCS+= aes_core.c aes_cbc.c
SRCS+= bf_enc.c
SRCS+= bn_asm.c
SRCS+= camellia.c cmll_cbc.c cmll_misc.c
SRCS+= des_enc.c fcrypt_b.c
SRCS+= rc4_enc.c rc4_skey.c
SRCS+= wp_block.c
.endif

all beforedepend: ${GENERATED}

.include <bsd.lib.mk>
