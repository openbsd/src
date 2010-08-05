/* Generated from /home/src/src/lib/libgssapi/../../kerberosV/src/lib/gssapi/spnego.asn1 */
/* Do not edit */

#ifndef __spnego_asn1_h__
#define __spnego_asn1_h__

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

#define ASN1_MALLOC_ENCODE(T, B, BL, S, L, R)                  \
  do {                                                         \
    (BL) = length_##T((S));                                    \
    (B) = malloc((BL));                                        \
    if((B) == NULL) {                                          \
      (R) = ENOMEM;                                            \
    } else {                                                   \
      (R) = encode_##T(((unsigned char*)(B)) + (BL) - 1, (BL), \
                       (S), (L));                              \
      if((R) != 0) {                                           \
        free((B));                                             \
        (B) = NULL;                                            \
      }                                                        \
    }                                                          \
  } while (0)

#endif

/*
MechType ::= OBJECT IDENTIFIER
*/

typedef heim_oid MechType;

int    encode_MechType(unsigned char *, size_t, const MechType *, size_t *);
int    decode_MechType(const unsigned char *, size_t, MechType *, size_t *);
void   free_MechType  (MechType *);
size_t length_MechType(const MechType *);
int    copy_MechType  (const MechType *, MechType *);


/*
MechTypeList ::= SEQUENCE OF MechType
*/

typedef struct MechTypeList {
  unsigned int len;
  MechType *val;
} MechTypeList;

int    encode_MechTypeList(unsigned char *, size_t, const MechTypeList *, size_t *);
int    decode_MechTypeList(const unsigned char *, size_t, MechTypeList *, size_t *);
void   free_MechTypeList  (MechTypeList *);
size_t length_MechTypeList(const MechTypeList *);
int    copy_MechTypeList  (const MechTypeList *, MechTypeList *);


/*
ContextFlags ::= BIT STRING {
  delegFlag(0),
  mutualFlag(1),
  replayFlag(2),
  sequenceFlag(3),
  anonFlag(4),
  confFlag(5),
  integFlag(6)
}
*/

typedef struct ContextFlags {
  unsigned int delegFlag:1;
  unsigned int mutualFlag:1;
  unsigned int replayFlag:1;
  unsigned int sequenceFlag:1;
  unsigned int anonFlag:1;
  unsigned int confFlag:1;
  unsigned int integFlag:1;
} ContextFlags;


int    encode_ContextFlags(unsigned char *, size_t, const ContextFlags *, size_t *);
int    decode_ContextFlags(const unsigned char *, size_t, ContextFlags *, size_t *);
void   free_ContextFlags  (ContextFlags *);
size_t length_ContextFlags(const ContextFlags *);
int    copy_ContextFlags  (const ContextFlags *, ContextFlags *);
unsigned ContextFlags2int(ContextFlags);
ContextFlags int2ContextFlags(unsigned);
const struct units * asn1_ContextFlags_units(void);

/*
NegTokenInit ::= SEQUENCE {
  mechTypes[0]    MechTypeList OPTIONAL,
  reqFlags[1]     ContextFlags OPTIONAL,
  mechToken[2]    OCTET STRING OPTIONAL,
  mechListMIC[3]  OCTET STRING OPTIONAL
}
*/

typedef struct NegTokenInit {
  MechTypeList *mechTypes;
  ContextFlags *reqFlags;
  heim_octet_string *mechToken;
  heim_octet_string *mechListMIC;
} NegTokenInit;

int    encode_NegTokenInit(unsigned char *, size_t, const NegTokenInit *, size_t *);
int    decode_NegTokenInit(const unsigned char *, size_t, NegTokenInit *, size_t *);
void   free_NegTokenInit  (NegTokenInit *);
size_t length_NegTokenInit(const NegTokenInit *);
int    copy_NegTokenInit  (const NegTokenInit *, NegTokenInit *);


/*
NegTokenTarg ::= SEQUENCE {
  negResult[0]      ENUMERATED {
    accept_completed(0),
    accept_incomplete(1),
    reject(2)
  } OPTIONAL,
  supportedMech[1]  MechType OPTIONAL,
  responseToken[2]  OCTET STRING OPTIONAL,
  mechListMIC[3]    OCTET STRING OPTIONAL
}
*/

typedef struct NegTokenTarg {
  enum  {
    accept_completed = 0,
    accept_incomplete = 1,
    reject = 2
  } *negResult;

  MechType *supportedMech;
  heim_octet_string *responseToken;
  heim_octet_string *mechListMIC;
} NegTokenTarg;

int    encode_NegTokenTarg(unsigned char *, size_t, const NegTokenTarg *, size_t *);
int    decode_NegTokenTarg(const unsigned char *, size_t, NegTokenTarg *, size_t *);
void   free_NegTokenTarg  (NegTokenTarg *);
size_t length_NegTokenTarg(const NegTokenTarg *);
int    copy_NegTokenTarg  (const NegTokenTarg *, NegTokenTarg *);




#endif /* __spnego_asn1_h__ */
