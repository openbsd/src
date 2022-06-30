/*
 * Public domain
 * compatibility shim for openssl11
 * overloading unistd.h is a ugly guly hack for this issue but works here
 */

#include_next <unistd.h>

#include <openssl/asn1.h>
#include <openssl/stack.h>

int ASN1_time_parse(const char *, size_t, struct tm *, int);
int ASN1_time_tm_cmp(struct tm *, struct tm *);

#ifndef DECLARE_STACK_OF
#define DECLARE_STACK_OF DEFINE_STACK_OF
#endif
