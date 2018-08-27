/* $OpenBSD: wycheproof.go,v 1.28 2018/08/27 21:27:39 tb Exp $ */
/*
 * Copyright (c) 2018 Joel Sing <jsing@openbsd.org>
 * Copyright (c) 2018 Theo Buehler <tb@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

// Wycheproof runs test vectors from Project Wycheproof against libcrypto.
package main

/*
#cgo LDFLAGS: -lcrypto

#include <string.h>

#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/curve25519.h>
#include <openssl/dsa.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/rsa.h>
*/
import "C"

import (
	"bytes"
	"crypto/sha1"
	"crypto/sha256"
	"crypto/sha512"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"hash"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"unsafe"
)

const testVectorPath = "/usr/local/share/wycheproof/testvectors"

type wycheproofTestGroupAesCbcPkcs5 struct {
	IVSize  int                          `json:"ivSize"`
	KeySize int                          `json:"keySize"`
	Type    string                       `json:"type"`
	Tests   []*wycheproofTestAesCbcPkcs5 `json:"tests"`
}

type wycheproofTestAesCbcPkcs5 struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Key     string   `json:"key"`
	IV      string   `json:"iv"`
	Msg     string   `json:"msg"`
	CT      string   `json:"ct"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupAesCcm struct {
	IVSize  int                     `json:"ivSize"`
	KeySize int                     `json:"keySize"`
	TagSize int                     `json:"tagSize"`
	Type    string                  `json:"type"`
	Tests   []*wycheproofTestAesCcm `json:"tests"`
}

type wycheproofTestAesCcm struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Key     string   `json:"key"`
	IV      string   `json:"iv"`
	AAD     string   `json:"aad"`
	Msg     string   `json:"msg"`
	CT      string   `json:"ct"`
	Tag     string   `json:"tag"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupChaCha20Poly1305 struct {
	IVSize  int                               `json:"ivSize"`
	KeySize int                               `json:"keySize"`
	TagSize int                               `json:"tagSize"`
	Type    string                            `json:"type"`
	Tests   []*wycheproofTestChaCha20Poly1305 `json:"tests"`
}

