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

const httpContent = "Hello, TLS!"

func TestTLSBasic(t *testing.T) {
	ts := httptest.NewTLSServer(
		http.HandlerFunc(
			func(w http.ResponseWriter, r *http.Request) {
				fmt.Fprintln(w, httpContent)
			},
		),
	)
	defer ts.Close()

	u, err := url.Parse(ts.URL)
	if err != nil {
		t.Fatalf("Failed to parse URL %q: %v", ts.URL, err)
	}

	caFile, err := createCAFile(ts.TLS.Certificates[0].Certificate[0])
	if err != nil {
		t.Fatalf("Failed to create CA file: %v", err)
	}
	defer os.Remove(caFile)

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
			t.Logf("Close failed: %v", err)
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
