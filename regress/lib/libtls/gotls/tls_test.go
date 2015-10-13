package tls

import (
	"encoding/pem"
	"fmt"
	"io/ioutil"
	"net/http"
	"net/http/httptest"
	"net/url"
	"os"
	"strings"
	"testing"
	"time"
)

const (
	httpContent = "Hello, TLS!"

	certHash = "SHA256:448f628a8a65aa18560e53a80c53acb38c51b427df0334082349141147dc9bf6"
)

var (
	certNotBefore = time.Unix(0, 0)
	certNotAfter = certNotBefore.Add(1000000 * time.Hour)
)

// createCAFile writes a PEM encoded version of the certificate out to a
// temporary file, for use by libtls.
func createCAFile(cert []byte) (string, error) {
	f, err := ioutil.TempFile("", "tls")
	if err != nil {
		return "", fmt.Errorf("failed to create file: %v", err)
	}
	defer f.Close()
	block := &pem.Block{
		Type:  "CERTIFICATE",
		Bytes: cert,
	}
	if err := pem.Encode(f, block); err != nil {
		return "", fmt.Errorf("failed to encode certificate: %v", err)
	}
	return f.Name(), nil
}

func newTestServer() (*httptest.Server, *url.URL, string, error) {
	ts := httptest.NewTLSServer(
		http.HandlerFunc(
			func(w http.ResponseWriter, r *http.Request) {
				fmt.Fprintln(w, httpContent)
			},
		),
	)

	u, err := url.Parse(ts.URL)
	if err != nil {
		return nil, nil, "", fmt.Errorf("failed to parse URL %q: %v", ts.URL, err)
	}

	caFile, err := createCAFile(ts.TLS.Certificates[0].Certificate[0])
	if err != nil {
		return nil, nil, "", fmt.Errorf("failed to create CA file: %v", err)
	}

	return ts, u, caFile, nil
}

func TestTLSBasic(t *testing.T) {
	ts, u, caFile, err := newTestServer()
	if err != nil {
		t.Fatalf("Failed to start test server: %v", err)
	}
	defer os.Remove(caFile)
	defer ts.Close()

	if err := Init(); err != nil {
		t.Fatal(err)
	}

	cfg, err := NewConfig()
	if err != nil {
		t.Fatal(err)
	}
	defer cfg.Free()
	cfg.SetCAFile(caFile)

	tls, err := NewClient(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer tls.Free()

	t.Logf("Connecting to %s", u.Host)

	if err := tls.Connect(u.Host, ""); err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := tls.Close(); err != nil {
			t.Fatalf("Close failed: %v", err)
		}
	}()

	n, err := tls.Write([]byte("GET / HTTP/1.0\n\n"))
	if err != nil {
		t.Fatal(err)
	}
	t.Logf("Wrote %d bytes...", n)

	buf := make([]byte, 1024)
	n, err = tls.Read(buf)
	if err != nil {
		t.Fatal(err)
	}
	t.Logf("Read %d bytes...", n)

	if !strings.Contains(string(buf), httpContent) {
		t.Errorf("Response does not contain %q", httpContent)
	}
}

func TestTLSSingleByteReadWrite(t *testing.T) {
	ts, u, caFile, err := newTestServer()
	if err != nil {
		t.Fatalf("Failed to start test server: %v", err)
	}
	defer os.Remove(caFile)
	defer ts.Close()

	if err := Init(); err != nil {
		t.Fatal(err)
	}

	cfg, err := NewConfig()
	if err != nil {
		t.Fatal(err)
	}
	defer cfg.Free()
	cfg.SetCAFile(caFile)

	tls, err := NewClient(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer tls.Free()

	t.Logf("Connecting to %s", u.Host)

	if err := tls.Connect(u.Host, ""); err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := tls.Close(); err != nil {
			t.Fatalf("Close failed: %v", err)
		}
	}()

	for _, b := range []byte("GET / HTTP/1.0\n\n") {
		n, err := tls.Write([]byte{b})
		if err != nil {
			t.Fatal(err)
		}
		if n != 1 {
			t.Fatalf("Wrote byte %v, got length %d, want 1", b, n)
		}
	}

	var body []byte
	for {
		buf := make([]byte, 1)
		n, err := tls.Read(buf)
		if err != nil {
			t.Fatal(err)
		}
		if n == 0 {
			break
		}
		if n != 1 {
			t.Fatalf("Read single byte, got length %d, want 1", n)
		}
		body = append(body, buf...)
	}

	if !strings.Contains(string(body), httpContent) {
		t.Errorf("Response does not contain %q", httpContent)
	}
}