type wycheproofTestChaCha20Poly1305 struct {
	TCID	int      `json:"tcId"`
	Comment string   `json:"comment"`
	Key     string   `json:"key"`
	IV      string   `json:"iv"`
	AAD     string   `json:"aad"`
	Msg     string   `json:"msg"`
	CT      string   `json:"ct"`
	Tag     string   `json:"tag"`
	Result	string	 `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofDSAKey struct {
	G       string `json:"g"`
	KeySize int    `json:"keySize"`
	P       string `json:"p"` 
	Q       string `json:"q"`
	Type    string `json:"type"`
	Y       string `json:"y"` 
}

type wycheproofTestDSA struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Msg     string   `json:"msg"`
	Sig     string   `json:"sig"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupDSA struct {
	Key    *wycheproofDSAKey    `json:"key"`
	KeyDER string               `json:"keyDer"`
	KeyPEM string               `json:"keyPem"`
	SHA    string               `json:"sha"`
	Type   string               `json:"type"`
	Tests  []*wycheproofTestDSA `json:"tests"`
}

type wycheproofECDSAKey struct {
	Curve        string `json:"curve"`
	KeySize      int    `json:"keySize"`
	Type         string `json:"type"`
	Uncompressed string `json:"uncompressed"`
	WX           string `json:"wx"`
	WY           string `json:"wy"`
}

type wycheproofTestECDSA struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Msg     string   `json:"msg"`
	Sig     string   `json:"sig"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupECDSA struct {
	Key    *wycheproofECDSAKey    `json:"key"`
	KeyDER string                 `json:"keyDer"`
	KeyPEM string                 `json:"keyPem"`
	SHA    string                 `json:"sha"`
	Type   string                 `json:"type"`
	Tests  []*wycheproofTestECDSA `json:"tests"`
}

type wycheproofTestRSA struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Msg     string   `json:"msg"`
	Sig     string   `json:"sig"`
	Padding string   `json:"padding"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupRSA struct {
	E       string               `json:"e"`
	KeyASN  string               `json:"keyAsn"`
	KeyDER  string               `json:"keyDer"`
	KeyPEM  string               `json:"keyPem"`
	KeySize int                  `json:"keysize"`
	N       string               `json:"n"`
	SHA     string               `json:"sha"`
	Type    string               `json:"type"`
	Tests   []*wycheproofTestRSA `json:"tests"`
}

type wycheproofTestX25519 struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Curve   string   `json:"curve"`
	Public  string   `json:"public"`
	Private string   `json:"private"`
	Shared  string   `json:"shared"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupX25519 struct {
	Curve string                  `json:"curve"`
	Tests []*wycheproofTestX25519 `json:"tests"`
}

type wycheproofTestVectors struct {
	Algorithm        string            `json:"algorithm"`
	GeneratorVersion string            `json:"generatorVersion"`
	Notes            map[string]string `json:"notes"`
	NumberOfTests    int               `json:"numberOfTests"`
	// Header
	TestGroups []json.RawMessage `json:"testGroups"`
}

var nids = map[string]int{
	"brainpoolP224r1": C.NID_brainpoolP224r1,
	"brainpoolP256r1": C.NID_brainpoolP256r1,
	"brainpoolP320r1": C.NID_brainpoolP320r1,
	"brainpoolP384r1": C.NID_brainpoolP384r1,
	"brainpoolP512r1": C.NID_brainpoolP512r1,
	"brainpoolP224t1": C.NID_brainpoolP224t1,
	"brainpoolP256t1": C.NID_brainpoolP256t1,
	"brainpoolP320t1": C.NID_brainpoolP320t1,
	"brainpoolP384t1": C.NID_brainpoolP384t1,
	"brainpoolP512t1": C.NID_brainpoolP512t1,
	"secp224r1":       C.NID_secp224r1,
	"secp256k1":       C.NID_secp256k1,
	"secp384r1":       C.NID_secp384r1,
	"secp521r1":       C.NID_secp521r1,
	"SHA-1":           C.NID_sha1,
	"SHA-224":         C.NID_sha224,
	"SHA-256":         C.NID_sha256,
	"SHA-384":         C.NID_sha384,
	"SHA-512":         C.NID_sha512,
}

func nidFromString(ns string) (int, error) {
	nid, ok := nids[ns]
	if ok {
		return nid, nil
	}
	return -1, fmt.Errorf("unknown NID %q", ns)
}

func hashFromString(hs string) (hash.Hash, error) {
	switch hs {
	case "SHA-1":
		return sha1.New(), nil
	case "SHA-224":
		return sha256.New224(), nil
	case "SHA-256":
		return sha256.New(), nil
	case "SHA-384":
		return sha512.New384(), nil
	case "SHA-512":
		return sha512.New(), nil
	default:
		return nil, fmt.Errorf("unknown hash %q", hs)
	}
}

func checkAesCbcPkcs5(ctx *C.EVP_CIPHER_CTX, doEncrypt int, key []byte, keyLen int, iv []byte, ivLen int, in []byte, inLen int, out []byte, outLen int, wt *wycheproofTestAesCbcPkcs5) bool {
	var action string
	if doEncrypt == 1 {
		action = "encrypting"
	} else {
		action = "decrypting"
	}

	ret := C.EVP_CipherInit_ex(ctx, nil, nil, (*C.uchar)(unsafe.Pointer(&key[0])), (*C.uchar)(unsafe.Pointer(&iv[0])), C.int(doEncrypt))
	if ret != 1 {
		log.Fatalf("EVP_CipherInit_ex failed: %d", ret)
	}

	cipherOut := make([]byte, inLen + C.EVP_MAX_BLOCK_LENGTH)
	var cipherOutLen C.int

	ret = C.EVP_CipherUpdate(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[0])), &cipherOutLen, (*C.uchar)(unsafe.Pointer(&in[0])), C.int(inLen))
	if ret != 1 {
		if wt.Result == "invalid" {
			fmt.Printf("INFO: Test case %d (%q) [%v] - EVP_CipherUpdate() = %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] - EVP_CipherUpdate() = %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
		return false
	}

	var finallen C.int
	ret = C.EVP_CipherFinal_ex(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[cipherOutLen])), &finallen)
	if ret != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] - EVP_CipherFinal_ex() = %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
		return false
	}

	cipherOutLen += finallen
	if cipherOutLen != C.int(outLen) && wt.Result != "invalid" {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - open length mismatch: got %d, want %d\n", wt.TCID, wt.Comment, action, cipherOutLen, outLen)
		return false
	}

	openedMsg := out[0:cipherOutLen]
	if outLen == 0 {
		out = nil
	}

	success := false
	if bytes.Equal(openedMsg, out) || wt.Result == "invalid" {
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - msg match: %t; want %v\n", wt.TCID, wt.Comment, action, bytes.Equal(openedMsg, out), wt.Result)
	}
	return success
}

func runAesCbcPkcs5Test(ctx *C.EVP_CIPHER_CTX, wt *wycheproofTestAesCbcPkcs5) bool {
	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}
	iv, err := hex.DecodeString(wt.IV)
	if err != nil {
		log.Fatalf("Failed to decode IV %q: %v", wt.IV, err)
	}
	ct, err := hex.DecodeString(wt.CT)
	if err != nil {
		log.Fatalf("Failed to decode CT %q: %v", wt.CT, err)
	}
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode message %q: %v", wt.Msg, err)
	}

	keyLen, ivLen, ctLen, msgLen := len(key), len(iv), len(ct), len(msg)
	
	if (keyLen == 0) {
		key = append(key, 0)
	}
	if (ivLen == 0) {
		iv = append(iv, 0)
	}
	if (ctLen == 0) {
		ct = append(ct, 0)
	}
	if (msgLen == 0) {
		msg = append(msg, 0)
	}

	openSuccess := checkAesCbcPkcs5(ctx, 0, key, keyLen, iv, ivLen, ct, ctLen, msg, msgLen, wt)
	sealSuccess := checkAesCbcPkcs5(ctx, 1, key, keyLen, iv, ivLen, msg, msgLen, ct, ctLen, wt)

	return openSuccess && sealSuccess
}

func runAesCbcPkcs5TestGroup(wtg *wycheproofTestGroupAesCbcPkcs5) bool {
	fmt.Printf("Running AES-CBC-PKCS5 test group %v with IV size %d and key size %d...\n", wtg.Type, wtg.IVSize, wtg.KeySize)

	var cipher *C.EVP_CIPHER
	switch wtg.KeySize {
	case 128:
		cipher = C.EVP_aes_128_cbc()
	case 192:
		cipher = C.EVP_aes_192_cbc()
	case 256:
		cipher = C.EVP_aes_256_cbc()
	default:
		log.Fatalf("Unsupported key size: %d", wtg.KeySize)
	}

	ctx := C.EVP_CIPHER_CTX_new()
	if ctx == nil {
		log.Fatal("EVP_CIPHER_CTX_new() failed")
	}
	defer C.EVP_CIPHER_CTX_free(ctx)

	ret := C.EVP_CipherInit_ex(ctx, cipher, nil, nil, nil, 0)
	if ret != 1 {
		log.Fatalf("EVP_CipherInit_ex failed: %d", ret)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runAesCbcPkcs5Test(ctx, wt) {
			success = false
		}
	}
	return success
}

func checkAesCcm(ctx *C.EVP_CIPHER_CTX, doEncrypt int, key []byte, keyLen int, iv []byte, ivLen int, aad []byte, aadLen int, in []byte, inLen int, out []byte, outLen int, tag []byte, tagLen int, wt *wycheproofTestAesCcm) bool {
	setTag := unsafe.Pointer(nil)
	var action string

	if doEncrypt == 1 {
		action = "encrypting"
	} else {
		action = "decrypting"
		setTag = unsafe.Pointer(&tag[0])
	}
	
	ret := C.EVP_CipherInit_ex(ctx, nil, nil, nil, nil, C.int(doEncrypt))
	if ret != 1 {
		log.Fatalf("[%v] cipher init failed", action)
	}

	ret = C.EVP_CIPHER_CTX_ctrl(ctx, C.EVP_CTRL_CCM_SET_IVLEN, C.int(ivLen), nil)
	if ret != 1 {
		if (wt.Comment == "Nonce is too long" || wt.Comment == "Invalid nonce size") {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] - setting IV len to %d failed. got %d, want %v\n", wt.TCID, wt.Comment, action, ivLen, ret, wt.Result)
		return false
	}

	ret = C.EVP_CIPHER_CTX_ctrl(ctx, C.EVP_CTRL_CCM_SET_TAG, C.int(tagLen), setTag)
	if ret != 1 {
		if (wt.Comment == "Invalid tag size") {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] - setting tag length to %d failed. got %d, want %v\n", wt.TCID, wt.Comment, action, tagLen, ret, wt.Result)
		return false
	}

	ret = C.EVP_CipherInit_ex(ctx, nil, nil, (*C.uchar)(unsafe.Pointer(&key[0])), (*C.uchar)(unsafe.Pointer(&iv[0])), C.int(doEncrypt))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - setting key and IV failed. got %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
		return false
	}

	var cipherOutLen C.int
	ret = C.EVP_CipherUpdate(ctx, nil, &cipherOutLen, nil, C.int(inLen))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - setting input length to %d failed. got %d, want %v\n", wt.TCID, wt.Comment, action, inLen, ret, wt.Result)
		return false
	}

	ret = C.EVP_CipherUpdate(ctx, nil, &cipherOutLen, (*C.uchar)(unsafe.Pointer(&aad[0])), C.int(aadLen))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - processing AAD failed. got %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
		return false
	}

	cipherOutLen = 0
	cipherOut := make([]byte, inLen)
	if inLen == 0 {
		cipherOut = append(cipherOut, 0)
	}

	ret = C.EVP_CipherUpdate(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[0])), &cipherOutLen, (*C.uchar)(unsafe.Pointer(&in[0])), C.int(inLen))
	if ret != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] - EVP_CipherUpdate failed: got %d, want %v\n", wt.TCID, wt.Comment, action, ret, wt.Result)
		return false
	}

	if cipherOutLen != C.int(outLen) {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - cipherOutLen %d != outLen %d. Result %v\n", wt.TCID, wt.Comment, cipherOutLen, action, outLen, wt.Result)
		return false
	}

	success := true
	if !bytes.Equal(cipherOut, out) {
		fmt.Printf("FAIL: Test case %d (%q) [%v] - expected and computed output do not match. Result.%v\n", wt.TCID, wt.Comment, action, wt.Result)
		success = false
	}
	return success
}

func runAesCcmTest(ctx *C.EVP_CIPHER_CTX, wt *wycheproofTestAesCcm) bool {
	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}

	iv, err := hex.DecodeString(wt.IV)
	if err != nil {
		log.Fatalf("Failed to decode IV %q: %v", wt.IV, err)
	}

	aad, err := hex.DecodeString(wt.AAD)
	if err != nil {
		log.Fatalf("Failed to decode AAD %q: %v", wt.AAD, err)
	}

	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode msg %q: %v", wt.Msg, err)
	}
	
	ct, err := hex.DecodeString(wt.CT)
	if err != nil {
		log.Fatalf("Failed to decode CT %q: %v", wt.CT, err)
	}

	tag, err := hex.DecodeString(wt.Tag)
	if err != nil {
		log.Fatalf("Failed to decode tag %q: %v", wt.Tag, err)
	}

	keyLen, ivLen, aadLen, msgLen, ctLen, tagLen := len(key), len(iv), len(aad), len(msg), len(ct), len(tag)

	if keyLen == 0 {
		key = append(key, 0)
	}
	if ivLen == 0 {
		iv = append(iv, 0)
	}
	if aadLen == 0 {
		aad = append(aad, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if ctLen == 0 {
		ct = append(ct, 0)
	}
	if tagLen == 0 {
		tag = append(tag, 0)
	}

	openSuccess := checkAesCcm(ctx, 0, key, keyLen, iv, ivLen, aad, aadLen, ct, ctLen, msg, msgLen, tag, tagLen, wt)
	sealSuccess := checkAesCcm(ctx, 1, key, keyLen, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)

	return openSuccess && sealSuccess
}

func runAesCcmTestGroup(wtg *wycheproofTestGroupAesCcm) bool {
	fmt.Printf("Running AES-CCM test group %v with IV size %d, key size %d and tag size %d...\n", wtg.Type, wtg.IVSize, wtg.KeySize, wtg.TagSize)

	var cipher *C.EVP_CIPHER
	switch wtg.KeySize {
	case 128:
		cipher = C.EVP_aes_128_ccm()
	case 192:
		cipher = C.EVP_aes_192_ccm()
	case 256:
		cipher = C.EVP_aes_256_ccm()
	default:
		log.Fatalf("Unsupported key size: %d", wtg.KeySize)
	}

	ctx := C.EVP_CIPHER_CTX_new()
	if ctx == nil {
		log.Fatal("EVP_CIPHER_CTX_new() failed")
	}
	defer C.EVP_CIPHER_CTX_free(ctx)

	C.EVP_CipherInit_ex(ctx, cipher, nil, nil, nil, 1)

	success := true
	for _, wt := range wtg.Tests {
		if !runAesCcmTest(ctx, wt) {
			success = false
		}
	}
	return success
}

func checkChaCha20Poly1305Open(ctx *C.EVP_AEAD_CTX, iv []byte, ivLen int, aad []byte, aadLen int, msg []byte, msgLen int, ct []byte, ctLen int, tag []byte, tagLen int, wt *wycheproofTestChaCha20Poly1305) bool {
	maxOutLen := ctLen + tagLen

	opened := make([]byte, maxOutLen)
	var openedMsgLen C.size_t

	catCtTag := append(ct, tag...)
	openRet := C.EVP_AEAD_CTX_open(ctx, (*C.uint8_t)(unsafe.Pointer(&opened[0])), (*C.size_t)(unsafe.Pointer(&openedMsgLen)), C.size_t(maxOutLen), (*C.uint8_t)(unsafe.Pointer(&iv[0])), C.size_t(ivLen), (*C.uint8_t)(unsafe.Pointer(&catCtTag[0])), C.size_t(len(catCtTag)), (*C.uint8_t)(unsafe.Pointer(&aad[0])), C.size_t(aadLen))

	if openRet != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) - EVP_AEAD_CTX_open() = %d, want %v\n", wt.TCID, wt.Comment, int(openRet), wt.Result)
		return false
	}

	if (openedMsgLen != C.size_t(msgLen)) {
		fmt.Printf("FAIL: Test case %d (%q) - open length mismatch: got %d, want %d\n", wt.TCID, wt.Comment, openedMsgLen, msgLen)
		return false
	}
	
	openedMsg := opened[0:openedMsgLen]
	if (msgLen == 0) {
		msg = nil
	}

	success := false
	if (bytes.Equal(openedMsg, msg)) || wt.Result == "invalid" {
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) - msg match: %t; want %v\n", wt.TCID, wt.Comment, bytes.Equal(openedMsg, msg), wt.Result)
	}
	return success
}

func checkChaCha20Poly1305Seal(ctx *C.EVP_AEAD_CTX, iv []byte, ivLen int, aad []byte, aadLen int, msg []byte, msgLen int, ct []byte, ctLen int, tag []byte, tagLen int, wt *wycheproofTestChaCha20Poly1305) bool {
	maxOutLen := msgLen + tagLen

	sealed := make([]byte, maxOutLen)
	var sealedLen C.size_t

	sealRet := C.EVP_AEAD_CTX_seal(ctx, (*C.uint8_t)(unsafe.Pointer(&sealed[0])), (*C.size_t)(unsafe.Pointer(&sealedLen)), C.size_t(maxOutLen), (*C.uint8_t)(unsafe.Pointer(&iv[0])), C.size_t(ivLen), (*C.uint8_t)(unsafe.Pointer(&msg[0])), C.size_t(msgLen), (*C.uint8_t)(unsafe.Pointer(&aad[0])), C.size_t(aadLen))

	if sealRet != 1 {
		fmt.Printf("FAIL: Test case %d (%q) - EVP_AEAD_CTX_seal() = %d, want %v\n", wt.TCID, wt.Comment, int(sealRet), wt.Result)
		return false
	}

	if (sealedLen != C.size_t(maxOutLen)) {
		fmt.Printf("FAIL: Test case %d (%q) - seal length mismatch: got %d, want %d\n", wt.TCID, wt.Comment, sealedLen, maxOutLen)
		return false
	}

	sealedCt := sealed[0:msgLen]
	sealedTag := sealed[msgLen: maxOutLen]

	success := false
	if (bytes.Equal(sealedCt, ct) && bytes.Equal(sealedTag, tag)) || wt.Result == "invalid" {
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) - EVP_AEAD_CTX_seal() = %d, ct match: %t, tag match: %t; want %v\n", wt.TCID, wt.Comment, int(sealRet), bytes.Equal(sealedCt, ct), bytes.Equal(sealedTag, tag), wt.Result)
	}
	return success
}

func runChaCha20Poly1305Test(iv_len int, key_len int, tag_len int, wt *wycheproofTestChaCha20Poly1305) bool {
	aead := C.EVP_aead_chacha20_poly1305()
	if aead == nil {
		log.Fatal("EVP_aead_chacha20_poly1305 failed")
	}

	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}
	iv, err := hex.DecodeString(wt.IV)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.IV, err)
	}
	aad, err := hex.DecodeString(wt.AAD)
	if err != nil {
		log.Fatalf("Failed to decode AAD %q: %v", wt.AAD, err)
	}
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode msg %q: %v", wt.Msg, err)
	}
	ct, err := hex.DecodeString(wt.CT)
	if err != nil {
		log.Fatalf("Failed to decode ct %q: %v", wt.CT, err)
	}
	tag, err := hex.DecodeString(wt.Tag)
	if err != nil {
		log.Fatalf("Failed to decode tag %q: %v", wt.Tag, err)
	}

	keyLen, ivLen, aadLen, tagLen := len(key), len(iv), len(aad), len(tag)
	if key_len != keyLen || iv_len != ivLen || tag_len != tagLen {
		fmt.Printf("FAIL: Test case %d (%q) - length mismatch; key: got %d, want %d; IV: got %d, want %d; tag: got %d, want %d\n", wt.TCID, wt.Comment, keyLen, key_len, ivLen, iv_len, tagLen, tag_len)
		return false
	}

	msgLen, ctLen := len(msg), len(ct)
	if msgLen != ctLen {
		fmt.Printf("FAIL: Test case %d (%q) - length mismatch: msgLen = %d, ctLen = %d\n", wt.TCID, wt.Comment, msgLen, ctLen)
		return false
	}

	if ivLen == 0 {
		iv = append(iv, 0)
	}
	if aadLen == 0 {
		aad = append(aad, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}

	var ctx C.EVP_AEAD_CTX
	if C.EVP_AEAD_CTX_init((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)), aead, (*C.uchar)(unsafe.Pointer(&key[0])), C.size_t(key_len), C.size_t(tag_len), nil) != 1 {
		log.Fatal("Failed to initialize AEAD context")
	}
	defer C.EVP_AEAD_CTX_cleanup((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)))

	openSuccess := checkChaCha20Poly1305Open((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)), iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)
	sealSuccess := checkChaCha20Poly1305Seal((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)), iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)

	return openSuccess && sealSuccess
}

func runChaCha20Poly1305TestGroup(wtg *wycheproofTestGroupChaCha20Poly1305) bool {
	// We currently only support nonces of length 12 (96 bits)
	if wtg.IVSize != 96 {
		return true
	}

	fmt.Printf("Running ChaCha20-Poly1305 test group %v with IV size %d, key size %d, tag size %d...\n", wtg.Type, wtg.IVSize, wtg.KeySize, wtg.TagSize)

	success := true
	for _, wt := range wtg.Tests {
		if !runChaCha20Poly1305Test(wtg.IVSize / 8, wtg.KeySize / 8, wtg.TagSize / 8, wt) {
			success = false
		}
	}
	return success
}

func runDSATest(dsa *C.DSA, h hash.Hash, wt *wycheproofTestDSA) bool {
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode message %q: %v", wt.Msg, err)
	}

	h.Reset()
	h.Write(msg)
	msg = h.Sum(nil)

	sig, err := hex.DecodeString(wt.Sig)
	if err != nil {
		log.Fatalf("Failed to decode signature %q: %v", wt.Sig, err)
	}

	msgLen, sigLen := len(msg), len(sig)
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if sigLen == 0 {
		sig = append(msg, 0)
	}

	ret := C.DSA_verify(0, (*C.uchar)(unsafe.Pointer(&msg[0])), C.int(msgLen),
		(*C.uchar)(unsafe.Pointer(&sig[0])), C.int(sigLen), dsa)

	success := true
	if (ret == 1) != (wt.Result == "valid") {
		fmt.Printf("FAIL: Test case %d (%q) - DSA_verify() = %d, want %v\n", wt.TCID, wt.Comment, ret, wt.Result)
		success = false
	}
	return success
}

func runDSATestGroup(wtg *wycheproofTestGroupDSA) bool {
	fmt.Printf("Running DSA test group %v, key size %d and %v...\n", wtg.Type, wtg.Key.KeySize, wtg.SHA)

	dsa := C.DSA_new()
	if dsa == nil {
		log.Fatal("DSA_new failed")
	}
	defer C.DSA_free(dsa)

	var bnG *C.BIGNUM
	wg := C.CString(wtg.Key.G)
	if C.BN_hex2bn(&bnG, wg) == 0 {
		log.Fatal("Failed to decode g")
	}

	var bnP *C.BIGNUM
	wp := C.CString(wtg.Key.P)
	if C.BN_hex2bn(&bnP, wp) == 0 {
		log.Fatal("Failed to decode p")
	}

	var bnQ *C.BIGNUM
	wq := C.CString(wtg.Key.Q)
	if C.BN_hex2bn(&bnQ, wq) == 0 {
		log.Fatal("Failed to decode q")
	}

	ret := C.DSA_set0_pqg(dsa, bnP, bnQ, bnG)
	if ret != 1 {
		log.Fatalf("DSA_set0_pqg returned %d", ret)
	}

	var bnY *C.BIGNUM
	wy := C.CString(wtg.Key.Y)
	if C.BN_hex2bn(&bnY, wy) == 0 {
		log.Fatal("Failed to decode y")
	}

	ret = C.DSA_set0_key(dsa, bnY, nil)
	if ret != 1 {
		log.Fatalf("DSA_set0_key returned %d", ret)
	}

	h, err := hashFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get hash: %v", err)
	}


	der, err := hex.DecodeString(wtg.KeyDER)
	if err != nil {
		log.Fatalf("Failed to decode DER encoded key: %v", err)
	}

	derLen := len(der)
	if derLen == 0 {
		der = append(der, 0)
	}

	Cder := (*C.uchar)(C.malloc((C.ulong)(derLen)))
	if Cder == nil {
		log.Fatal("malloc failed")
	}
	C.memcpy(unsafe.Pointer(Cder), unsafe.Pointer(&der[0]), C.ulong(derLen))

	p := (*C.uchar)(Cder)
	dsaDER := C.d2i_DSA_PUBKEY(nil, (**C.uchar)(&p), C.long(derLen))
	defer C.DSA_free(dsaDER)
	C.free(unsafe.Pointer(Cder))


	keyPEM := C.CString(wtg.KeyPEM);
	bio := C.BIO_new_mem_buf(unsafe.Pointer(keyPEM), C.int(len(wtg.KeyPEM)))
	if bio == nil {
		log.Fatal("BIO_new_mem_buf failed")
	}
	defer C.BIO_free(bio)

	dsaPEM := C.PEM_read_bio_DSA_PUBKEY(bio, nil, nil, nil)
	if dsaPEM == nil {
		log.Fatal("PEM_read_bio_DSA_PUBKEY failed")
	}
	defer C.DSA_free(dsaPEM)


	success := true
	for _, wt := range wtg.Tests {
		if !runDSATest(dsa, h, wt) {
			success = false
		}
		if !runDSATest(dsaDER, h, wt) {
			success = false
		}
		if !runDSATest(dsaPEM, h, wt) {
			success = false
		}
	}
	return success
}

func runECDSATest(ecKey *C.EC_KEY, nid int, h hash.Hash, wt *wycheproofTestECDSA) bool {
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode message %q: %v", wt.Msg, err)
	}

	h.Reset()
	h.Write(msg)
	msg = h.Sum(nil)

	sig, err := hex.DecodeString(wt.Sig)
	if err != nil {
		log.Fatalf("Failed to decode signature %q: %v", wt.Sig, err)
	}

	msgLen, sigLen := len(msg), len(sig)
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if sigLen == 0 {
		sig = append(sig, 0)
	}
	ret := C.ECDSA_verify(0, (*C.uchar)(unsafe.Pointer(&msg[0])), C.int(msgLen),
		(*C.uchar)(unsafe.Pointer(&sig[0])), C.int(sigLen), ecKey)

	// XXX audit acceptable cases...
	success := true
	if (ret == 1) != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d (%q) - ECDSA_verify() = %d, want %v\n", wt.TCID, wt.Comment, int(ret), wt.Result)
		success = false
	}
	return success
}

func runECDSATestGroup(wtg *wycheproofTestGroupECDSA) bool {
	// No secp256r1 support.
	if wtg.Key.Curve == "secp256r1" {
		return true
	}

	fmt.Printf("Running ECDSA test group %v with curve %v, key size %d and %v...\n", wtg.Type, wtg.Key.Curve, wtg.Key.KeySize, wtg.SHA)

	nid, err := nidFromString(wtg.Key.Curve)
	if err != nil {
		log.Fatalf("Failed to get nid for curve: %v", err)
	}
	ecKey := C.EC_KEY_new_by_curve_name(C.int(nid))
	if ecKey == nil {
		log.Fatal("EC_KEY_new_by_curve_name failed")
	}
	defer C.EC_KEY_free(ecKey)

	var bnX *C.BIGNUM
	wx := C.CString(wtg.Key.WX)
	if C.BN_hex2bn(&bnX, wx) == 0 {
		log.Fatal("Failed to decode WX")
	}
	C.free(unsafe.Pointer(wx))
	defer C.BN_free(bnX)

	var bnY *C.BIGNUM
	wy := C.CString(wtg.Key.WY)
	if C.BN_hex2bn(&bnY, wy) == 0 {
		log.Fatal("Failed to decode WY")
	}
	C.free(unsafe.Pointer(wy))
	defer C.BN_free(bnY)

	if C.EC_KEY_set_public_key_affine_coordinates(ecKey, bnX, bnY) != 1 {
		log.Fatal("Failed to set EC public key")
	}

	nid, err = nidFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get MD NID: %v", err)
	}
	h, err := hashFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get hash: %v", err)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runECDSATest(ecKey, nid, h, wt) {
			success = false
		}
	}
	return success
}

func runRSATest(rsa *C.RSA, nid int, h hash.Hash, wt *wycheproofTestRSA) bool {
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode message %q: %v", wt.Msg, err)
	}

	h.Reset()
	h.Write(msg)
	msg = h.Sum(nil)

	sig, err := hex.DecodeString(wt.Sig)
	if err != nil {
		log.Fatalf("Failed to decode signature %q: %v", wt.Sig, err)
	}

	msgLen, sigLen := len(msg), len(sig)
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if sigLen == 0 {
		sig = append(sig, 0)
	}

	ret := C.RSA_verify(C.int(nid), (*C.uchar)(unsafe.Pointer(&msg[0])), C.uint(msgLen),
		(*C.uchar)(unsafe.Pointer(&sig[0])), C.uint(sigLen), rsa)

	// XXX audit acceptable cases...
	success := true
	if (ret == 1) != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d (%q) - RSA_verify() = %d, want %v\n", wt.TCID, wt.Comment, int(ret), wt.Result)
		success = false
	}
	return success
}

func runRSATestGroup(wtg *wycheproofTestGroupRSA) bool {
	fmt.Printf("Running RSA test group %v with key size %d and %v...\n", wtg.Type, wtg.KeySize, wtg.SHA)

	rsa := C.RSA_new()
	if rsa == nil {
		log.Fatal("RSA_new failed")
	}
	defer C.RSA_free(rsa)

	e := C.CString(wtg.E)
	if C.BN_hex2bn(&rsa.e, e) == 0 {
		log.Fatal("Failed to set RSA e")
	}
	C.free(unsafe.Pointer(e))

	n := C.CString(wtg.N)
	if C.BN_hex2bn(&rsa.n, n) == 0 {
		log.Fatal("Failed to set RSA n")
	}
	C.free(unsafe.Pointer(n))

	nid, err := nidFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get MD NID: %v", err)
	}
	h, err := hashFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get hash: %v", err)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runRSATest(rsa, nid, h, wt) {
			success = false
		}
	}
	return success
}

func runX25519Test(wt *wycheproofTestX25519) bool {
	public, err := hex.DecodeString(wt.Public)
	if err != nil {
		log.Fatalf("Failed to decode public %q: %v", wt.Public, err)
	}
	private, err := hex.DecodeString(wt.Private)
	if err != nil {
		log.Fatalf("Failed to decode private %q: %v", wt.Private, err)
	}
	shared, err := hex.DecodeString(wt.Shared)
	if err != nil {
		log.Fatalf("Failed to decode shared %q: %v", wt.Shared, err)
	}

	got := make([]byte, C.X25519_KEY_LENGTH)
	result := true

	if C.X25519((*C.uint8_t)(unsafe.Pointer(&got[0])), (*C.uint8_t)(unsafe.Pointer(&private[0])), (*C.uint8_t)(unsafe.Pointer(&public[0]))) != 1 {
		result = false
	} else {
		result = bytes.Equal(got, shared)
	}

	// XXX audit acceptable cases...
	success := true
	if result != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d (%q) - X25519(), want %v\n", wt.TCID, wt.Comment, wt.Result)
		success = false
	}
	return success
}

func runX25519TestGroup(wtg *wycheproofTestGroupX25519) bool {
	fmt.Printf("Running X25519 test group with curve %v...\n", wtg.Curve)

	success := true
	for _, wt := range wtg.Tests {
		if !runX25519Test(wt) {
			success = false
		}
	}
	return success
}

func runTestVectors(path string) bool {
	b, err := ioutil.ReadFile(path)
	if err != nil {
		log.Fatalf("Failed to read test vectors: %v", err)
	}
	wtv := &wycheproofTestVectors{}
	if err := json.Unmarshal(b, wtv); err != nil {
		log.Fatalf("Failed to unmarshal JSON: %v", err)
	}
	fmt.Printf("Loaded Wycheproof test vectors for %v with %d tests from %q\n", wtv.Algorithm, wtv.NumberOfTests, filepath.Base(path))

	var wtg interface{}
	switch wtv.Algorithm {
	case "AES-CBC-PKCS5":
		wtg = &wycheproofTestGroupAesCbcPkcs5{}
	case "AES-CCM":
		wtg = &wycheproofTestGroupAesCcm{}
	case "CHACHA20-POLY1305":
		wtg = &wycheproofTestGroupChaCha20Poly1305{}
	case "DSA":
		wtg = &wycheproofTestGroupDSA{}
	case "ECDSA":
		wtg = &wycheproofTestGroupECDSA{}
	case "RSASig":
		wtg = &wycheproofTestGroupRSA{}
	case "X25519":
		wtg = &wycheproofTestGroupX25519{}
	default:
		log.Fatalf("Unknown test vector algorithm %q", wtv.Algorithm)
	}

	success := true
	for _, tg := range wtv.TestGroups {
		if err := json.Unmarshal(tg, wtg); err != nil {
			log.Fatalf("Failed to unmarshal test groups JSON: %v", err)
		}
		switch wtv.Algorithm {
		case "AES-CBC-PKCS5":
			if !runAesCbcPkcs5TestGroup(wtg.(*wycheproofTestGroupAesCbcPkcs5)) {
				success = false
			}
		case "AES-CCM":
			if !runAesCcmTestGroup(wtg.(*wycheproofTestGroupAesCcm)) {
				success = false
			}
		case "CHACHA20-POLY1305":
			if !runChaCha20Poly1305TestGroup(wtg.(*wycheproofTestGroupChaCha20Poly1305)) {
				success = false
			}
		case "DSA":
			if !runDSATestGroup(wtg.(*wycheproofTestGroupDSA)) {
				success = false
			}
		case "ECDSA":
			if !runECDSATestGroup(wtg.(*wycheproofTestGroupECDSA)) {
				success = false
			}
		case "RSASig":
			if !runRSATestGroup(wtg.(*wycheproofTestGroupRSA)) {
				success = false
			}
		case "X25519":
			if !runX25519TestGroup(wtg.(*wycheproofTestGroupX25519)) {
				success = false
			}
		default:
			log.Fatalf("Unknown test vector algorithm %q", wtv.Algorithm)
		}
	}
	return success
}

func main() {
	if _, err := os.Stat(testVectorPath); os.IsNotExist(err) {
		fmt.Printf("package wycheproof-testvectors is required for this regress\n")
		fmt.Printf("SKIPPING\n")
		os.Exit(0)
	}

	// AES, ECDH, RSA-PSS
	tests := []struct {
		name    string
		pattern string
	}{
		{"AES", "aes_c[bc]*test.json"},
		{"ChaCha20-Poly1305", "chacha20_poly1305_test.json"},
		{"DSA", "dsa_test.json"},
		{"ECDSA", "ecdsa_[^w]*test.json"}, // Skip ecdsa_webcrypto_test.json for now.
		{"RSA signature", "rsa_signature_*test.json"},
		{"X25519", "x25519_*test.json"},
	}

	success := true

	for _, test := range tests {
		tvs, err := filepath.Glob(filepath.Join(testVectorPath, test.pattern))
		if err != nil {
			log.Fatalf("Failed to glob %v test vectors: %v", test.name, err)
		}
		if len(tvs) == 0 {
			log.Fatalf("Failed to find %v test vectors at %q\n", test.name, testVectorPath)
		}
		for _, tv := range tvs {
			if !runTestVectors(tv) {
				success = false
			}
		}
	}

	if !success {
		os.Exit(1)
	}
}
