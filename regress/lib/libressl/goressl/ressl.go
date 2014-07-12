// Package ressl provides a Go interface to the libressl library.
package ressl

/*
#cgo LDFLAGS: -lressl -lssl -lcrypto

#include <stdlib.h>

#include <ressl/ressl.h>

typedef void *ressl;
*/
import "C"

import (
	"errors"
	"fmt"
	"unsafe"
)

// ResslConfig provides configuration options for a Ressl context.
type ResslConfig struct {
	caFile *C.char
	resslCfg *C.struct_ressl_config
}

// Ressl encapsulates the context for ressl.
type Ressl struct {
	cfg *ResslConfig
	ctx *C.struct_ressl
}

// Init initialises the ressl library.
func Init() error {
	if C.ressl_init() != 0 {
		return errors.New("initialisation failed")
	}
	return nil
}

// NewConfig returns a new ressl configuration.
func NewConfig() (*ResslConfig, error) {
	cfg := C.ressl_config_new()
	if cfg == nil {
		return nil, errors.New("failed to allocate config")
	}
	return &ResslConfig{
		resslCfg: cfg,
	}, nil
}

// SetCAFile sets the CA file to be used for connections.
func (c *ResslConfig) SetCAFile(filename string) {
	if c.caFile != nil {
		C.free(unsafe.Pointer(c.caFile))
	}
	c.caFile = C.CString(filename)
	C.ressl_config_ca_file(c.resslCfg, c.caFile)
}

// SetInsecure disables verification for the connection.
func (c *ResslConfig) SetInsecure() {
	C.ressl_config_insecure(c.resslCfg)
}

// SetSecure enables verification for the connection.
func (c *ResslConfig) SetSecure() {
	C.ressl_config_secure(c.resslCfg)
}

// Free frees resources associated with the ressl configuration.
func (c *ResslConfig) Free() {
	if c.resslCfg == nil {
		return
	}
	C.ressl_config_free(c.resslCfg)
	c.resslCfg = nil
}

// New returns a new ressl context, using the optional configuration. If no
// configuration is specified the defaults will be used.
func New(config *ResslConfig) (*Ressl, error) {
	var sslCfg *C.struct_ressl_config
	if config != nil {
		sslCfg = config.resslCfg
	}
	ctx := C.ressl_new(sslCfg)
	if ctx == nil {
		return nil, errors.New("ressl new failed")
	}
	return &Ressl{
		cfg: config,
		ctx: ctx,
	}, nil
}

// Error returns the error message from the ressl context.
func (r *Ressl) Error() string {
	if msg := C.ressl_error(r.ctx); msg != nil {
		return C.GoString(msg)
	}
	return ""
}

// Connect attempts to establish an SSL connection to the specified host on
// the given port. The host may optionally contain a colon separated port
// value if the port string is specified as an empty string.
func (r *Ressl) Connect(host, port string) error {
	h := C.CString(host)
	var p *C.char
	if port != "" {
		p = C.CString(port)
	}
	defer C.free(unsafe.Pointer(h))
	defer C.free(unsafe.Pointer(p))
	if C.ressl_connect(r.ctx, h, p) != 0 {
		return fmt.Errorf("connect failed: %v", r.Error())
	}
	return nil
}

// Read reads data the SSL connection into the given buffer.
func (r *Ressl) Read(buf []byte) (int, error) {
	var inlen C.size_t
	if C.ressl_read(r.ctx, (*C.char)(unsafe.Pointer(&buf[0])), C.size_t(len(buf)), (*C.size_t)(unsafe.Pointer(&inlen))) != 0 {
		return -1, fmt.Errorf("read failed: %v", r.Error())
	}
	return int(inlen), nil
}

// Write writes the given data to the SSL connection.
func (r *Ressl) Write(buf []byte) (int, error) {
	var outlen C.size_t
	p := C.CString(string(buf))
	defer C.free(unsafe.Pointer(p))
	if C.ressl_write(r.ctx, p, C.size_t(len(buf)), (*C.size_t)(unsafe.Pointer(&outlen))) != 0 {
		return -1, fmt.Errorf("write failed: %v", r.Error())
	}
	return int(outlen), nil
}

// Close closes the SSL connection.
func (r *Ressl) Close() error {
	if C.ressl_close(r.ctx) != 0 {
		return fmt.Errorf("close failed: %v", r.Error())
	}
	return nil
}

// Free frees resources associated with the ressl context.
func (r *Ressl) Free() {
	if r.ctx == nil {
		return
	}
	C.ressl_free(r.ctx)
	r.ctx = nil
}
