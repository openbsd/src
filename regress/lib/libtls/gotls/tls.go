// Package tls provides a Go interface to the libtls library.
package tls

/*
#cgo LDFLAGS: -ltls -lssl -lcrypto

#include <stdlib.h>

#include <tls.h>

typedef void *tls;
*/
import "C"

import (
	"errors"
	"fmt"
	"unsafe"
)

// TLSConfig provides configuration options for a TLS context.
type TLSConfig struct {
	caFile *C.char
	tlsCfg *C.struct_tls_config
}

// TLS encapsulates the TLS context.
type TLS struct {
	cfg *TLSConfig
	ctx *C.struct_tls
}

// Init initialises the TLS library.
func Init() error {
	if C.tls_init() != 0 {
		return errors.New("initialisation failed")
	}
	return nil
}

// NewConfig returns a new TLS configuration.
func NewConfig() (*TLSConfig, error) {
	cfg := C.tls_config_new()
	if cfg == nil {
		return nil, errors.New("failed to allocate config")
	}
	return &TLSConfig{
		tlsCfg: cfg,
	}, nil
}

// SetCAFile sets the CA file to be used for connections.
func (c *TLSConfig) SetCAFile(filename string) {
	if c.caFile != nil {
		C.free(unsafe.Pointer(c.caFile))
	}
	c.caFile = C.CString(filename)
	C.tls_config_set_ca_file(c.tlsCfg, c.caFile)
}

// InsecureNoVerifyCert disables certificate verification for the connection.
func (c *TLSConfig) InsecureNoVerifyCert() {
	C.tls_config_insecure_noverifycert(c.tlsCfg)
}

// InsecureNoVerifyHost disables hostname verification for the connection.
func (c *TLSConfig) InsecureNoVerifyHost() {
	C.tls_config_insecure_noverifyhost(c.tlsCfg)
}

// SetSecure enables verification for the connection.
func (c *TLSConfig) SetVerify() {
	C.tls_config_verify(c.tlsCfg)
}

// Free frees resources associated with the TLS configuration.
func (c *TLSConfig) Free() {
	if c.tlsCfg == nil {
		return
	}
	C.tls_config_free(c.tlsCfg)
	c.tlsCfg = nil
}

// NewClient returns a new TLS client context, using the optional configuration.
// If no configuration is specified the default configuration will be used.
func NewClient(config *TLSConfig) (*TLS, error) {
	var sslCfg *C.struct_tls_config
	if config != nil {
		sslCfg = config.tlsCfg
	}
	ctx := C.tls_client()
	if ctx == nil {
		return nil, errors.New("tls client failed")
	}
	if C.tls_configure(ctx, sslCfg) != 0 {
		return nil, errors.New("tls configure failed")
	}
	return &TLS{
		cfg: config,
		ctx: ctx,
	}, nil
}

// Error returns the error message from the TLS context.
func (t *TLS) Error() string {
	if msg := C.tls_error(t.ctx); msg != nil {
		return C.GoString(msg)
	}
	return ""
}

// Connect attempts to establish an TLS connection to the specified host on
// the given port. The host may optionally contain a colon separated port
// value if the port string is specified as an empty string.
func (t *TLS) Connect(host, port string) error {
	h := C.CString(host)
	var p *C.char
	if port != "" {
		p = C.CString(port)
	}
	defer C.free(unsafe.Pointer(h))
	defer C.free(unsafe.Pointer(p))
	if C.tls_connect(t.ctx, h, p) != 0 {
		return fmt.Errorf("connect failed: %v", t.Error())
	}
	return nil
}

// Read reads data the TLS connection into the given buffer.
func (t *TLS) Read(buf []byte) (int, error) {
	var inlen C.size_t
	if C.tls_read(t.ctx, unsafe.Pointer(&buf[0]), C.size_t(len(buf)), (*C.size_t)(unsafe.Pointer(&inlen))) != 0 {
		return -1, fmt.Errorf("read failed: %v", t.Error())
	}
	return int(inlen), nil
}

// Write writes the given data to the TLS connection.
func (t *TLS) Write(buf []byte) (int, error) {
	var outlen C.size_t
	p := C.CString(string(buf))
	defer C.free(unsafe.Pointer(p))
	if C.tls_write(t.ctx, unsafe.Pointer(p), C.size_t(len(buf)), (*C.size_t)(unsafe.Pointer(&outlen))) != 0 {
		return -1, fmt.Errorf("write failed: %v", t.Error())
	}
	return int(outlen), nil
}

// Close closes the TLS connection.
func (t *TLS) Close() error {
	if C.tls_close(t.ctx) != 0 {
		return fmt.Errorf("close failed: %v", t.Error())
	}
	return nil
}

// Free frees resources associated with the TLS context.
func (t *TLS) Free() {
	if t.ctx == nil {
		return
	}
	C.tls_free(t.ctx)
	t.ctx = nil
}
