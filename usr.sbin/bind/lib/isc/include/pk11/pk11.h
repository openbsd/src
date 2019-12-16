/*
 * Copyright (C) 2014, 2016  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef PK11_PK11_H
#define PK11_PK11_H 1

/*! \file pk11/pk11.h */

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/types.h>

#define PK11_FATALCHECK(func, args) \
	((void) (((rv = (func) args) == CKR_OK) || \
		 ((pk11_error_fatalcheck)(__FILE__, __LINE__, #func, rv), 0)))

#include <pkcs11/cryptoki.h>
#include <pk11/site.h>

ISC_LANG_BEGINDECLS

#define SES_MAGIC	ISC_MAGIC('P','K','S','S')
#define TOK_MAGIC	ISC_MAGIC('P','K','T','K')

#define VALID_SES(x)	ISC_MAGIC_VALID(x, SES_MAGIC)
#define VALID_TOK(x)	ISC_MAGIC_VALID(x, TOK_MAGIC)

typedef struct pk11_context pk11_context_t;

struct pk11_object {
	CK_OBJECT_HANDLE	object;
	CK_SLOT_ID		slot;
	CK_BBOOL		ontoken;
	CK_BBOOL		reqlogon;
	CK_BYTE			attrcnt;
	CK_ATTRIBUTE		*repr;
};

struct pk11_context {
	void			*handle;
	CK_SESSION_HANDLE	session;
	CK_BBOOL		ontoken;
	CK_OBJECT_HANDLE	object;
#if defined(PK11_MD5_HMAC_REPLACE) ||  defined(PK11_SHA_1_HMAC_REPLACE) || \
    defined(PK11_SHA224_HMAC_REPLACE) || defined(PK11_SHA256_HMAC_REPLACE) || \
    defined(PK11_SHA384_HMAC_REPLACE) || defined(PK11_SHA512_HMAC_REPLACE)
	unsigned char		*key;
#endif
};

typedef struct pk11_object pk11_object_t;

typedef enum {
	OP_ANY = 0,
	OP_RAND = 1,
	OP_RSA = 2,
	OP_DSA = 3,
	OP_DH = 4,
	OP_DIGEST = 5,
	OP_EC = 6,
	OP_GOST = 7,
	OP_AES = 8,
	OP_MAX = 9
} pk11_optype_t;

/*%
 * Global flag to make choose_slots() verbose
 */
LIBISC_EXTERNAL_DATA extern isc_boolean_t pk11_verbose_init;

/*%
 * Function prototypes
 */

void pk11_set_lib_name(const char *lib_name);
/*%<
 * Set the PKCS#11 provider (aka library) path/name.
 */

isc_result_t pk11_initialize(isc_mem_t *mctx, const char *engine);
/*%<
 * Initialize PKCS#11 device
 *
 * mctx:   memory context to attach to pk11_mctx.
 * engine: PKCS#11 provider (aka library) path/name.
 *
 * returns:
 *         ISC_R_SUCCESS
 *         PK11_R_NOPROVIDER: can't load the provider
 *         PK11_R_INITFAILED: C_Initialize() failed
 *         PK11_R_NORANDOMSERVICE: can't find required random service
 *         PK11_R_NODIGESTSERVICE: can't find required digest service
 *         PK11_R_NOAESSERVICE: can't find required AES service
 */

isc_result_t pk11_get_session(pk11_context_t *ctx,
			      pk11_optype_t optype,
			      isc_boolean_t need_services,
			      isc_boolean_t rw,
			      isc_boolean_t logon,
			      const char *pin,
			      CK_SLOT_ID slot);
/*%<
 * Initialize PKCS#11 device and acquire a session.
 *
 * need_services:
 * 	  if ISC_TRUE, this session requires full PKCS#11 API
 * 	  support including random and digest services, and
 * 	  the lack of these services will cause the session not
 * 	  to be initialized.  If ISC_FALSE, the function will return
 * 	  an error code indicating the missing service, but the
 * 	  session will be usable for other purposes.
 * rw:    if ISC_TRUE, session will be read/write (useful for
 *        generating or destroying keys); otherwise read-only.
 * login: indicates whether to log in to the device
 * pin:   optional PIN, overriding any PIN currently associated
 *        with the
 * slot:  device slot ID
 */

void pk11_return_session(pk11_context_t *ctx);
/*%<
 * Release an active PKCS#11 session for reuse.
 */

isc_result_t pk11_finalize(void);
/*%<
 * Shut down PKCS#11 device and free all sessions.
 */

isc_result_t pk11_rand_bytes(unsigned char *buf, int num);

void pk11_rand_seed_fromfile(const char *randomfile);

isc_result_t pk11_parse_uri(pk11_object_t *obj, const char *label,
			    isc_mem_t *mctx, pk11_optype_t optype);

ISC_PLATFORM_NORETURN_PRE void
pk11_error_fatalcheck(const char *file, int line,
		      const char *funcname, CK_RV rv)
ISC_PLATFORM_NORETURN_POST;

void pk11_dump_tokens(void);

CK_RV
pkcs_C_Initialize(CK_VOID_PTR pReserved);

char *pk11_get_load_error_message(void);

CK_RV
pkcs_C_Finalize(CK_VOID_PTR pReserved);

CK_RV
pkcs_C_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList,
		   CK_ULONG_PTR pulCount);

CK_RV
pkcs_C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo);

CK_RV
pkcs_C_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type,
			CK_MECHANISM_INFO_PTR pInfo);

CK_RV
pkcs_C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
		   CK_VOID_PTR pApplication,
		   CK_RV  (*Notify) (CK_SESSION_HANDLE hSession,
				     CK_NOTIFICATION event,
				     CK_VOID_PTR pApplication),
		   CK_SESSION_HANDLE_PTR phSession);

