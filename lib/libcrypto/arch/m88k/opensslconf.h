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
