/* $OpenBSD: wycheproof.go,v 1.87 2018/11/07 22:51:17 tb Exp $ */
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

#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/bn.h>
#include <openssl/cmac.h>
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
	"encoding/base64"
	"encoding/hex"
	"encoding/json"
	"flag"
	"fmt"
	"hash"
	"io/ioutil"
	"log"
	"os"
	"path/filepath"
	"sort"
	"unsafe"
)

const testVectorPath = "/usr/local/share/wycheproof/testvectors"

var acceptableAudit = false
var acceptableComments map[string]int
var acceptableFlags map[string]int

type wycheproofJWKPublic struct {
	Crv string `json:"crv"`
	KID string `json:"kid"`
	KTY string `json:"kty"`
	X   string `json:"x"`
	Y   string `json:"y"`
}

type wycheproofJWKPrivate struct {
	Crv string `json:"crv"`
	D   string `json:"d"`
	KID string `json:"kid"`
	KTY string `json:"kty"`
	X   string `json:"x"`
	Y   string `json:"y"`
}

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

type wycheproofTestGroupAead struct {
	IVSize  int                   `json:"ivSize"`
	KeySize int                   `json:"keySize"`
	TagSize int                   `json:"tagSize"`
	Type    string                `json:"type"`
	Tests   []*wycheproofTestAead `json:"tests"`
}

