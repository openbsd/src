/* $OpenBSD: wycheproof.go,v 1.6 2018/08/20 17:06:18 tb Exp $ */
/*
 * Copyright (c) 2018 Joel Sing <jsing@openbsd.org>
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

#include <openssl/bn.h>
#include <openssl/curve25519.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>
#include <openssl/objects.h>
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

func runChaCha20Poly1305Test(iv_len int, key_len int, tag_len int, wt *wycheproofTestChaCha20Poly1305) bool {
	aead := C.EVP_aead_chacha20_poly1305()
	if aead == nil {
		log.Fatal("EVP_aead_chacha20_poly1305 failed")
	}

	key, err := hex.DecodeString(wt.Key)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.Key, err)
	}
	if key_len != len(key) {
		log.Fatalf("Key length mismatch: got %d, want %d", len(key), key_len)
	}

	var ctx C.EVP_AEAD_CTX
	if C.EVP_AEAD_CTX_init((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)), aead, (*C.uchar)(unsafe.Pointer(&key[0])), C.size_t(key_len), C.size_t(tag_len), nil) != 1 {
		log.Fatalf("Failed to initialize AEAD context")
	}

	iv, err := hex.DecodeString(wt.IV)
	if err != nil {
		log.Fatalf("Failed to decode key %q: %v", wt.IV, err)
	}
	if iv_len != len(iv) {
		log.Fatalf("IV length mismatch: got %d, want %d", len(iv), iv_len)
	}
	aad, err := hex.DecodeString(wt.AAD)
	if err != nil {
		log.Fatalf("Failed to decode AAD %q: %v", wt.AAD, err)
	}
	msg, err := hex.DecodeString(wt.Msg)
	if err != nil {
		log.Fatalf("Failed to decode msg %q: %v", wt.Msg, err)
	}

	ivLen, aadLen, msgLen := len(iv), len(aad), len(msg)
	if ivLen == 0 {
		iv = append(iv, 0)
	}
	if aadLen == 0 {
		aad = append(aad, 0)
	}
	if msgLen == 0 {
		msg = append(msg, 0)
	}

	maxOutLen := msgLen + tag_len
	out := make([]byte, maxOutLen)
	var outLen C.size_t

	ret := C.EVP_AEAD_CTX_seal((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)), (*C.uint8_t)(unsafe.Pointer(&out[0])), (*C.size_t)(unsafe.Pointer(&outLen)), C.size_t(maxOutLen), (*C.uint8_t)(unsafe.Pointer(&iv[0])), C.size_t(ivLen), (*C.uint8_t)(unsafe.Pointer(&msg[0])), C.size_t(msgLen), (*C.uint8_t)(unsafe.Pointer(&aad[0])), C.size_t(aadLen))

	C.EVP_AEAD_CTX_cleanup((*C.EVP_AEAD_CTX)(unsafe.Pointer(&ctx)))

	if ret != 1 && wt.Result == "invalid" {
		fmt.Printf("INFO: Test case %d (%q) - EVP_AEAD_CTX_seal() = %d, want %v\n", wt.TCID, wt.Comment, int(ret), wt.Result)
		return true
	}

	if (outLen != C.size_t(maxOutLen)) {
		fmt.Printf("FAIL: Test case %d (%q) - ChaCha output length mismatch: got %d, want %d", wt.TCID, wt.Comment, outLen, maxOutLen)
		return false
	}
	
	outCt := out[0:msgLen]
	outTag := out[msgLen: maxOutLen]

	ct, err := hex.DecodeString(wt.CT)
	if err != nil {
		log.Fatalf("Failed to decode ct %q: %v", wt.CT, err)
	}
	if len(ct) != msgLen {
		fmt.Printf("FAIL: Test case %d (%q) - msg length %d doesn't match ct length %d", wt.TCID, wt.Comment, msgLen, len(ct))
		return false
	}
	tag, err := hex.DecodeString(wt.Tag)
	if err != nil {
		log.Fatalf("Failed to decode tag %q: %v", wt.Tag, err)
	}
	if len(tag) != tag_len {
		fmt.Printf("FAIL: Test case %d (%q) tag length: got %d, want %d", wt.TCID, wt.Comment, len(tag), tag_len)
		return false
	}

	success := false
	if (bytes.Equal(outCt, ct) && bytes.Equal(outTag, tag)) || wt.Result == "invalid" {
		success = true
	} else {
		fmt.Printf("FAIL: Test case %d (%q) - EVP_AEAD_CTX_seal() = %d, ct match: %t, tag match: %t; want %v\n", wt.TCID, wt.Comment, int(ret), bytes.Equal(outCt, ct), bytes.Equal(outTag, tag), wt.Result)
	}

	return success
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
	case "CHACHA20-POLY1305":
		wtg = &wycheproofTestGroupChaCha20Poly1305{}
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
		case "CHACHA20-POLY1305":
			if !runChaCha20Poly1305TestGroup(wtg.(*wycheproofTestGroupChaCha20Poly1305)) {
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

	// AES, Chacha20Poly1305, DSA, ECDH
	tests := []struct {
		name    string
		pattern string
	}{
		{"ChaCha20-Poly1305", "chacha20_poly1305_test.json"},
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
