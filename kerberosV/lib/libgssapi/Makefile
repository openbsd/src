# $OpenBSD: Makefile,v 1.9 2013/11/02 14:15:07 ajacoutot Exp $

LIB=	gssapi

GEN_GSSAPI=	asn1_GSSAPIContextToken.x gssapi_asn1-priv.hx gssapi_asn1.hx

GEN_SPNEGO=	asn1_ContextFlags.x asn1_MechType.x asn1_MechTypeList.x asn1_NegHints.x \
		asn1_NegTokenInit.x asn1_NegTokenInitWin.x asn1_NegTokenResp.x asn1_NegotiationToken.x \
		asn1_NegotiationTokenWin.x spnego_asn1-priv.hx spnego_asn1.hx

GEN=	${GEN_GSSAPI} ${GEN_SPNEGO}

GGEN=	asn1_ContextFlags.x		\
	asn1_GSSAPIContextToken.x	\
	asn1_MechType.x			\
	asn1_MechTypeList.x		\
	asn1_NegotiationToken.x		\
	asn1_NegotiationTokenWin.x	\
	asn1_NegHints.x			\
	asn1_NegTokenInit.x		\
	asn1_NegTokenInitWin.x		\
	asn1_NegTokenResp.x		\

krb5src= \
	krb5/8003.c \
	krb5/accept_sec_context.c \
	krb5/acquire_cred.c \
	krb5/add_cred.c \
	krb5/address_to_krb5addr.c \
	krb5/aeap.c \
	krb5/arcfour.c \
	krb5/authorize_localname.c \
	krb5/canonicalize_name.c \
	krb5/creds.c \
	krb5/ccache_name.c \
	krb5/cfx.c \
	krb5/compare_name.c \
	krb5/compat.c \
	krb5/context_time.c \
	krb5/copy_ccache.c \
	krb5/decapsulate.c \
	krb5/delete_sec_context.c \
	krb5/display_name.c \
	krb5/display_status.c \
	krb5/duplicate_name.c \
	krb5/encapsulate.c \
	krb5/export_name.c \
	krb5/export_sec_context.c \
	krb5/external.c \
	krb5/get_mic.c \
	krb5/import_name.c \
	krb5/import_sec_context.c \
	krb5/indicate_mechs.c \
	krb5/init.c \
	krb5/init_sec_context.c \
	krb5/inquire_context.c \
	krb5/inquire_cred.c \
	krb5/inquire_cred_by_mech.c \
	krb5/inquire_cred_by_oid.c \
	krb5/inquire_mechs_for_name.c \
	krb5/inquire_names_for_mech.c \
	krb5/inquire_sec_context_by_oid.c \
	krb5/pname_to_uid.c \
	krb5/process_context_token.c \
	krb5/prf.c \
	krb5/release_buffer.c \
	krb5/release_cred.c \
	krb5/release_name.c \
	krb5/sequence.c \
	krb5/store_cred.c \
	krb5/set_cred_option.c \
	krb5/set_sec_context_option.c \
	krb5/ticket_flags.c \
	krb5/unwrap.c \
	krb5/verify_mic.c \
	krb5/wrap.c

