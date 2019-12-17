/*
 * Copyright (C) Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: pk11_api.c,v 1.2 2019/12/17 01:46:37 sthen Exp $ */

/*! \file */

#include <config.h>

#include <string.h>
#include <dlfcn.h>

#include <isc/log.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/print.h>
#include <isc/stdio.h>
#include <isc/thread.h>
#include <isc/util.h>

#include <pkcs11/cryptoki.h>
#include <pkcs11/pkcs11.h>

#define KEEP_PKCS11_NAMES
#include <pk11/pk11.h>
#include <pk11/internal.h>

static void *hPK11 = NULL;
static char loaderrmsg[1024];

CK_RV
pkcs_C_Initialize(CK_VOID_PTR pReserved) {
	CK_C_Initialize sym;

	if (hPK11 != NULL)
		return (CKR_LIBRARY_ALREADY_INITIALIZED);

	hPK11 = dlopen(pk11_get_lib_name(), RTLD_NOW);

	if (hPK11 == NULL) {
		snprintf(loaderrmsg, sizeof(loaderrmsg),
			 "dlopen(\"%s\") failed: %s\n",
			 pk11_get_lib_name(), dlerror());
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	}
	sym = (CK_C_Initialize)dlsym(hPK11, "C_Initialize");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(pReserved);
}

char *pk11_get_load_error_message(void) {
	return (loaderrmsg);
}

CK_RV
pkcs_C_Finalize(CK_VOID_PTR pReserved) {
	CK_C_Finalize sym;
	CK_RV rv;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	sym = (CK_C_Finalize)dlsym(hPK11, "C_Finalize");
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	rv = (*sym)(pReserved);
	if ((rv == CKR_OK) && (dlclose(hPK11) != 0))
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	hPK11 = NULL;
	return (rv);
}

CK_RV
pkcs_C_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList,
		   CK_ULONG_PTR pulCount)
{
	static CK_C_GetSlotList sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GetSlotList)dlsym(hPK11, "C_GetSlotList");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(tokenPresent, pSlotList, pulCount);
}

CK_RV
pkcs_C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo) {
	static CK_C_GetTokenInfo sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GetTokenInfo)dlsym(hPK11, "C_GetTokenInfo");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, pInfo);
}

CK_RV
pkcs_C_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type,
			CK_MECHANISM_INFO_PTR pInfo)
{
	static CK_C_GetMechanismInfo sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GetMechanismInfo)dlsym(hPK11,
						   "C_GetMechanismInfo");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, type, pInfo);
}

CK_RV
pkcs_C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags,
		   CK_VOID_PTR pApplication,
		   CK_RV  (*Notify) (CK_SESSION_HANDLE hSession,
				     CK_NOTIFICATION event,
				     CK_VOID_PTR pApplication),
		   CK_SESSION_HANDLE_PTR phSession)
{
	static CK_C_OpenSession sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		hPK11 = dlopen(pk11_get_lib_name(), RTLD_NOW);
	if (hPK11 == NULL) {
		snprintf(loaderrmsg, sizeof(loaderrmsg),
			 "dlopen(\"%s\") failed: %s\n",
			 pk11_get_lib_name(), dlerror());
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	}
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_OpenSession)dlsym(hPK11, "C_OpenSession");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(slotID, flags, pApplication, Notify, phSession);
}

CK_RV
pkcs_C_CloseSession(CK_SESSION_HANDLE hSession) {
	static CK_C_CloseSession sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_CloseSession)dlsym(hPK11, "C_CloseSession");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
	     CK_CHAR_PTR pPin, CK_ULONG usPinLen)
{
	static CK_C_Login sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_Login)dlsym(hPK11, "C_Login");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, userType, pPin, usPinLen);
}

CK_RV
pkcs_C_Logout(CK_SESSION_HANDLE hSession) {
	static CK_C_Logout sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_Logout)dlsym(hPK11, "C_Logout");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
		    CK_ULONG usCount, CK_OBJECT_HANDLE_PTR phObject)
{
	static CK_C_CreateObject sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_CreateObject)dlsym(hPK11, "C_CreateObject");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pTemplate, usCount, phObject);
}

CK_RV
pkcs_C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject) {
	static CK_C_DestroyObject sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_DestroyObject)dlsym(hPK11, "C_DestroyObject");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject);
}

CK_RV
pkcs_C_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
			 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG usCount)
{
	static CK_C_GetAttributeValue sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GetAttributeValue)dlsym(hPK11,
						    "C_GetAttributeValue");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

CK_RV
pkcs_C_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject,
			 CK_ATTRIBUTE_PTR pTemplate, CK_ULONG usCount)
{
	static CK_C_SetAttributeValue sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_SetAttributeValue)dlsym(hPK11,
						    "C_SetAttributeValue");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, hObject, pTemplate, usCount);
}

CK_RV
pkcs_C_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate,
		       CK_ULONG usCount)
{
	static CK_C_FindObjectsInit sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_FindObjectsInit)dlsym(hPK11, "C_FindObjectsInit");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pTemplate, usCount);
}

CK_RV
pkcs_C_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject,
		   CK_ULONG usMaxObjectCount, CK_ULONG_PTR pusObjectCount)
{
	static CK_C_FindObjects sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_FindObjects)dlsym(hPK11, "C_FindObjects");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, phObject, usMaxObjectCount, pusObjectCount);
}

CK_RV
pkcs_C_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
	static CK_C_FindObjectsFinal sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_FindObjectsFinal)dlsym(hPK11,
						   "C_FindObjectsFinal");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession);
}