CK_RV
pkcs_C_CloseSession(CK_SESSION_HANDLE hSession);

CK_RV
pkcs_C_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
	     CK_CHAR_PTR pPin, CK_ULONG usPinLen);

CK_RV
pkcs_C_Logout(CK_SESSION_HANDLE hSession);

CK_RV
pkcs_C_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount, CK_OBJECT_HANDLE_PTR phObject);

CK_RV
pkcs_C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject);

CK_RV
pkcs_C_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
			 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG usCount);

CK_RV
pkcs_C_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
			 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG usCount);

CK_RV
pkcs_C_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
		       CK_ULONG usCount);

CK_RV
pkcs_C_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
		   CK_ULONG usMaxObjectCount, CK_ULONG_PTR pusObjectCount);

CK_RV
pkcs_C_FindObjectsFinal(CK_SESSION_HANDLE hSession);

CK_RV
pkcs_C_EncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		   CK_OBJECT_HANDLE hKey);

CK_RV
pkcs_C_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	       CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
	       CK_ULONG_PTR pulEncryptedDataLen);

CK_RV
pkcs_C_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism);

CK_RV
pkcs_C_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		    CK_ULONG ulPartLen);

CK_RV
pkcs_C_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
		   CK_ULONG_PTR pulDigestLen);

CK_RV
pkcs_C_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		CK_OBJECT_HANDLE hKey);

CK_RV
pkcs_C_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	    CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
	    CK_ULONG_PTR pulSignatureLen);

CK_RV
pkcs_C_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		  CK_ULONG ulPartLen);

CK_RV
pkcs_C_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
		 CK_ULONG_PTR pulSignatureLen);

CK_RV
pkcs_C_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		  CK_OBJECT_HANDLE hKey);

CK_RV
pkcs_C_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	      CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
	      CK_ULONG ulSignatureLen);

CK_RV
pkcs_C_VerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		    CK_ULONG ulPartLen);

CK_RV
pkcs_C_VerifyFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
		   CK_ULONG ulSignatureLen);

CK_RV
pkcs_C_GenerateKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		   CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
		   CK_OBJECT_HANDLE_PTR phKey);

CK_RV
pkcs_C_GenerateKeyPair(CK_SESSION_HANDLE hSession,
		       CK_MECHANISM_PTR pMechanism,
		       CK_ATTRIBUTE_PTR pPublicKeyTemplate,
		       CK_ULONG usPublicKeyAttributeCount,
		       CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
		       CK_ULONG usPrivateKeyAttributeCount,
		       CK_OBJECT_HANDLE_PTR phPrivateKey,
		       CK_OBJECT_HANDLE_PTR phPublicKey);

CK_RV
pkcs_C_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		 CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR pTemplate,
		 CK_ULONG ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey);

CK_RV
pkcs_C_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed,
		  CK_ULONG ulSeedLen);

CK_RV
pkcs_C_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR RandomData,
		      CK_ULONG ulRandomLen);

ISC_LANG_ENDDECLS

#endif /* PK11_PK11_H */
