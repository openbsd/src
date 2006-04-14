/* $KTH: asn1-common.h,v 1.4 2003/07/15 13:57:31 lha Exp $ */

#include <stddef.h>
#include <time.h>

#ifndef __asn1_common_definitions__
#define __asn1_common_definitions__

typedef struct heim_octet_string {
    size_t length;
    void *data;
} heim_octet_string;

typedef char *heim_general_string;
typedef char *heim_utf8_string;

typedef struct heim_oid {
    size_t length;
    unsigned *components;
} heim_oid;

#endif