type wycheproofTestAead struct {
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

type wycheproofTestGroupAesCmac struct {
	KeySize int                      `json:"keySize"`
	TagSize int                      `json:"tagSize"`
	Type    string                   `json:"type"`
	Tests   []*wycheproofTestAesCmac `json:"tests"`
}

type wycheproofTestAesCmac struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Key     string   `json:"key"`
	Msg     string   `json:"msg"`
	Tag     string   `json:"tag"`
	Result  string   `json:"result"`
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

type wycheproofTestECDH struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Public  string   `json:"public"`
	Private string   `json:"private"`
	Shared  string   `json:"shared"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupECDH struct {
	Curve    string                `json:"curve"`
	Encoding string                `json:"encoding"`
	Type     string                `json:"type"`
	Tests    []*wycheproofTestECDH `json:"tests"`
}

type wycheproofTestECDHWebCrypto struct {
	TCID    int                   `json:"tcId"`
	Comment string                `json:"comment"`
	Public  *wycheproofJWKPublic  `json:"public"`
	Private *wycheproofJWKPrivate `json:"private"`
	Shared  string                `json:"shared"`
	Result  string                `json:"result"`
	Flags   []string              `json:"flags"`
}

type wycheproofTestGroupECDHWebCrypto struct {
	Curve    string                         `json:"curve"`
	Encoding string                         `json:"encoding"`
	Type     string                         `json:"type"`
	Tests    []*wycheproofTestECDHWebCrypto `json:"tests"`
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

type wycheproofTestGroupECDSAWebCrypto struct {
	JWK    *wycheproofJWKPublic   `json:"jwk"`
	Key    *wycheproofECDSAKey    `json:"key"`
	KeyDER string                 `json:"keyDer"`
	KeyPEM string                 `json:"keyPem"`
	SHA    string                 `json:"sha"`
	Type   string                 `json:"type"`
	Tests  []*wycheproofTestECDSA `json:"tests"`
}

type wycheproofTestKW struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Key     string   `json:"key"`
	Msg     string   `json:"msg"`
	CT      string   `json:"ct"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupKW struct {
	KeySize int                 `json:"keySize"`
	Type    string              `json:"type"`
	Tests   []*wycheproofTestKW `json:"tests"`
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

type wycheproofTestRSASSA struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Msg     string   `json:"msg"`
	Sig     string   `json:"sig"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroupRSASSA struct {
	E       string                  `json:"e"`
	KeyASN  string                  `json:"keyAsn"`
	KeyDER  string                  `json:"keyDer"`
	KeyPEM  string                  `json:"keyPem"`
	KeySize int                     `json:"keysize"`
	MGF     string                  `json:"mgf"`
	MGFSHA  string                  `json:"mgfSha"`
	N       string                  `json:"n"`
	SLen    int                     `json:"sLen"`
	SHA     string                  `json:"sha"`
	Type    string                  `json:"type"`
	Tests   []*wycheproofTestRSASSA `json:"tests"`
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
	"P-256K":          C.NID_secp256k1,
	"secp256r1":       C.NID_X9_62_prime256v1, // RFC 8422, Table 4, p.32
	"P-256":           C.NID_X9_62_prime256v1,
	"secp384r1":       C.NID_secp384r1,
	"P-384":           C.NID_secp384r1,
	"secp521r1":       C.NID_secp521r1,
	"P-521":           C.NID_secp521r1,
	"SHA-1":           C.NID_sha1,
	"SHA-224":         C.NID_sha224,
	"SHA-256":         C.NID_sha256,
	"SHA-384":         C.NID_sha384,
	"SHA-512":         C.NID_sha512,
}

func gatherAcceptableStatistics(testcase int, comment string, flags []string) {
	fmt.Printf("AUDIT: Test case %d (%q) %v\n", testcase, comment, flags)

	if comment == "" {
		acceptableComments["No comment"]++
	} else {
		acceptableComments[comment]++
	}

	if len(flags) == 0 {
		acceptableFlags["NoFlag"]++
	} else {
		for _, flag := range flags {
			acceptableFlags[flag]++
		}
	}
}

func printAcceptableStatistics() {
	fmt.Printf("\nComment statistics:\n")

	var comments []string
	for comment := range acceptableComments {
		comments = append(comments, comment)
	}
	sort.Strings(comments)
	for _, comment := range comments {
		prcomment := comment
		if len(comment) > 45 {
			prcomment = comment[0:42] + "..."
		}
		fmt.Printf("%-45v %5d\n", prcomment, acceptableComments[comment])
	}

	fmt.Printf("\nFlag statistics:\n")
	var flags []string
	for flag := range acceptableFlags {
		flags = append(flags, flag)
	}
	sort.Strings(flags)
	for _, flag := range flags {
		fmt.Printf("%-45v %5d\n", flag, acceptableFlags[flag])
	}
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

func hashEvpMdFromString(hs string) (*C.EVP_MD, error) {
	switch hs {
	case "SHA-1":
		return C.EVP_sha1(), nil
	case "SHA-224":
		return C.EVP_sha224(), nil
	case "SHA-256":
		return C.EVP_sha256(), nil
	case "SHA-384":
		return C.EVP_sha384(), nil
	case "SHA-512":
		return C.EVP_sha512(), nil
	default:
		return nil, fmt.Errorf("unknown hash %q", hs)
	}
}

func checkAesCbcPkcs5(ctx *C.EVP_CIPHER_CTX, doEncrypt int, key []byte, keyLen int,
	iv []byte, ivLen int, in []byte, inLen int, out []byte, outLen int,
	wt *wycheproofTestAesCbcPkcs5) bool {
	var action string
	if doEncrypt == 1 {
		action = "encrypting"
	} else {
		action = "decrypting"
	}

	ret := C.EVP_CipherInit_ex(ctx, nil, nil, (*C.uchar)(unsafe.Pointer(&key[0])),
		(*C.uchar)(unsafe.Pointer(&iv[0])), C.int(doEncrypt))
	if ret != 1 {
		log.Fatalf("EVP_CipherInit_ex failed: %d", ret)
	}

	cipherOut := make([]byte, inLen + C.EVP_MAX_BLOCK_LENGTH)
	var cipherOutLen C.int

	ret = C.EVP_CipherUpdate(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[0])), &cipherOutLen,
		(*C.uchar)(unsafe.Pointer(&in[0])), C.int(inLen))
	if ret != 1 {
		if wt.Result == "invalid" {
			fmt.Printf("INFO: Test case %d (%q) [%v] %v - EVP_CipherUpdate() = %d, want %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - EVP_CipherUpdate() = %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
		return false
	}

	var finallen C.int
	ret = C.EVP_CipherFinal_ex(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[cipherOutLen])), &finallen)
	if ret != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - EVP_CipherFinal_ex() = %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
		return false
	}

	cipherOutLen += finallen
	if cipherOutLen != C.int(outLen) && wt.Result != "invalid" {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - open length mismatch: got %d, want %d\n",
			wt.TCID, wt.Comment, action, wt.Flags, cipherOutLen, outLen)
		return false
	}

	openedMsg := out[0:cipherOutLen]
	if outLen == 0 {
		out = nil
	}

	success := false
	if bytes.Equal(openedMsg, out) || wt.Result == "invalid" {
		success = true
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
	} else {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - msg match: %t; want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, bytes.Equal(openedMsg, out), wt.Result)
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

	if keyLen == 0 {
		key = append(key, 0)
	}
	if ivLen == 0 {
		iv = append(iv, 0)
	}
	if ctLen == 0 {
		ct = append(ct, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}

	openSuccess := checkAesCbcPkcs5(ctx, 0, key, keyLen, iv, ivLen, ct, ctLen, msg, msgLen, wt)
	sealSuccess := checkAesCbcPkcs5(ctx, 1, key, keyLen, iv, ivLen, msg, msgLen, ct, ctLen, wt)

	return openSuccess && sealSuccess
}

func runAesCbcPkcs5TestGroup(algorithm string, wtg *wycheproofTestGroupAesCbcPkcs5) bool {
	fmt.Printf("Running %v test group %v with IV size %d and key size %d...\n",
		algorithm, wtg.Type, wtg.IVSize, wtg.KeySize)

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

func checkAesAead(algorithm string, ctx *C.EVP_CIPHER_CTX, doEncrypt int,
	key []byte, keyLen int, iv []byte, ivLen int, aad []byte, aadLen int,
	in []byte, inLen int, out []byte, outLen int, tag []byte, tagLen int,
	wt *wycheproofTestAead) bool {
	var ctrlSetIVLen C.int
	var ctrlSetTag C.int
	var ctrlGetTag C.int

	doCCM := false
	switch algorithm {
	case "AES-CCM":
		doCCM = true
		ctrlSetIVLen = C.EVP_CTRL_CCM_SET_IVLEN
		ctrlSetTag = C.EVP_CTRL_CCM_SET_TAG
		ctrlGetTag = C.EVP_CTRL_CCM_GET_TAG
	case "AES-GCM":
		ctrlSetIVLen = C.EVP_CTRL_GCM_SET_IVLEN
		ctrlSetTag = C.EVP_CTRL_GCM_SET_TAG
		ctrlGetTag = C.EVP_CTRL_GCM_GET_TAG
	}

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

	ret = C.EVP_CIPHER_CTX_ctrl(ctx, ctrlSetIVLen, C.int(ivLen), nil)
	if ret != 1 {
		if wt.Comment == "Nonce is too long" || wt.Comment == "Invalid nonce size" ||
			wt.Comment == "0 size IV is not valid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - setting IV len to %d failed. got %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ivLen, ret, wt.Result)
		return false
	}

	if doEncrypt == 0 || doCCM {
		ret = C.EVP_CIPHER_CTX_ctrl(ctx, ctrlSetTag, C.int(tagLen), setTag)
		if ret != 1 {
			if wt.Comment == "Invalid tag size" {
				return true
			}
			fmt.Printf("FAIL: Test case %d (%q) [%v] %v - setting tag length to %d failed. got %d, want %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, tagLen, ret, wt.Result)
			return false
		}
	}

	ret = C.EVP_CipherInit_ex(ctx, nil, nil, (*C.uchar)(unsafe.Pointer(&key[0])),
		(*C.uchar)(unsafe.Pointer(&iv[0])), C.int(doEncrypt))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - setting key and IV failed. got %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
		return false
	}

	var cipherOutLen C.int
	if doCCM {
		ret = C.EVP_CipherUpdate(ctx, nil, &cipherOutLen, nil, C.int(inLen))
		if ret != 1 {
			fmt.Printf("FAIL: Test case %d (%q) [%v] %v - setting input length to %d failed. got %d, want %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, inLen, ret, wt.Result)
			return false
		}
	}

	ret = C.EVP_CipherUpdate(ctx, nil, &cipherOutLen, (*C.uchar)(unsafe.Pointer(&aad[0])), C.int(aadLen))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - processing AAD failed. got %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
		return false
	}

	cipherOutLen = 0
	cipherOut := make([]byte, inLen)
	if inLen == 0 {
		cipherOut = append(cipherOut, 0)
	}

	ret = C.EVP_CipherUpdate(ctx, (*C.uchar)(unsafe.Pointer(&cipherOut[0])), &cipherOutLen,
		(*C.uchar)(unsafe.Pointer(&in[0])), C.int(inLen))
	if ret != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - EVP_CipherUpdate() = %d, want %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
		return false
	}

	if doEncrypt == 1 {
		var tmpLen C.int
		dummyOut := make([]byte, 16)

		ret = C.EVP_CipherFinal_ex(ctx, (*C.uchar)(unsafe.Pointer(&dummyOut[0])), &tmpLen)
		if ret != 1 {
			fmt.Printf("FAIL: Test case %d (%q) [%v] %v - EVP_CipherFinal_ex() = %d, want %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
			return false
		}
		cipherOutLen += tmpLen
	}

	if cipherOutLen != C.int(outLen) {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - cipherOutLen %d != outLen %d. Result %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, cipherOutLen, outLen, wt.Result)
		return false
	}

	success := true
	if !bytes.Equal(cipherOut, out) {
		fmt.Printf("FAIL: Test case %d (%q) [%v] %v - expected and computed output do not match. Result: %v\n",
			wt.TCID, wt.Comment, action, wt.Flags, wt.Result)
		success = false
	}
	if doEncrypt == 1 {
		tagOut := make([]byte, tagLen)
		ret = C.EVP_CIPHER_CTX_ctrl(ctx, ctrlGetTag, C.int(tagLen), unsafe.Pointer(&tagOut[0]))
		if ret != 1 {
			fmt.Printf("FAIL: Test case %d (%q) [%v] %v - EVP_CIPHER_CTX_ctrl() = %d, want %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
			return false
		}

		// There are no acceptable CCM cases. All acceptable GCM tests
		// pass. They have len(IV) <= 48. NIST SP 800-38D, 5.2.1.1, p.8,
		// allows 1 <= len(IV) <= 2^64-1, but notes:
		//   "For IVs it is recommended that implementations restrict
		//    support to the length of 96 bits, to promote
		//    interoperability, efficiency and simplicity of design."
		if bytes.Equal(tagOut, tag) != (wt.Result == "valid" || wt.Result == "acceptable") {
			fmt.Printf("FAIL: Test case %d (%q) [%v] %v - expected and computed tag do not match - ret: %d, Result: %v\n",
				wt.TCID, wt.Comment, action, wt.Flags, ret, wt.Result)
			success = false
		}
		if acceptableAudit && bytes.Equal(tagOut, tag) && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
	}
	return success
}

func runAesAeadTest(algorithm string, ctx *C.EVP_CIPHER_CTX, aead *C.EVP_AEAD, wt *wycheproofTestAead) bool {
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

	openEvp := checkAesAead(algorithm, ctx, 0, key, keyLen, iv, ivLen, aad, aadLen, ct, ctLen, msg, msgLen, tag, tagLen, wt)
	sealEvp := checkAesAead(algorithm, ctx, 1, key, keyLen, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)

	openAead, sealAead := true, true
	if aead != nil {
		var ctx C.EVP_AEAD_CTX
		if C.EVP_AEAD_CTX_init(&ctx, aead, (*C.uchar)(unsafe.Pointer(&key[0])), C.size_t(keyLen), C.size_t(tagLen), nil) != 1 {
			log.Fatal("Failed to initialize AEAD context")
		}
		defer C.EVP_AEAD_CTX_cleanup(&ctx)

		// Make sure we don't accidentally prepend or compare against a 0.
		if ctLen == 0 {
			ct = nil
		}

		openAead = checkAeadOpen(&ctx, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)
		sealAead = checkAeadSeal(&ctx, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)
	}

	return openEvp && sealEvp && openAead && sealAead
}

func runAesAeadTestGroup(algorithm string, wtg *wycheproofTestGroupAead) bool {
	fmt.Printf("Running %v test group %v with IV size %d, key size %d and tag size %d...\n",
		algorithm, wtg.Type, wtg.IVSize, wtg.KeySize, wtg.TagSize)

	var cipher *C.EVP_CIPHER
	var aead *C.EVP_AEAD
	switch algorithm {
	case "AES-CCM":
		switch wtg.KeySize {
		case 128:
			cipher = C.EVP_aes_128_ccm()
		case 192:
			cipher = C.EVP_aes_192_ccm()
		case 256:
			cipher = C.EVP_aes_256_ccm()
		default:
			fmt.Printf("INFO: Skipping tests with invalid key size %d\n", wtg.KeySize)
			return true
		}
	case "AES-GCM":
		switch wtg.KeySize {
		case 128:
			cipher = C.EVP_aes_128_gcm()
			aead = C.EVP_aead_aes_128_gcm()
		case 192:
			cipher = C.EVP_aes_192_gcm()
		case 256:
			cipher = C.EVP_aes_256_gcm()
			aead = C.EVP_aead_aes_256_gcm()
		default:
			fmt.Printf("INFO: Skipping tests with invalid key size %d\n", wtg.KeySize)
			return true
		}
	default:
		log.Fatalf("runAesAeadTestGroup() - unhandled algorithm: %v", algorithm)
	}

	ctx := C.EVP_CIPHER_CTX_new()
	if ctx == nil {
		log.Fatal("EVP_CIPHER_CTX_new() failed")
	}
	defer C.EVP_CIPHER_CTX_free(ctx)

	C.EVP_CipherInit_ex(ctx, cipher, nil, nil, nil, 1)

	success := true
	for _, wt := range wtg.Tests {
		if !runAesAeadTest(algorithm, ctx, aead, wt) {
			success = false
		}
	}
	return success
}

func runAesCmacTest(cipher *C.EVP_CIPHER, wt *wycheproofTestAesCmac) bool {
	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}

	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode msg %q: %v", wt.Msg, err)
	}

	tag, err := hex.DecodeString(wt.Tag)
	if err != nil {
		log.Fatalf("Failed to decode tag %q: %v", wt.Tag, err)
	}

	keyLen, msgLen, tagLen := len(key), len(msg), len(tag)

	if keyLen == 0 {
		key = append(key, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if tagLen == 0 {
		tag = append(tag, 0)
	}

	ctx := C.CMAC_CTX_new()
	if ctx == nil {
		log.Fatal("CMAC_CTX_new failed")
	}
	defer C.CMAC_CTX_free(ctx)

	ret := C.CMAC_Init(ctx, unsafe.Pointer(&key[0]), C.size_t(keyLen), cipher, nil)
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - CMAC_Init() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}

	ret = C.CMAC_Update(ctx, unsafe.Pointer(&msg[0]), C.size_t(msgLen))
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - CMAC_Update() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}

	var outLen C.size_t
	outTag := make([]byte, 16)

	ret = C.CMAC_Final(ctx, (*C.uchar)(unsafe.Pointer(&outTag[0])), &outLen)
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - CMAC_Final() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}

	outTag = outTag[0:tagLen]

	success := true
	if bytes.Equal(tag, outTag) != (wt.Result == "valid") {
		fmt.Printf("FAIL: Test case %d (%q) %v - want %v\n",
			wt.TCID, wt.Comment, wt.Flags, wt.Result)
		success = false
	}
	return success
}

func runAesCmacTestGroup(algorithm string, wtg *wycheproofTestGroupAesCmac) bool {
	fmt.Printf("Running %v test group %v with key size %d and tag size %d...\n",
		algorithm, wtg.Type, wtg.KeySize, wtg.TagSize)
	var cipher *C.EVP_CIPHER

	switch wtg.KeySize {
	case 128:
		cipher = C.EVP_aes_128_cbc()
	case 192:
		cipher = C.EVP_aes_192_cbc()
	case 256:
		cipher = C.EVP_aes_256_cbc()
	default:
		fmt.Printf("INFO: Skipping tests with invalid key size %d\n", wtg.KeySize)
		return true
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runAesCmacTest(cipher, wt) {
			success = false
		}
	}
	return success
}

func checkAeadOpen(ctx *C.EVP_AEAD_CTX, iv []byte, ivLen int, aad []byte, aadLen int, msg []byte, msgLen int,
	ct []byte, ctLen int, tag []byte, tagLen int, wt *wycheproofTestAead) bool {
	maxOutLen := ctLen + tagLen

	opened := make([]byte, maxOutLen)
	var openedMsgLen C.size_t

	catCtTag := append(ct, tag...)
	openRet := C.EVP_AEAD_CTX_open(ctx, (*C.uint8_t)(unsafe.Pointer(&opened[0])),
		(*C.size_t)(unsafe.Pointer(&openedMsgLen)), C.size_t(maxOutLen),
		(*C.uint8_t)(unsafe.Pointer(&iv[0])), C.size_t(ivLen),
		(*C.uint8_t)(unsafe.Pointer(&catCtTag[0])), C.size_t(len(catCtTag)),
		(*C.uint8_t)(unsafe.Pointer(&aad[0])), C.size_t(aadLen))

	if openRet != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - EVP_AEAD_CTX_open() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(openRet), wt.Result)
		return false
	}

	if openedMsgLen != C.size_t(msgLen) {
		fmt.Printf("FAIL: Test case %d (%q) %v - open length mismatch: got %d, want %d\n",
			wt.TCID, wt.Comment, wt.Flags, openedMsgLen, msgLen)
		return false
	}

	openedMsg := opened[0:openedMsgLen]
	if msgLen == 0 {
		msg = nil
	}

	success := false
	if bytes.Equal(openedMsg, msg) || wt.Result == "invalid" {
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) %v - msg match: %t; want %v\n",
			wt.TCID, wt.Comment, wt.Flags, bytes.Equal(openedMsg, msg), wt.Result)
	}
	return success
}

func checkAeadSeal(ctx *C.EVP_AEAD_CTX, iv []byte, ivLen int, aad []byte, aadLen int, msg []byte,
	msgLen int, ct []byte, ctLen int, tag []byte, tagLen int, wt *wycheproofTestAead) bool {
	maxOutLen := msgLen + tagLen

	sealed := make([]byte, maxOutLen)
	var sealedLen C.size_t

	sealRet := C.EVP_AEAD_CTX_seal(ctx, (*C.uint8_t)(unsafe.Pointer(&sealed[0])),
		(*C.size_t)(unsafe.Pointer(&sealedLen)), C.size_t(maxOutLen),
		(*C.uint8_t)(unsafe.Pointer(&iv[0])), C.size_t(ivLen),
		(*C.uint8_t)(unsafe.Pointer(&msg[0])), C.size_t(msgLen),
		(*C.uint8_t)(unsafe.Pointer(&aad[0])), C.size_t(aadLen))

	if sealRet != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - EVP_AEAD_CTX_seal() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(sealRet), wt.Result)
		return false
	}

	if sealedLen != C.size_t(maxOutLen) {
		fmt.Printf("FAIL: Test case %d (%q) %v - seal length mismatch: got %d, want %d\n",
			wt.TCID, wt.Comment, wt.Flags, sealedLen, maxOutLen)
		return false
	}

	sealedCt := sealed[0:msgLen]
	sealedTag := sealed[msgLen:maxOutLen]

	success := false
	if bytes.Equal(sealedCt, ct) && bytes.Equal(sealedTag, tag) || wt.Result == "invalid" {
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) %v - EVP_AEAD_CTX_seal() = %d, ct match: %t, tag match: %t; want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(sealRet),
			bytes.Equal(sealedCt, ct), bytes.Equal(sealedTag, tag), wt.Result)
	}
	return success
}

func runChaCha20Poly1305Test(wt *wycheproofTestAead) bool {
	aead := C.EVP_aead_chacha20_poly1305()

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

	keyLen, ivLen, aadLen, msgLen, ctLen, tagLen := len(key), len(iv), len(aad), len(msg), len(ct), len(tag)

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
	if C.EVP_AEAD_CTX_init(&ctx, aead, (*C.uchar)(unsafe.Pointer(&key[0])), C.size_t(keyLen), C.size_t(tagLen), nil) != 1 {
		log.Fatal("Failed to initialize AEAD context")
	}
	defer C.EVP_AEAD_CTX_cleanup(&ctx)

	openSuccess := checkAeadOpen(&ctx, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)
	sealSuccess := checkAeadSeal(&ctx, iv, ivLen, aad, aadLen, msg, msgLen, ct, ctLen, tag, tagLen, wt)

	return openSuccess && sealSuccess
}

func runChaCha20Poly1305TestGroup(algorithm string, wtg *wycheproofTestGroupAead) bool {
	// We currently only support nonces of length 12 (96 bits)
	if wtg.IVSize != 96 {
		return true
	}

	fmt.Printf("Running %v test group %v with IV size %d, key size %d, tag size %d...\n",
		algorithm, wtg.Type, wtg.IVSize, wtg.KeySize, wtg.TagSize)

	success := true
	for _, wt := range wtg.Tests {
		if !runChaCha20Poly1305Test(wt) {
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
	if ret == 1 != (wt.Result == "valid") {
		fmt.Printf("FAIL: Test case %d (%q) %v - DSA_verify() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		success = false
	}
	return success
}

func runDSATestGroup(algorithm string, wtg *wycheproofTestGroupDSA) bool {
	fmt.Printf("Running %v test group %v, key size %d and %v...\n",
		algorithm, wtg.Type, wtg.Key.KeySize, wtg.SHA)

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
	C.free(unsafe.Pointer(wg))

	var bnP *C.BIGNUM
	wp := C.CString(wtg.Key.P)
	if C.BN_hex2bn(&bnP, wp) == 0 {
		log.Fatal("Failed to decode p")
	}
	C.free(unsafe.Pointer(wp))

	var bnQ *C.BIGNUM
	wq := C.CString(wtg.Key.Q)
	if C.BN_hex2bn(&bnQ, wq) == 0 {
		log.Fatal("Failed to decode q")
	}
	C.free(unsafe.Pointer(wq))

	ret := C.DSA_set0_pqg(dsa, bnP, bnQ, bnG)
	if ret != 1 {
		log.Fatalf("DSA_set0_pqg returned %d", ret)
	}

	var bnY *C.BIGNUM
	wy := C.CString(wtg.Key.Y)
	if C.BN_hex2bn(&bnY, wy) == 0 {
		log.Fatal("Failed to decode y")
	}
	C.free(unsafe.Pointer(wy))

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

	Cder := (*C.uchar)(C.malloc(C.ulong(derLen)))
	if Cder == nil {
		log.Fatal("malloc failed")
	}
	C.memcpy(unsafe.Pointer(Cder), unsafe.Pointer(&der[0]), C.ulong(derLen))

	p := (*C.uchar)(Cder)
	dsaDER := C.d2i_DSA_PUBKEY(nil, (**C.uchar)(&p), C.long(derLen))
	defer C.DSA_free(dsaDER)
	C.free(unsafe.Pointer(Cder))

	keyPEM := C.CString(wtg.KeyPEM)
	bio := C.BIO_new_mem_buf(unsafe.Pointer(keyPEM), C.int(len(wtg.KeyPEM)))
	if bio == nil {
		log.Fatal("BIO_new_mem_buf failed")
	}
	defer C.free(unsafe.Pointer(keyPEM))
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

func runECDHTest(nid int, doECpoint bool, wt *wycheproofTestECDH) bool {
	privKey := C.EC_KEY_new_by_curve_name(C.int(nid))
	if privKey == nil {
		log.Fatalf("EC_KEY_new_by_curve_name failed")
	}
	defer C.EC_KEY_free(privKey)

	var bnPriv *C.BIGNUM
	wPriv := C.CString(wt.Private)
	if C.BN_hex2bn(&bnPriv, wPriv) == 0 {
		log.Fatal("Failed to decode wPriv")
	}
	C.free(unsafe.Pointer(wPriv))
	defer C.BN_free(bnPriv)

	ret := C.EC_KEY_set_private_key(privKey, bnPriv)
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - EC_KEY_set_private_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}

	pub, err := hex.DecodeString(wt.Public)
	if err != nil {
		log.Fatalf("Failed to decode public key: %v", err)
	}

	pubLen := len(pub)
	if pubLen == 0 {
		pub = append(pub, 0)
	}

	Cpub := (*C.uchar)(C.malloc(C.ulong(pubLen)))
	if Cpub == nil {
		log.Fatal("malloc failed")
	}
	C.memcpy(unsafe.Pointer(Cpub), unsafe.Pointer(&pub[0]), C.ulong(pubLen))

	p := (*C.uchar)(Cpub)
	var pubKey *C.EC_KEY
	if doECpoint {
		pubKey = C.EC_KEY_new_by_curve_name(C.int(nid))
		if pubKey == nil {
			log.Fatal("EC_KEY_new_by_curve_name failed")
		}
		pubKey = C.o2i_ECPublicKey(&pubKey, (**C.uchar)(&p), C.long(pubLen))
	} else {
		pubKey = C.d2i_EC_PUBKEY(nil, (**C.uchar)(&p), C.long(pubLen))
	}
	defer C.EC_KEY_free(pubKey)
	C.free(unsafe.Pointer(Cpub))

	if pubKey == nil {
		if wt.Result == "invalid" || wt.Result == "acceptable" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - ASN decoding failed: want %v\n",
			wt.TCID, wt.Comment, wt.Flags, wt.Result)
		return false
	}

	privGroup := C.EC_KEY_get0_group(privKey)

	secLen := (C.EC_GROUP_get_degree(privGroup) + 7) / 8

	secret := make([]byte, secLen)
	if secLen == 0 {
		secret = append(secret, 0)
	}

	pubPoint := C.EC_KEY_get0_public_key(pubKey)

	ret = C.ECDH_compute_key(unsafe.Pointer(&secret[0]), C.ulong(secLen), pubPoint, privKey, nil)
	if ret != C.int(secLen) {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - ECDH_compute_key() = %d, want %d, result: %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, int(secLen), wt.Result)
		return false
	}

	shared, err := hex.DecodeString(wt.Shared)
	if err != nil {
		log.Fatalf("Failed to decode shared secret: %v", err)
	}

	success := true
	if !bytes.Equal(shared, secret) {
		fmt.Printf("FAIL: Test case %d (%q) %v - expected and computed shared secret do not match, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, wt.Result)
		success = false
	}
	if acceptableAudit && success && wt.Result == "acceptable" {
		gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
	}
	return success
}

func runECDHTestGroup(algorithm string, wtg *wycheproofTestGroupECDH) bool {
	doECpoint := false
	if wtg.Encoding == "ecpoint" {
		doECpoint = true
	}

	fmt.Printf("Running %v test group %v with curve %v and %v encoding...\n",
		algorithm, wtg.Type, wtg.Curve, wtg.Encoding)

	nid, err := nidFromString(wtg.Curve)
	if err != nil {
		log.Fatalf("Failed to get nid for curve: %v", err)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runECDHTest(nid, doECpoint, wt) {
			success = false
		}
	}
	return success
}

func runECDHWebCryptoTest(nid int, wt *wycheproofTestECDHWebCrypto) bool {
	privKey := C.EC_KEY_new_by_curve_name(C.int(nid))
	if privKey == nil {
		log.Fatalf("EC_KEY_new_by_curve_name failed")
	}
	defer C.EC_KEY_free(privKey)

	d, err := base64.RawURLEncoding.DecodeString(wt.Private.D)
	if err != nil {
		log.Fatalf("Failed to base64 decode d: %v", err)
	}
	bnD := C.BN_bin2bn((*C.uchar)(unsafe.Pointer(&d[0])), C.int(len(d)), nil)
	if bnD == nil {
		log.Fatal("Failed to decode D")
	}
	defer C.BN_free(bnD)

	ret := C.EC_KEY_set_private_key(privKey, bnD)
	if ret != 1 {
		fmt.Printf("FAIL: Test case %d (%q) %v - EC_KEY_set_private_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}

	x, err := base64.RawURLEncoding.DecodeString(wt.Public.X)
	if err != nil {
		log.Fatalf("Failed to base64 decode x: %v", err)
	}
	bnX := C.BN_bin2bn((*C.uchar)(unsafe.Pointer(&x[0])), C.int(len(x)), nil)
	if bnX == nil {
		log.Fatal("Failed to decode X")
	}
	defer C.BN_free(bnX)

	y, err := base64.RawURLEncoding.DecodeString(wt.Public.Y)
	if err != nil {
		log.Fatalf("Failed to base64 decode y: %v", err)
	}
	bnY := C.BN_bin2bn((*C.uchar)(unsafe.Pointer(&y[0])), C.int(len(y)), nil)
	if bnY == nil {
		log.Fatal("Failed to decode Y")
	}
	defer C.BN_free(bnY)

	pubKey := C.EC_KEY_new_by_curve_name(C.int(nid))
	if pubKey == nil {
		log.Fatal("Failed to create EC_KEY")
	}
	defer C.EC_KEY_free(pubKey)

	ret = C.EC_KEY_set_public_key_affine_coordinates(pubKey, bnX, bnY)
	if ret != 1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - EC_KEY_set_public_key_affine_coordinates() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, wt.Result)
		return false
	}
	pubPoint := C.EC_KEY_get0_public_key(pubKey)

	privGroup := C.EC_KEY_get0_group(privKey)

	secLen := (C.EC_GROUP_get_degree(privGroup) + 7) / 8

	secret := make([]byte, secLen)
	if secLen == 0 {
		secret = append(secret, 0)
	}

	ret = C.ECDH_compute_key(unsafe.Pointer(&secret[0]), C.ulong(secLen), pubPoint, privKey, nil)
	if ret != C.int(secLen) {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - ECDH_compute_key() = %d, want %d, result: %v\n",
			wt.TCID, wt.Comment, wt.Flags, ret, int(secLen), wt.Result)
		return false
	}

	shared, err := hex.DecodeString(wt.Shared)
	if err != nil {
		log.Fatalf("Failed to decode shared secret: %v", err)
	}

	success := true
	if !bytes.Equal(shared, secret) {
		fmt.Printf("FAIL: Test case %d (%q) %v - expected and computed shared secret do not match, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, wt.Result)
		success = false
	}
	if acceptableAudit && success && wt.Result == "acceptable" {
		gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
	}
	return success
}

func runECDHWebCryptoTestGroup(algorithm string, wtg *wycheproofTestGroupECDHWebCrypto) bool {
	fmt.Printf("Running %v test group %v with curve %v and %v encoding...\n",
		algorithm, wtg.Type, wtg.Curve, wtg.Encoding)

	nid, err := nidFromString(wtg.Curve)
	if err != nil {
		log.Fatalf("Failed to get nid for curve: %v", err)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runECDHWebCryptoTest(nid, wt) {
			success = false
		}
	}
	return success
}

func runECDSATest(ecKey *C.EC_KEY, nid int, h hash.Hash, webcrypto bool, wt *wycheproofTestECDSA) bool {
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode message %q: %v", wt.Msg, err)
	}

	h.Reset()
	h.Write(msg)
	msg = h.Sum(nil)

	msgLen := len(msg)
	if msgLen == 0 {
		msg = append(msg, 0)
	}

	var ret C.int
	if webcrypto {
		cDer, derLen := encodeECDSAWebCryptoSig(wt.Sig)
		if cDer == nil {
			fmt.Print("FAIL: unable to decode signature")
			return false
		}
		defer C.free(unsafe.Pointer(cDer))

		ret = C.ECDSA_verify(0, (*C.uchar)(unsafe.Pointer(&msg[0])), C.int(msgLen),
			(*C.uchar)(unsafe.Pointer(cDer)), C.int(derLen), ecKey)
	} else {
		sig, err := hex.DecodeString(wt.Sig)
		if err != nil {
			log.Fatalf("Failed to decode signature %q: %v", wt.Sig, err)
		}

		sigLen := len(sig)
		if sigLen == 0 {
			sig = append(sig, 0)
		}
		ret = C.ECDSA_verify(0, (*C.uchar)(unsafe.Pointer(&msg[0])), C.int(msgLen),
			(*C.uchar)(unsafe.Pointer(&sig[0])), C.int(sigLen), ecKey)
	}

	// XXX audit acceptable cases...
	success := true
	if ret == 1 != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d (%q) %v - ECDSA_verify() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
		success = false
	}
	if acceptableAudit && ret == 1 && wt.Result == "acceptable" {
		gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
	}
	return success
}

func runECDSATestGroup(algorithm string, wtg *wycheproofTestGroupECDSA) bool {
	fmt.Printf("Running %v test group %v with curve %v, key size %d and %v...\n",
		algorithm, wtg.Type, wtg.Key.Curve, wtg.Key.KeySize, wtg.SHA)

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
		if !runECDSATest(ecKey, nid, h, false, wt) {
			success = false
		}
	}
	return success
}

// DER encode the signature (so that ECDSA_verify() can decode and encode it again...)
func encodeECDSAWebCryptoSig(wtSig string) (*C.uchar, C.int) {
	cSig := C.ECDSA_SIG_new()
	if cSig == nil {
		log.Fatal("ECDSA_SIG_new() failed")
	}
	defer C.ECDSA_SIG_free(cSig)

	sigLen := len(wtSig)
	r := C.CString(wtSig[:sigLen/2])
	s := C.CString(wtSig[sigLen/2:])
	defer C.free(unsafe.Pointer(r))
	defer C.free(unsafe.Pointer(s))
	if C.BN_hex2bn(&cSig.r, r) == 0 {
		return nil, 0
	}
	if C.BN_hex2bn(&cSig.s, s) == 0 {
		return nil, 0
	}

	derLen := C.i2d_ECDSA_SIG(cSig, nil)
	if derLen == 0 {
		return nil, 0
	}
	cDer := (*C.uchar)(C.malloc(C.ulong(derLen)))
	if cDer == nil {
		log.Fatal("malloc failed")
	}

	p := cDer
	ret := C.i2d_ECDSA_SIG(cSig, (**C.uchar)(&p))
	if ret == 0 || ret != derLen {
		C.free(unsafe.Pointer(cDer))
		return nil, 0
	}

	return cDer, derLen
}

func runECDSAWebCryptoTestGroup(algorithm string, wtg *wycheproofTestGroupECDSAWebCrypto) bool {
	fmt.Printf("Running %v test group %v with curve %v, key size %d and %v...\n",
		algorithm, wtg.Type, wtg.Key.Curve, wtg.Key.KeySize, wtg.SHA)

	nid, err := nidFromString(wtg.JWK.Crv)
	if err != nil {
		log.Fatalf("Failed to get nid for curve: %v", err)
	}
	ecKey := C.EC_KEY_new_by_curve_name(C.int(nid))
	if ecKey == nil {
		log.Fatal("EC_KEY_new_by_curve_name failed")
	}
	defer C.EC_KEY_free(ecKey)

	x, err := base64.RawURLEncoding.DecodeString(wtg.JWK.X)
	if err != nil {
		log.Fatalf("Failed to base64 decode X: %v", err)
	}
	bnX := C.BN_bin2bn((*C.uchar)(unsafe.Pointer(&x[0])), C.int(len(x)), nil)
	if bnX == nil {
		log.Fatal("Failed to decode X")
	}
	defer C.BN_free(bnX)

	y, err := base64.RawURLEncoding.DecodeString(wtg.JWK.Y)
	if err != nil {
		log.Fatalf("Failed to base64 decode Y: %v", err)
	}
	bnY := C.BN_bin2bn((*C.uchar)(unsafe.Pointer(&y[0])), C.int(len(y)), nil)
	if bnY == nil {
		log.Fatal("Failed to decode Y")
	}
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
		if !runECDSATest(ecKey, nid, h, true, wt) {
			success = false
		}
	}
	return success
}

func runKWTestWrap(keySize int, key []byte, keyLen int, msg []byte, msgLen int, ct []byte, ctLen int, wt *wycheproofTestKW) bool {
	var aesKey C.AES_KEY

	ret := C.AES_set_encrypt_key((*C.uchar)(unsafe.Pointer(&key[0])), (C.int)(keySize), (*C.AES_KEY)(unsafe.Pointer(&aesKey)))
	if ret != 0 {
		fmt.Printf("FAIL: Test case %d (%q) %v - AES_set_encrypt_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
		return false
	}

	outLen := msgLen
	out := make([]byte, outLen)
	copy(out, msg)
	out = append(out, make([]byte, 8)...)
	ret = C.AES_wrap_key((*C.AES_KEY)(unsafe.Pointer(&aesKey)), nil, (*C.uchar)(unsafe.Pointer(&out[0])), (*C.uchar)(unsafe.Pointer(&out[0])), (C.uint)(msgLen))
	success := false
	if ret == C.int(len(out)) && bytes.Equal(out, ct) {
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
		if wt.Result != "invalid" {
			success = true
		}
	} else if wt.Result != "valid" {
		success = true
	}
	if !success {
		fmt.Printf("FAIL: Test case %d (%q) %v - msgLen = %d, AES_wrap_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, msgLen, int(ret), wt.Result)
	}
	return success
}

func runKWTestUnWrap(keySize int, key []byte, keyLen int, msg []byte, msgLen int, ct []byte, ctLen int, wt *wycheproofTestKW) bool {
	var aesKey C.AES_KEY

	ret := C.AES_set_decrypt_key((*C.uchar)(unsafe.Pointer(&key[0])), (C.int)(keySize), (*C.AES_KEY)(unsafe.Pointer(&aesKey)))
	if ret != 0 {
		fmt.Printf("FAIL: Test case %d (%q) %v - AES_set_encrypt_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
		return false
	}

	out := make([]byte, ctLen)
	copy(out, ct)
	if ctLen == 0 {
		out = append(out, 0)
	}
	ret = C.AES_unwrap_key((*C.AES_KEY)(unsafe.Pointer(&aesKey)), nil, (*C.uchar)(unsafe.Pointer(&out[0])), (*C.uchar)(unsafe.Pointer(&out[0])), (C.uint)(ctLen))
	success := false
	if ret == C.int(ctLen - 8) && bytes.Equal(out[0:ret], msg[0:ret]) {
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
		if wt.Result != "invalid" {
			success = true
		}
	} else if wt.Result != "valid" {
		success = true
	}
	if !success {
		fmt.Printf("FAIL: Test case %d (%q) %v - keyLen = %d, AES_unwrap_key() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, keyLen, int(ret), wt.Result)
	}
	return success
}

func runKWTest(keySize int, wt *wycheproofTestKW) bool {
	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode msg %q: %v", wt.Msg, err)
	}
	ct, err := hex.DecodeString(wt.CT)
	if err != nil {
		log.Fatalf("Failed to decode ct %q: %v", wt.CT, err)
	}

	keyLen, msgLen, ctLen := len(key), len(msg), len(ct)

	if keyLen == 0 {
		key = append(key, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}
	if ctLen == 0 {
		ct = append(ct, 0)
	}

	wrapSuccess := runKWTestWrap(keySize, key, keyLen, msg, msgLen, ct, ctLen, wt)
	unwrapSuccess := runKWTestUnWrap(keySize, key, keyLen, msg, msgLen, ct, ctLen, wt)

	return wrapSuccess && unwrapSuccess
}

func runKWTestGroup(algorithm string, wtg *wycheproofTestGroupKW) bool {
	fmt.Printf("Running %v test group %v with key size %d...\n",
		algorithm, wtg.Type, wtg.KeySize)

	success := true
	for _, wt := range wtg.Tests {
		if !runKWTest(wtg.KeySize, wt) {
			success = false
		}
	}
	return success
}

func runRSASSATest(rsa *C.RSA, h hash.Hash, sha *C.EVP_MD, mgfSha *C.EVP_MD, sLen int, wt *wycheproofTestRSASSA) bool {
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

	sigOut := make([]byte, C.RSA_size(rsa) - 11)
	if sigLen == 0 {
		sigOut = append(sigOut, 0)
	}

	ret := C.RSA_public_decrypt(C.int(sigLen), (*C.uchar)(unsafe.Pointer(&sig[0])),
		(*C.uchar)(unsafe.Pointer(&sigOut[0])), rsa, C.RSA_NO_PADDING)
	if ret == -1 {
		if wt.Result == "invalid" {
			return true
		}
		fmt.Printf("FAIL: Test case %d (%q) %v - RSA_public_decrypt() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
		return false
	}

	ret = C.RSA_verify_PKCS1_PSS_mgf1(rsa, (*C.uchar)(unsafe.Pointer(&msg[0])), sha, mgfSha,
		(*C.uchar)(unsafe.Pointer(&sigOut[0])), C.int(sLen))

	success := false
	if ret == 1 && (wt.Result == "valid" || wt.Result == "acceptable") {
		// All acceptable cases that pass use SHA-1 and are flagged:
		// "WeakHash" : "The key for this test vector uses a weak hash function."
		if acceptableAudit && wt.Result == "acceptable" {
			gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
		}
		success = true
	} else if ret == 0 && (wt.Result == "invalid" || wt.Result == "acceptable") {
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) %v - RSA_verify_PKCS1_PSS_mgf1() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
	}
	return success
}

func runRSASSATestGroup(algorithm string, wtg *wycheproofTestGroupRSASSA) bool {
	fmt.Printf("Running %v test group %v with key size %d and %v...\n",
		algorithm, wtg.Type, wtg.KeySize, wtg.SHA)
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

	h, err := hashFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get hash: %v", err)
	}

	sha, err := hashEvpMdFromString(wtg.SHA)
	if err != nil {
		log.Fatalf("Failed to get hash: %v", err)
	}

	mgfSha, err := hashEvpMdFromString(wtg.MGFSHA)
	if err != nil {
		log.Fatalf("Failed to get MGF hash: %v", err)
	}

	success := true
	for _, wt := range wtg.Tests {
		if !runRSASSATest(rsa, h, sha, mgfSha, wtg.SLen, wt) {
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
	if ret == 1 != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d (%q) %v - RSA_verify() = %d, want %v\n",
			wt.TCID, wt.Comment, wt.Flags, int(ret), wt.Result)
		success = false
	}
	if acceptableAudit && ret == 1 && wt.Result == "acceptable" {
		gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
	}
	return success
}

func runRSATestGroup(algorithm string, wtg *wycheproofTestGroupRSA) bool {
	fmt.Printf("Running %v test group %v with key size %d and %v...\n",
		algorithm, wtg.Type, wtg.KeySize, wtg.SHA)

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
		fmt.Printf("FAIL: Test case %d (%q) %v - X25519(), want %v\n",
			wt.TCID, wt.Comment, wt.Flags, wt.Result)
		success = false
	}
	if acceptableAudit && result && wt.Result == "acceptable" {
		gatherAcceptableStatistics(wt.TCID, wt.Comment, wt.Flags)
	}
	return success
}

func runX25519TestGroup(algorithm string, wtg *wycheproofTestGroupX25519) bool {
	fmt.Printf("Running %v test group with curve %v...\n", algorithm, wtg.Curve)

	success := true
	for _, wt := range wtg.Tests {
		if !runX25519Test(wt) {
			success = false
		}
	}
	return success
}

func runTestVectors(path string, webcrypto bool) bool {
	b, err := ioutil.ReadFile(path)
	if err != nil {
		log.Fatalf("Failed to read test vectors: %v", err)
	}
	wtv := &wycheproofTestVectors{}
	if err := json.Unmarshal(b, wtv); err != nil {
		log.Fatalf("Failed to unmarshal JSON: %v", err)
	}
	fmt.Printf("Loaded Wycheproof test vectors for %v with %d tests from %q\n",
		wtv.Algorithm, wtv.NumberOfTests, filepath.Base(path))

	var wtg interface{}
	switch wtv.Algorithm {
	case "AES-CBC-PKCS5":
		wtg = &wycheproofTestGroupAesCbcPkcs5{}
	case "AES-CCM":
		wtg = &wycheproofTestGroupAead{}
	case "AES-CMAC":
		wtg = &wycheproofTestGroupAesCmac{}
	case "AES-GCM":
		wtg = &wycheproofTestGroupAead{}
	case "CHACHA20-POLY1305":
		wtg = &wycheproofTestGroupAead{}
	case "DSA":
		wtg = &wycheproofTestGroupDSA{}
	case "ECDH":
		if webcrypto {
			wtg = &wycheproofTestGroupECDHWebCrypto{}
		} else {
			wtg = &wycheproofTestGroupECDH{}
		}
	case "ECDSA":
		if webcrypto {
			wtg = &wycheproofTestGroupECDSAWebCrypto{}
		} else {
			wtg = &wycheproofTestGroupECDSA{}
		}
	case "KW":
		wtg = &wycheproofTestGroupKW{}
	case "RSASSA-PSS":
		wtg = &wycheproofTestGroupRSASSA{}
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
			if !runAesCbcPkcs5TestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupAesCbcPkcs5)) {
				success = false
			}
		case "AES-CCM":
			if !runAesAeadTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupAead)) {
				success = false
			}
		case "AES-CMAC":
			if !runAesCmacTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupAesCmac)) {
				success = false
			}
		case "AES-GCM":
			if !runAesAeadTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupAead)) {
				success = false
			}
		case "CHACHA20-POLY1305":
			if !runChaCha20Poly1305TestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupAead)) {
				success = false
			}
		case "DSA":
			if !runDSATestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupDSA)) {
				success = false
			}
		case "ECDH":
			if webcrypto {
				if !runECDHWebCryptoTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupECDHWebCrypto)) {
					success = false
				}
			} else {
				if !runECDHTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupECDH)) {
					success = false
				}
			}
		case "ECDSA":
			if webcrypto {
				if !runECDSAWebCryptoTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupECDSAWebCrypto)) {
					success = false
				}
			} else {
				if !runECDSATestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupECDSA)) {
					success = false
				}
			}
		case "KW":
			if !runKWTestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupKW)) {
				success = false
			}
		case "RSASSA-PSS":
			if !runRSASSATestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupRSASSA)) {
				success = false
			}
		case "RSASig":
			if !runRSATestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupRSA)) {
				success = false
			}
		case "X25519":
			if !runX25519TestGroup(wtv.Algorithm, wtg.(*wycheproofTestGroupX25519)) {
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
		fmt.Printf("SKIPPED\n")
		os.Exit(0)
	}

	flag.BoolVar(&acceptableAudit, "v", false, "audit acceptable cases")
	flag.Parse()

	acceptableComments = make(map[string]int)
	acceptableFlags = make(map[string]int)

	tests := []struct {
		name    string
		pattern string
	}{
		{"AES", "aes_[cg]*[^xv]_test.json"}, // Skip AES-EAX, AES-GCM-SIV and AES-SIV-CMAC.
		{"ChaCha20-Poly1305", "chacha20_poly1305_test.json"},
		{"DSA", "dsa_test.json"},
		{"ECDH", "ecdh_[^w]*test.json"},
		{"ECDHWebCrypto", "ecdh_w*_test.json"},
		{"ECDSA", "ecdsa_[^w]*test.json"},
		{"ECDSAWebCrypto", "ecdsa_w*_test.json"},
		{"KW", "kw_test.json"},
		{"RSA", "rsa_*test.json"},
		{"X25519", "x25519_*test.json"},
	}

	success := true

	for _, test := range tests {
		webcrypto := test.name == "ECDSAWebCrypto" || test.name == "ECDHWebCrypto"
		tvs, err := filepath.Glob(filepath.Join(testVectorPath, test.pattern))
		if err != nil {
			log.Fatalf("Failed to glob %v test vectors: %v", test.name, err)
		}
		if len(tvs) == 0 {
			log.Fatalf("Failed to find %v test vectors at %q\n", test.name, testVectorPath)
		}
		for _, tv := range tvs {
			if !runTestVectors(tv, webcrypto) {
				success = false
			}
		}
	}

	if acceptableAudit {
		printAcceptableStatistics()
	}

	if !success {
		os.Exit(1)
	}
}