mechsrc = \
	mech/context.c \
	mech/doxygen.c \
	mech/gss_accept_sec_context.c \
	mech/gss_acquire_cred.c \
	mech/gss_acquire_cred_ext.c \
	mech/gss_acquire_cred_with_password.c \
	mech/gss_add_cred.c \
	mech/gss_add_oid_set_member.c \
	mech/gss_authorize_localname.c \
	mech/gss_aeap.c \
	mech/gss_buffer_set.c \
	mech/gss_canonicalize_name.c \
	mech/gss_compare_name.c \
	mech/gss_context_time.c \
	mech/gss_create_empty_oid_set.c \
	mech/gss_cred.c \
	mech/gss_decapsulate_token.c \
	mech/gss_delete_name_attribute.c \
	mech/gss_delete_sec_context.c \
	mech/gss_display_name.c \
	mech/gss_display_name_ext.c \
	mech/gss_display_status.c \
	mech/gss_duplicate_name.c \
	mech/gss_duplicate_oid.c \
	mech/gss_encapsulate_token.c \
	mech/gss_export_name.c \
	mech/gss_export_name_composite.c \
	mech/gss_export_sec_context.c \
	mech/gss_get_mic.c \
	mech/gss_get_name_attribute.c \
	mech/gss_import_name.c \
	mech/gss_import_sec_context.c \
	mech/gss_indicate_mechs.c \
	mech/gss_init_sec_context.c \
	mech/gss_inquire_context.c \
	mech/gss_inquire_cred.c \
	mech/gss_inquire_cred_by_mech.c \
	mech/gss_inquire_cred_by_oid.c \
	mech/gss_inquire_mechs_for_name.c \
	mech/gss_inquire_name.c \
	mech/gss_inquire_names_for_mech.c \
	mech/gss_krb5.c \
	mech/gss_mech_switch.c \
	mech/gss_mo.c \
	mech/gss_names.c \
	mech/gss_oid.c \
	mech/gss_oid_equal.c \
	mech/gss_oid_to_str.c \
	mech/gss_pname_to_uid.c \
	mech/gss_process_context_token.c \
	mech/gss_pseudo_random.c \
	mech/gss_release_buffer.c \
	mech/gss_release_cred.c \
	mech/gss_release_name.c \
	mech/gss_release_oid.c \
	mech/gss_release_oid_set.c \
	mech/gss_seal.c \
	mech/gss_set_cred_option.c \
	mech/gss_set_name_attribute.c \
	mech/gss_set_sec_context_option.c \
	mech/gss_sign.c \
	mech/gss_store_cred.c \
	mech/gss_test_oid_set_member.c \
	mech/gss_unseal.c \
	mech/gss_unwrap.c \
	mech/gss_utils.c \
	mech/gss_verify.c \
	mech/gss_verify_mic.c \
	mech/gss_wrap.c \
	mech/gss_wrap_size_limit.c \
	mech/gss_inquire_sec_context_by_oid.c

spnegosrc = \
	spnego/accept_sec_context.c \
	spnego/compat.c \
	spnego/context_stubs.c \
	spnego/cred_stubs.c \
	spnego/external.c \
	spnego/init_sec_context.c

KRB5SRC=	${krb5src:S@/@__@}
MECHSRC=	${mechsrc:S@/@__@}
SPNEGOSRC=	${spnegosrc:S@/@__@}


SRCS=	${KRB5SRC} \
	${MECHSRC} \
	${SPNEGOSRC} \
	gkrb5_err.c \
	gkrb5_err.h \
	${GEN:S/.x$/.c/:S/.hx$/.h/}

INCS=		gssapi.h

OIDINCS=	gssapi_oid.h
OIDINCSDIR=	${INCLUDEDIR}/gssapi

KRB5INCS=	gssapi.h gssapi_krb5.h
KRB5INCSDIR=	${INCLUDEDIR}/gssapi

SPNEGOINCS=	gssapi_spnego.h
SPNEGOINCSDIR=	${INCLUDEDIR}/gssapi

INCSGROUPS=	INCS OIDINCS KRB5INCS SPNEGOINCS

GEN+=		gssapi_asn1.hx \
		gssapi_asn1-priv.hx \
		spnego_asn1.hx \
		spnego_asn1-priv.hx

MPAGES=		gssapi.3 \
		gss_acquire_cred.3 \
		mech.5

MSRC=		${KRB5DIR}/lib/gssapi \
		${KRB5DIR}/lib/gssapi/mech

