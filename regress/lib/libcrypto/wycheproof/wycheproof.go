/* $OpenBSD: wycheproof.go,v 1.4 2018/08/10 16:18:55 jsing Exp $ */
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
	"SHA-1":   C.NID_sha1,
	"SHA-224": C.NID_sha224,
	"SHA-256": C.NID_sha256,
	"SHA-384": C.NID_sha384,
	"SHA-512": C.NID_sha512,
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

	// TODO: AES, Chacha20Poly1305, DSA, ECDH, ECDSA, RSA-PSS.
	tests := []struct {
		name    string
		pattern string
	}{
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
