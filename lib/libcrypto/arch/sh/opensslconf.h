#include <openssl/opensslfeatures.h>

#undef OPENSSL_EXPORT_VAR_AS_FUNCTION

#ifndef OPENSSL_FILE
#ifdef OPENSSL_NO_FILENAMES
#define OPENSSL_FILE ""
#define OPENSSL_LINE 0
#else
#define OPENSSL_FILE __FILE__
#define OPENSSL_LINE __LINE__
#endif
#endif

#if defined(HEADER_RC4_H)
#if !defined(RC4_CHUNK)
/*
 * This enables code handling data aligned at natural CPU word
 * boundary. See crypto/rc4/rc4_enc.c for further details.
 */
#undef RC4_CHUNK
#endif
#endif