func TestTLSInfo(t *testing.T) {
	ts, u, caFile, err := newTestServer()
	if err != nil {
		t.Fatalf("Failed to start test server: %v", err)
	}
	defer os.Remove(caFile)
	defer ts.Close()

	if err := Init(); err != nil {
		t.Fatal(err)
	}

	cfg, err := NewConfig()
	if err != nil {
		t.Fatal(err)
	}
	defer cfg.Free()
	cfg.SetCAFile(caFile)

	tls, err := NewClient(cfg)
	if err != nil {
		t.Fatal(err)
	}
	defer tls.Free()

	t.Logf("Connecting to %s", u.Host)

	if err := tls.Connect(u.Host, ""); err != nil {
		t.Fatal(err)
	}
	defer func() {
		if err := tls.Close(); err != nil {
			t.Fatalf("Close failed: %v", err)
		}
	}()

	// All of these should fail since the handshake has not completed.
	if _, err := tls.ConnVersion(); err == nil {
		t.Error("ConnVersion() return nil error, want error")
	}
	if _, err := tls.ConnCipher(); err == nil {
		t.Error("ConnCipher() return nil error, want error")
	}

	if got, want := tls.PeerCertProvided(), false; got != want {
		t.Errorf("PeerCertProvided() = %v, want %v", got, want)
	}
	for _, name := range []string{"127.0.0.1", "::1", "example.com"} {
		if got, want := tls.PeerCertContainsName(name), false; got != want {
			t.Errorf("PeerCertContainsName(%q) = %v, want %v", name, got, want)
		}
	}

	if _, err := tls.PeerCertIssuer(); err == nil {
		t.Error("PeerCertIssuer() returned nil error, want error")
	}
	if _, err := tls.PeerCertSubject(); err == nil {
		t.Error("PeerCertSubject() returned nil error, want error")
	}
	if _, err := tls.PeerCertHash(); err == nil {
		t.Error("PeerCertHash() returned nil error, want error")
	}
	if _, err := tls.PeerCertNotBefore(); err == nil {
		t.Error("PeerCertNotBefore() returned nil error, want error")
	}
	if _, err := tls.PeerCertNotAfter(); err == nil {
		t.Error("PeerCertNotAfter() returned nil error, want error")
	}

	// Complete the handshake...
	if err := tls.Handshake(); err != nil {
		t.Fatalf("Handshake failed: %v", err)
	}

	if version, err := tls.ConnVersion(); err != nil {
		t.Errorf("ConnVersion() return error: %v", err)
	} else {
		t.Logf("Protocol version: %v", version)
	}
	if cipher, err := tls.ConnCipher(); err != nil {
		t.Errorf("ConnCipher() return error: %v", err)
	} else {
		t.Logf("Cipher: %v", cipher)
	}

	if got, want := tls.PeerCertProvided(), true; got != want {
		t.Errorf("PeerCertProvided() = %v, want %v", got, want)
	}
	for _, name := range []string{"127.0.0.1", "::1", "example.com"} {
		if got, want := tls.PeerCertContainsName(name), true; got != want {
			t.Errorf("PeerCertContainsName(%q) = %v, want %v", name, got, want)
		}
	}

	if issuer, err := tls.PeerCertIssuer(); err != nil {
		t.Errorf("PeerCertIssuer() returned error: %v", err)
	} else {
		t.Logf("Issuer: %v", issuer)
	}
	if subject, err := tls.PeerCertSubject(); err != nil {
		t.Errorf("PeerCertSubject() returned error: %v", err)
	} else {
		t.Logf("Subject: %v", subject)
	}
	if hash, err := tls.PeerCertHash(); err != nil {
		t.Errorf("PeerCertHash() returned error: %v", err)
	} else if hash != certHash {
		t.Errorf("Got cert hash %q, want %q", hash, certHash)
	} else {
		t.Logf("Hash: %v", hash)
	}
	if notBefore, err := tls.PeerCertNotBefore(); err != nil {
		t.Errorf("PeerCertNotBefore() returned error: %v", err)
	} else if !certNotBefore.Equal(notBefore) {
		t.Errorf("Got cert notBefore %v, want %v", notBefore.UTC(), certNotBefore.UTC())
	} else {
		t.Logf("NotBefore: %v", notBefore.UTC())
	}
	if notAfter, err := tls.PeerCertNotAfter(); err != nil {
		t.Errorf("PeerCertNotAfter() returned error: %v", err)
	} else if !certNotAfter.Equal(notAfter) {
		t.Errorf("Got cert notAfter %v, want %v", notAfter.UTC(), certNotAfter.UTC())
	} else {
		t.Logf("NotAfter: %v", notAfter.UTC())
	}
}