CK_RV
pkcs_C_EncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		   CK_OBJECT_HANDLE hKey)
{
	static CK_C_EncryptInit sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_EncryptInit)dlsym(hPK11, "C_EncryptInit");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	       CK_ULONG ulDataLen, CK_BYTE_PTR pEncryptedData,
	       CK_ULONG_PTR pulEncryptedDataLen)
{
	static CK_C_Encrypt sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_Encrypt)dlsym(hPK11, "C_Encrypt");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen,
		      pEncryptedData, pulEncryptedDataLen);
}

CK_RV
pkcs_C_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism) {
	static CK_C_DigestInit sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_DigestInit)dlsym(hPK11, "C_DigestInit");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism);
}

CK_RV
pkcs_C_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		    CK_ULONG ulPartLen)
{
	static CK_C_DigestUpdate sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_DigestUpdate)dlsym(hPK11, "C_DigestUpdate");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pDigest,
		   CK_ULONG_PTR pulDigestLen)
{
	static CK_C_DigestFinal sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_DigestFinal)dlsym(hPK11, "C_DigestFinal");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pDigest, pulDigestLen);
}

CK_RV
pkcs_C_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		CK_OBJECT_HANDLE hKey)
{
	static CK_C_SignInit sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_SignInit)dlsym(hPK11, "C_SignInit");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	    CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
	    CK_ULONG_PTR pulSignatureLen)
{
	static CK_C_Sign sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_Sign)dlsym(hPK11, "C_Sign");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen, pSignature, pulSignatureLen);
}

CK_RV
pkcs_C_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		  CK_ULONG ulPartLen)
{
	static CK_C_SignUpdate sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_SignUpdate)dlsym(hPK11, "C_SignUpdate");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
		 CK_ULONG_PTR pulSignatureLen)
{
	static CK_C_SignFinal sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_SignFinal)dlsym(hPK11, "C_SignFinal");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSignature, pulSignatureLen);
}

CK_RV
pkcs_C_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		  CK_OBJECT_HANDLE hKey)
{
	static CK_C_VerifyInit sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_VerifyInit)dlsym(hPK11, "C_VerifyInit");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, hKey);
}

CK_RV
pkcs_C_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
	      CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
	      CK_ULONG ulSignatureLen)
{
	static CK_C_Verify sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_Verify)dlsym(hPK11, "C_Verify");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pData, ulDataLen, pSignature, ulSignatureLen);
}

CK_RV
pkcs_C_VerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pPart,
		    CK_ULONG ulPartLen)
{
	static CK_C_VerifyUpdate sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_VerifyUpdate)dlsym(hPK11, "C_VerifyUpdate");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pPart, ulPartLen);
}

CK_RV
pkcs_C_VerifyFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSignature,
		   CK_ULONG ulSignatureLen)
{
	static CK_C_VerifyFinal sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_VerifyFinal)dlsym(hPK11, "C_VerifyFinal");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSignature, ulSignatureLen);
}

CK_RV
pkcs_C_GenerateKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		   CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount,
		   CK_OBJECT_HANDLE_PTR phKey)
{
	static CK_C_GenerateKey sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GenerateKey)dlsym(hPK11, "C_GenerateKey");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pMechanism, pTemplate, ulCount, phKey);
}

CK_RV
pkcs_C_GenerateKeyPair(CK_SESSION_HANDLE hSession,
		       CK_MECHANISM_PTR pMechanism,
		       CK_ATTRIBUTE_PTR pPublicKeyTemplate,
		       CK_ULONG usPublicKeyAttributeCount,
		       CK_ATTRIBUTE_PTR pPrivateKeyTemplate,
		       CK_ULONG usPrivateKeyAttributeCount,
		       CK_OBJECT_HANDLE_PTR phPrivateKey,
		       CK_OBJECT_HANDLE_PTR phPublicKey)
{
	static CK_C_GenerateKeyPair sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GenerateKeyPair)dlsym(hPK11, "C_GenerateKeyPair");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession,
		      pMechanism,
		      pPublicKeyTemplate,
		      usPublicKeyAttributeCount,
		      pPrivateKeyTemplate,
		      usPrivateKeyAttributeCount,
		      phPrivateKey,
		      phPublicKey);
}

CK_RV
pkcs_C_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
		 CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR pTemplate,
		 CK_ULONG ulAttributeCount, CK_OBJECT_HANDLE_PTR phKey)
{
	static CK_C_DeriveKey sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_DeriveKey)dlsym(hPK11, "C_DeriveKey");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession,
		      pMechanism,
		      hBaseKey,
		      pTemplate,
		      ulAttributeCount,
		      phKey);
}

CK_RV
pkcs_C_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pSeed,
		  CK_ULONG ulSeedLen)
{
	static CK_C_SeedRandom sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_SeedRandom)dlsym(hPK11, "C_SeedRandom");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, pSeed, ulSeedLen);
}

CK_RV
pkcs_C_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR RandomData,
		      CK_ULONG ulRandomLen)
{
	static CK_C_GenerateRandom sym = NULL;
	static void *pPK11 = NULL;

	if (hPK11 == NULL)
		return (CKR_LIBRARY_FAILED_TO_LOAD);
	if ((sym == NULL) || (hPK11 != pPK11)) {
		pPK11 = hPK11;
		sym = (CK_C_GenerateRandom)dlsym(hPK11, "C_GenerateRandom");
	}
	if (sym == NULL)
		return (CKR_SYMBOL_RESOLUTION_FAILED);
	return (*sym)(hSession, RandomData, ulRandomLen);
}
