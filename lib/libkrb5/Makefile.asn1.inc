# $OpenBSD: Makefile.asn1.inc,v 1.7 2006/04/14 08:24:41 biorn Exp $

DIR_ASN1=	${KRB5SRC}/lib/asn1
HDR_ASN1=	der.h asn1-common.h
HDR_GEN+=	krb5_asn1.h asn1_err.h

.PATH:	${DIR_ASN1}

gen_files_asn1= \
	krb5_asn1.h \
	asn1_AD_AND_OR.c		\
	asn1_AD_IF_RELEVANT.c		\
	asn1_AD_KDCIssued.c		\
	asn1_AD_MANDATORY_FOR_KDC.c	\
	asn1_APOptions.c                \
	asn1_AP_REP.c                   \
	asn1_AP_REQ.c                   \
	asn1_AS_REP.c                   \
	asn1_AS_REQ.c                   \
	asn1_AUTHDATA_TYPE.c		\
	asn1_Authenticator.c            \
	asn1_AuthorizationData.c        \
	asn1_CBCParameter.c		\
	asn1_CKSUMTYPE.c		\
	asn1_ChangePasswdDataMS.c	\
	asn1_Checksum.c                 \
	asn1_ETYPE_INFO.c		\
	asn1_ETYPE_INFO2.c		\
	asn1_ETYPE_INFO_ENTRY.c		\
	asn1_ETYPE_INFO2_ENTRY.c	\
	asn1_ENCTYPE.c			\
	asn1_EncAPRepPart.c             \
	asn1_EncASRepPart.c             \
	asn1_EncKDCRepPart.c            \
	asn1_EncKrbCredPart.c           \
	asn1_EncKrbPrivPart.c           \
	asn1_EncTGSRepPart.c            \
	asn1_EncTicketPart.c            \
	asn1_EncryptedData.c            \
	asn1_EncryptionKey.c            \
	asn1_EtypeList.c		\
	asn1_HostAddress.c              \
	asn1_HostAddresses.c            \
	asn1_KDCOptions.c               \
	asn1_KDC_REP.c                  \
	asn1_KDC_REQ.c                  \
	asn1_KDC_REQ_BODY.c             \
	asn1_KRB_CRED.c                 \
	asn1_KRB_ERROR.c                \
	asn1_KRB_PRIV.c                 \
	asn1_KRB_SAFE.c                 \
	asn1_KRB_SAFE_BODY.c            \
	asn1_KerberosString.c		\
	asn1_KerberosTime.c             \
	asn1_KrbCredInfo.c              \
	asn1_LastReq.c                  \
	asn1_LR_TYPE.c			\
	asn1_MESSAGE_TYPE.c		\
	asn1_METHOD_DATA.c              \
	asn1_NAME_TYPE.c		\
	asn1_PADATA_TYPE.c		\
	asn1_PA_DATA.c                  \
	asn1_PA_ENC_SAM_RESPONSE_ENC.c	\
	asn1_PA_ENC_TS_ENC.c            \
	asn1_PA_PAC_REQUEST.c		\
	asn1_PA_SAM_CHALLENGE_2.c	\
	asn1_PA_SAM_CHALLENGE_2_BODY.c	\
	asn1_PA_SAM_REDIRECT.c		\
	asn1_PA_SAM_RESPONSE_2.c	\
	asn1_PA_SAM_TYPE.c		\
	asn1_Principal.c                \
	asn1_PrincipalName.c            \
	asn1_RC2CBCParameter.c		\
	asn1_Realm.c                    \
	asn1_SAMFlags.c			\
	asn1_TGS_REP.c                  \
	asn1_TGS_REQ.c                  \
	asn1_Ticket.c                   \
	asn1_TicketFlags.c              \
	asn1_TransitedEncoding.c	\
	asn1_UNSIGNED.c


gen_files_asn1+=\
	asn1_err.h \
	asn1_err.c

SRCS+=	der_cmp.c \
	der_get.c \
	der_put.c \
	der_free.c \
	der_length.c \
	der_copy.c \
	${gen_files_asn1}


CFLAGS+=	-I${DIR_ASN1}

regen_asn1:
	cd ${GENDIR}; compile_et ${DIR_ASN1}/asn1_err.et
	cd ${GENDIR}; asn1_compile ${DIR_ASN1}/k5.asn1 krb5_asn1; \
		for f in *.x; do mv $$f `echo $$f | sed s,.x$$,.c,g`; done

regen: regen_asn1
