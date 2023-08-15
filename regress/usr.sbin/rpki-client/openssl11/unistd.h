/*
 * Public domain
 * compatibility shim for openssl11
 * overloading unistd.h is a ugly guly hack for this issue but works here
 */

#include_next <unistd.h>

#include <openssl/stack.h>

#define CMS_get_version(cms, version) (*(version) = 3, 1)
#define CMS_SignerInfo_get_version(cms, version) (*(version) = 3, 1)

#ifndef DECLARE_STACK_OF
#define DECLARE_STACK_OF DEFINE_STACK_OF
#endif