MLINKS= \
		gss_acquire_cred.3	gss_accept_sec_context.3	\
		gss_acquire_cred.3	gss_add_cred.3			\
		gss_acquire_cred.3	gss_add_oid_set_member.3	\
		gss_acquire_cred.3	gss_canonicalize_name.3		\
		gss_acquire_cred.3	gss_compare_name.3		\
		gss_acquire_cred.3	gss_context_time.3		\
		gss_acquire_cred.3	gss_create_empty_oid_set.3	\
		gss_acquire_cred.3	gss_delete_sec_context.3	\
		gss_acquire_cred.3	gss_display_name.3		\
		gss_acquire_cred.3	gss_display_status.3		\
		gss_acquire_cred.3	gss_duplicate_name.3		\
		gss_acquire_cred.3	gss_export_name.3		\
		gss_acquire_cred.3	gss_export_sec_context.3	\
		gss_acquire_cred.3	gss_get_mic.3			\
		gss_acquire_cred.3	gss_import_name.3		\
		gss_acquire_cred.3	gss_import_sec_context.3	\
		gss_acquire_cred.3	gss_indicate_mechs.3		\
		gss_acquire_cred.3	gss_init_sec_context.3		\
		gss_acquire_cred.3	gss_inquire_context.3		\
		gss_acquire_cred.3	gss_inquire_cred.3		\
		gss_acquire_cred.3	gss_inquire_cred_by_mech.3	\
		gss_acquire_cred.3	gss_inquire_mechs_for_name.3	\
		gss_acquire_cred.3	gss_inquire_names_for_mech.3	\
		gss_acquire_cred.3	gss_krb5_ccache_name.3		\
		gss_acquire_cred.3	gss_krb5_compat_des3_mic.3	\
		gss_acquire_cred.3	gss_krb5_copy_ccache.3		\
		gss_acquire_cred.3	gss_krb5_get_tkt_flags.3	\
		gss_acquire_cred.3	gss_krb5_import_ccache.3	\
		gss_acquire_cred.3	gss_krb5_import_cred.3		\
		gss_acquire_cred.3	gss_process_context_token.3	\
		gss_acquire_cred.3	gss_release_buffer.3		\
		gss_acquire_cred.3	gss_release_cred.3		\
		gss_acquire_cred.3	gss_release_name.3		\
		gss_acquire_cred.3	gss_release_oid_set.3		\
		gss_acquire_cred.3	gss_seal.3			\
		gss_acquire_cred.3	gss_sign.3			\
		gss_acquire_cred.3	gss_test_oid_set_member.3	\
		gss_acquire_cred.3	gss_unseal.3			\
		gss_acquire_cred.3	gss_unwrap.3			\
		gss_acquire_cred.3	gss_verify.3			\
		gss_acquire_cred.3	gss_verify_mic.3		\
		gss_acquire_cred.3	gss_wrap.3			\
		gss_acquire_cred.3	gss_wrap_size_limit.3		\
		gss_acquire_cred.3	gsskrb5_extract_authz_data_from_sec_context.3 \
		gss_acquire_cred.3	gsskrb5_register_acceptor_identity.3

CPPFLAGS=	-I${KRB5DIR}/lib/gssapi \
		-I${KRB5DIR}/lib/gssapi/gssapi \
		-I${KRB5DIR}/lib/gssapi/krb5 \
		-I${KRB5DIR}/lib/gssapi/mech \
		-I${KRB5DIR}/lib/gssapi/spnego \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken -I.

CLEANFILES=	${GEN} ${GEN:S/.x$/.c/:S/.hx$/.h/} gssapi_asn1_files spnego_asn1_files \
		gssapi_asn1-template.c spnego_asn1-template.c

.for i in ${KRB5SRC} ${MECHSRC} ${SPNEGOSRC}
CLEANFILES+=$i
SRCS+=$i

${i}: ${i:S@__@/@}
	ln -s ${.ALLSRC} ${.OBJDIR}/${i}
.endfor

${GEN_GSSAPI}: gssapi.asn1
	${ASN1_COMPILE} ${.ALLSRC:M*.asn1} gssapi_asn1

${GEN_SPNEGO}: spnego.asn1 spnego.opt
	${ASN1_COMPILE} --option-file=${.ALLSRC:M*.opt} ${.ALLSRC:M*.asn1} spnego_asn1

.SUFFIXES: .h .c .x .hx

.x.c:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.hx.h:
	cmp -s ${.IMPSRC} ${.TARGET} 2> /dev/null || cp ${.IMPSRC} ${.TARGET}

.include <bsd.lib.mk>

.SUFFIXES: .h .c .x .hx

.PATH:	${KRB5DIR}/lib/gssapi/gssapi ${KRB5DIR}/lib/gssapi ${KRB5DIR}/lib/gssapi/krb5 \
	${KRB5DIR}/lib/gssapi/mech ${KRB5DIR}/lib/gssapi/spnego
