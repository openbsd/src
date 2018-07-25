/* $OpenBSD: wycheproof.go,v 1.1 2018/07/25 18:04:09 jsing Exp $ */
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
#include <openssl/objects.h>
#include <openssl/rsa.h>
*/
import "C"

import (
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

type wycheproofTest struct {
	TCID    int      `json:"tcId"`
	Comment string   `json:"comment"`
	Msg     string   `json:"msg"`
	Sig     string   `json:"sig"`
	Padding string   `json:"padding"`
	Result  string   `json:"result"`
	Flags   []string `json:"flags"`
}

type wycheproofTestGroup struct {
	E       string            `json:"e"`
	KeyASN  string            `json:"keyAsn"`
	KeyDER  string            `json:"keyDer"`
	KeyPEM  string            `json:"keyPem"`
	KeySize int               `json:"keysize"`
	N       string            `json:"n"`
	SHA     string            `json:"sha"`
	Type    string            `json:"type"`
	Tests   []*wycheproofTest `json:"tests"`
}

type wycheproofTestVectors struct {
	Algorithm        string            `json:"algorithm"`
	GeneratorVersion string            `json:"generatorVersion"`
	Notes            map[string]string `json:"notes"`
	NumberOfTests    int               `json:"numberOfTests"`
	// Header
	TestGroups []*wycheproofTestGroup `json:"testGroups"`
}

func nidFromString(ns string) (int, error) {
	switch ns {
	case "SHA-1":
		return C.NID_sha1, nil
	case "SHA-224":
		return C.NID_sha224, nil
	case "SHA-256":
		return C.NID_sha256, nil
	case "SHA-384":
		return C.NID_sha384, nil
	case "SHA-512":
		return C.NID_sha512, nil
	default:
		return -1, fmt.Errorf("unknown NID %q", ns)
	}
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

func runRSATest(rsa *C.RSA, nid int, h hash.Hash, wt *wycheproofTest) bool {
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
	succeeded := true
	if (ret == 1) != (wt.Result == "valid") && wt.Result != "acceptable" {
		fmt.Printf("FAIL: Test case %d - RSA_verify() = %d, want %v\n", wt.TCID, int(ret), wt.Result)
		succeeded = false
	}
	return succeeded
}

func runRSATestGroup(wtg *wycheproofTestGroup) bool {
	fmt.Printf("Running RSA test group %v with key size %d and %v...\n", wtg.Type, wtg.KeySize, wtg.SHA)

	rsa := C.RSA_new()
	if rsa == nil {
		log.Fatal("RSA_new failed")
	}
	defer C.RSA_free(rsa)

	e := C.CString(wtg.E)
	if C.BN_hex2bn(&rsa.e, e) == 0 {
		log.Fatalf("Failed to set RSA e")
	}
	C.free(unsafe.Pointer(e))

	n := C.CString(wtg.N)
	if C.BN_hex2bn(&rsa.n, n) == 0 {
		log.Fatalf("Failed to set RSA n")
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

	succeeded := true
	for _, wt := range wtg.Tests {
		if !runRSATest(rsa, nid, h, wt) {
			succeeded = false
		}
	}
	return succeeded
}

func runRSATestVectors(path string) bool {
	b, err := ioutil.ReadFile(path)
	if err != nil {
		log.Fatalf("Failed to read test vectors: %v", err)
	}
	wtv := &wycheproofTestVectors{}
	if err := json.Unmarshal(b, wtv); err != nil {
		log.Fatalf("Failed to unmarshal JSON: %v", err)
	}
	fmt.Printf("Loaded Wycheproof test vectors for %v with %d tests\n", wtv.Algorithm, wtv.NumberOfTests)

	succeeded := true
	for _, wtg := range wtv.TestGroups {
		if !runRSATestGroup(wtg) {
			succeeded = false
		}
	}
	return succeeded
}

func main() {
	if _, err := os.Stat(testVectorPath); os.IsNotExist(err) {
		fmt.Printf("package wycheproof-testvectors is required for this regress\n")
		fmt.Printf("SKIPPING\n")
		os.Exit(0)
	}

	tvs, err := filepath.Glob(filepath.Join(testVectorPath, "*.json"))
	if err != nil || len(tvs) == 0 {
		log.Fatalf("Failed to find test vectors at %q\n", testVectorPath)
	}

	succeeded := true

	tvs, err = filepath.Glob(filepath.Join(testVectorPath, "rsa_signature_*test.json"))
	if err != nil {
		log.Fatalf("Failed to find RSA test vectors: %v", err)
	}
	for _, tv := range tvs {
		if !runRSATestVectors(tv) {
			succeeded = false
		}
	}

	if !succeeded {
		os.Exit(1)
	}
}
