/* Generated from /usr/src/kerberosV/lib/hdb/../../src/lib/hdb/hdb.asn1 */
/* Do not edit */

#ifndef __hdb_asn1_h__
#define __hdb_asn1_h__

#include <stddef.h>
#include <time.h>

time_t timegm (struct tm*);

#ifndef __asn1_common_definitions__
#define __asn1_common_definitions__

typedef struct octet_string {
  size_t length;
  void *data;
} octet_string;

typedef char *general_string;

typedef struct oid {
  size_t length;
  unsigned *components;
} oid;

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

enum { HDB_DB_FORMAT = 2 };

enum { hdb_pw_salt = 3 };

enum { hdb_afs3_salt = 10 };

/*
Salt ::= SEQUENCE {
  type[0]         INTEGER,
  salt[1]         OCTET STRING
}
*/

typedef struct Salt {
  int type;
  octet_string salt;
} Salt;

int    encode_Salt(unsigned char *, size_t, const Salt *, size_t *);
int    decode_Salt(const unsigned char *, size_t, Salt *, size_t *);
void   free_Salt  (Salt *);
size_t length_Salt(const Salt *);
int    copy_Salt  (const Salt *, Salt *);


/*
Key ::= SEQUENCE {
  mkvno[0]        INTEGER OPTIONAL,
  key[1]          EncryptionKey,
  salt[2]         Salt OPTIONAL
}
*/

typedef struct Key {
  int *mkvno;
  EncryptionKey key;
  Salt *salt;
} Key;

int    encode_Key(unsigned char *, size_t, const Key *, size_t *);
int    decode_Key(const unsigned char *, size_t, Key *, size_t *);
void   free_Key  (Key *);
size_t length_Key(const Key *);
int    copy_Key  (const Key *, Key *);


/*
Event ::= SEQUENCE {
  time[0]         KerberosTime,
  principal[1]    Principal OPTIONAL
}
*/

typedef struct Event {
  KerberosTime time;
  Principal *principal;
} Event;

int    encode_Event(unsigned char *, size_t, const Event *, size_t *);
int    decode_Event(const unsigned char *, size_t, Event *, size_t *);
void   free_Event  (Event *);
size_t length_Event(const Event *);
int    copy_Event  (const Event *, Event *);


/*
HDBFlags ::= BIT STRING {
  initial(0),
  forwardable(1),
  proxiable(2),
  renewable(3),
  postdate(4),
  server(5),
  client(6),
  invalid(7),
  require-preauth(8),
  change-pw(9),
  require-hwauth(10),
  ok-as-delegate(11),
  user-to-user(12),
  immutable(13)
}
*/

typedef struct HDBFlags {
  unsigned int initial:1;
  unsigned int forwardable:1;
  unsigned int proxiable:1;
  unsigned int renewable:1;
  unsigned int postdate:1;
  unsigned int server:1;
  unsigned int client:1;
  unsigned int invalid:1;
  unsigned int require_preauth:1;
  unsigned int change_pw:1;
  unsigned int require_hwauth:1;
  unsigned int ok_as_delegate:1;
  unsigned int user_to_user:1;
  unsigned int immutable:1;
} HDBFlags;


int    encode_HDBFlags(unsigned char *, size_t, const HDBFlags *, size_t *);
int    decode_HDBFlags(const unsigned char *, size_t, HDBFlags *, size_t *);
void   free_HDBFlags  (HDBFlags *);
size_t length_HDBFlags(const HDBFlags *);
int    copy_HDBFlags  (const HDBFlags *, HDBFlags *);
unsigned HDBFlags2int(HDBFlags);
HDBFlags int2HDBFlags(unsigned);
extern struct units HDBFlags_units[];

/*
GENERATION ::= SEQUENCE {
  time[0]         KerberosTime,
  usec[1]         INTEGER,
  gen[2]          INTEGER
}
*/

typedef struct GENERATION {
  KerberosTime time;
  int usec;
  int gen;
} GENERATION;

int    encode_GENERATION(unsigned char *, size_t, const GENERATION *, size_t *);
int    decode_GENERATION(const unsigned char *, size_t, GENERATION *, size_t *);
void   free_GENERATION  (GENERATION *);
size_t length_GENERATION(const GENERATION *);
int    copy_GENERATION  (const GENERATION *, GENERATION *);


/*
hdb_entry ::= SEQUENCE {
  principal[0]    Principal OPTIONAL,
  kvno[1]         INTEGER,
  keys[2]         SEQUENCE OF Key,
  created-by[3]   Event,
  modified-by[4]  Event OPTIONAL,
  valid-start[5]  KerberosTime OPTIONAL,
  valid-end[6]    KerberosTime OPTIONAL,
  pw-end[7]       KerberosTime OPTIONAL,
  max-life[8]     INTEGER OPTIONAL,
  max-renew[9]    INTEGER OPTIONAL,
  flags[10]       HDBFlags,
  etypes[11]      SEQUENCE OF INTEGER OPTIONAL,
  generation[12]  GENERATION OPTIONAL
}
*/

typedef struct hdb_entry {
  Principal *principal;
  int kvno;
  struct  {
    unsigned int len;
    Key *val;
  } keys;
  Event created_by;
  Event *modified_by;
  KerberosTime *valid_start;
  KerberosTime *valid_end;
  KerberosTime *pw_end;
  int *max_life;
  int *max_renew;
  HDBFlags flags;
  struct  {
    unsigned int len;
    int *val;
  } *etypes;
  GENERATION *generation;
} hdb_entry;

int    encode_hdb_entry(unsigned char *, size_t, const hdb_entry *, size_t *);
int    decode_hdb_entry(const unsigned char *, size_t, hdb_entry *, size_t *);
void   free_hdb_entry  (hdb_entry *);
size_t length_hdb_entry(const hdb_entry *);
int    copy_hdb_entry  (const hdb_entry *, hdb_entry *);


#endif /* __hdb_asn1_h__ */
