/* Generated from /usr/src/kerberosV/lib/asn1/../../src/lib/asn1/k5.asn1 */
/* Do not edit */

#ifndef __krb5_asn1_h__
#define __krb5_asn1_h__

#include <stddef.h>
#include <time.h>

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

/*
NAME-TYPE ::= INTEGER
*/

typedef enum NAME_TYPE {
  KRB5_NT_UNKNOWN = 0,
  KRB5_NT_PRINCIPAL = 1,
  KRB5_NT_SRV_INST = 2,
  KRB5_NT_SRV_HST = 3,
  KRB5_NT_SRV_XHST = 4,
  KRB5_NT_UID = 5,
  KRB5_NT_X500_PRINCIPAL = 6
} NAME_TYPE;

int    encode_NAME_TYPE(unsigned char *, size_t, const NAME_TYPE *, size_t *);
int    decode_NAME_TYPE(const unsigned char *, size_t, NAME_TYPE *, size_t *);
void   free_NAME_TYPE  (NAME_TYPE *);
size_t length_NAME_TYPE(const NAME_TYPE *);
int    copy_NAME_TYPE  (const NAME_TYPE *, NAME_TYPE *);


/*
MESSAGE-TYPE ::= INTEGER
*/

typedef enum MESSAGE_TYPE {
  krb_as_req = 10,
  krb_as_rep = 11,
  krb_tgs_req = 12,
  krb_tgs_rep = 13,
  krb_ap_req = 14,
  krb_ap_rep = 15,
  krb_safe = 20,
  krb_priv = 21,
  krb_cred = 22,
  krb_error = 30
} MESSAGE_TYPE;

int    encode_MESSAGE_TYPE(unsigned char *, size_t, const MESSAGE_TYPE *, size_t *);
int    decode_MESSAGE_TYPE(const unsigned char *, size_t, MESSAGE_TYPE *, size_t *);
void   free_MESSAGE_TYPE  (MESSAGE_TYPE *);
size_t length_MESSAGE_TYPE(const MESSAGE_TYPE *);
int    copy_MESSAGE_TYPE  (const MESSAGE_TYPE *, MESSAGE_TYPE *);


/*
PADATA-TYPE ::= INTEGER
*/

typedef enum PADATA_TYPE {
  KRB5_PADATA_NONE = 0,
  KRB5_PADATA_TGS_REQ = 1,
  KRB5_PADATA_AP_REQ = 1,
  KRB5_PADATA_ENC_TIMESTAMP = 2,
  KRB5_PADATA_PW_SALT = 3,
  KRB5_PADATA_ENC_UNIX_TIME = 5,
  KRB5_PADATA_SANDIA_SECUREID = 6,
  KRB5_PADATA_SESAME = 7,
  KRB5_PADATA_OSF_DCE = 8,
  KRB5_PADATA_CYBERSAFE_SECUREID = 9,
  KRB5_PADATA_AFS3_SALT = 10,
  KRB5_PADATA_ETYPE_INFO = 11,
  KRB5_PADATA_SAM_CHALLENGE = 12,
  KRB5_PADATA_SAM_RESPONSE = 13,
  KRB5_PADATA_PK_AS_REQ = 14,
  KRB5_PADATA_PK_AS_REP = 15,
  KRB5_PADATA_PK_AS_SIGN = 16,
  KRB5_PADATA_PK_KEY_REQ = 17,
  KRB5_PADATA_PK_KEY_REP = 18,
  KRB5_PADATA_USE_SPECIFIED_KVNO = 20,
  KRB5_PADATA_SAM_REDIRECT = 21,
  KRB5_PADATA_GET_FROM_TYPED_DATA = 22,
  KRB5_PADATA_SAM_ETYPE_INFO = 23
} PADATA_TYPE;

int    encode_PADATA_TYPE(unsigned char *, size_t, const PADATA_TYPE *, size_t *);
int    decode_PADATA_TYPE(const unsigned char *, size_t, PADATA_TYPE *, size_t *);
void   free_PADATA_TYPE  (PADATA_TYPE *);
size_t length_PADATA_TYPE(const PADATA_TYPE *);
int    copy_PADATA_TYPE  (const PADATA_TYPE *, PADATA_TYPE *);


/*
CKSUMTYPE ::= INTEGER
*/

typedef enum CKSUMTYPE {
  CKSUMTYPE_NONE = 0,
  CKSUMTYPE_CRC32 = 1,
  CKSUMTYPE_RSA_MD4 = 2,
  CKSUMTYPE_RSA_MD4_DES = 3,
  CKSUMTYPE_DES_MAC = 4,
  CKSUMTYPE_DES_MAC_K = 5,
  CKSUMTYPE_RSA_MD4_DES_K = 6,
  CKSUMTYPE_RSA_MD5 = 7,
  CKSUMTYPE_RSA_MD5_DES = 8,
  CKSUMTYPE_RSA_MD5_DES3 = 9,
  CKSUMTYPE_HMAC_SHA1_96_AES_128 = 10,
  CKSUMTYPE_HMAC_SHA1_96_AES_256 = 11,
  CKSUMTYPE_HMAC_SHA1_DES3 = 12,
  CKSUMTYPE_SHA1 = 1000,
  CKSUMTYPE_GSSAPI = 32771,
  CKSUMTYPE_HMAC_MD5 = -138,
  CKSUMTYPE_HMAC_MD5_ENC = -1138
} CKSUMTYPE;

int    encode_CKSUMTYPE(unsigned char *, size_t, const CKSUMTYPE *, size_t *);
int    decode_CKSUMTYPE(const unsigned char *, size_t, CKSUMTYPE *, size_t *);
void   free_CKSUMTYPE  (CKSUMTYPE *);
size_t length_CKSUMTYPE(const CKSUMTYPE *);
int    copy_CKSUMTYPE  (const CKSUMTYPE *, CKSUMTYPE *);


/*
ENCTYPE ::= INTEGER
*/

typedef enum ENCTYPE {
  ETYPE_NULL = 0,
  ETYPE_DES_CBC_CRC = 1,
  ETYPE_DES_CBC_MD4 = 2,
  ETYPE_DES_CBC_MD5 = 3,
  ETYPE_DES3_CBC_MD5 = 5,
  ETYPE_OLD_DES3_CBC_SHA1 = 7,
  ETYPE_SIGN_DSA_GENERATE = 8,
  ETYPE_ENCRYPT_RSA_PRIV = 9,
  ETYPE_ENCRYPT_RSA_PUB = 10,
  ETYPE_DES3_CBC_SHA1 = 16,
  ETYPE_AES128_CTS_HMAC_SHA1_96 = 17,
  ETYPE_AES256_CTS_HMAC_SHA1_96 = 18,
  ETYPE_ARCFOUR_HMAC_MD5 = 23,
  ETYPE_ARCFOUR_HMAC_MD5_56 = 24,
  ETYPE_ENCTYPE_PK_CROSS = 48,
  ETYPE_DES_CBC_NONE = -4096,
  ETYPE_DES3_CBC_NONE = -4097,
  ETYPE_DES_CFB64_NONE = -4098,
  ETYPE_DES_PCBC_NONE = -4099
} ENCTYPE;

int    encode_ENCTYPE(unsigned char *, size_t, const ENCTYPE *, size_t *);
int    decode_ENCTYPE(const unsigned char *, size_t, ENCTYPE *, size_t *);
void   free_ENCTYPE  (ENCTYPE *);
size_t length_ENCTYPE(const ENCTYPE *);
int    copy_ENCTYPE  (const ENCTYPE *, ENCTYPE *);


/*
UNSIGNED ::= UNSIGNED INTEGER
*/

typedef unsigned int UNSIGNED;

int    encode_UNSIGNED(unsigned char *, size_t, const UNSIGNED *, size_t *);
int    decode_UNSIGNED(const unsigned char *, size_t, UNSIGNED *, size_t *);
void   free_UNSIGNED  (UNSIGNED *);
size_t length_UNSIGNED(const UNSIGNED *);
int    copy_UNSIGNED  (const UNSIGNED *, UNSIGNED *);


/*
Realm ::= GeneralString
*/

typedef general_string Realm;

int    encode_Realm(unsigned char *, size_t, const Realm *, size_t *);
int    decode_Realm(const unsigned char *, size_t, Realm *, size_t *);
void   free_Realm  (Realm *);
size_t length_Realm(const Realm *);
int    copy_Realm  (const Realm *, Realm *);


/*
PrincipalName ::= SEQUENCE {
  name-type[0]    NAME-TYPE,
  name-string[1]  SEQUENCE OF GeneralString
}
*/

typedef struct PrincipalName {
  NAME_TYPE name_type;
  struct  {
    unsigned int len;
    general_string *val;
  } name_string;
} PrincipalName;

int    encode_PrincipalName(unsigned char *, size_t, const PrincipalName *, size_t *);
int    decode_PrincipalName(const unsigned char *, size_t, PrincipalName *, size_t *);
void   free_PrincipalName  (PrincipalName *);
size_t length_PrincipalName(const PrincipalName *);
int    copy_PrincipalName  (const PrincipalName *, PrincipalName *);


/*
Principal ::= SEQUENCE {
  name[0]         PrincipalName,
  realm[1]        Realm
}
*/

typedef struct Principal {
  PrincipalName name;
  Realm realm;
} Principal;

int    encode_Principal(unsigned char *, size_t, const Principal *, size_t *);
int    decode_Principal(const unsigned char *, size_t, Principal *, size_t *);
void   free_Principal  (Principal *);
size_t length_Principal(const Principal *);
int    copy_Principal  (const Principal *, Principal *);


/*
HostAddress ::= SEQUENCE {
  addr-type[0]    INTEGER,
  address[1]      OCTET STRING
}
*/

typedef struct HostAddress {
  int addr_type;
  octet_string address;
} HostAddress;

int    encode_HostAddress(unsigned char *, size_t, const HostAddress *, size_t *);
int    decode_HostAddress(const unsigned char *, size_t, HostAddress *, size_t *);
void   free_HostAddress  (HostAddress *);
size_t length_HostAddress(const HostAddress *);
int    copy_HostAddress  (const HostAddress *, HostAddress *);


/*
HostAddresses ::= SEQUENCE OF HostAddress
*/

typedef struct HostAddresses {
  unsigned int len;
  HostAddress *val;
} HostAddresses;

int    encode_HostAddresses(unsigned char *, size_t, const HostAddresses *, size_t *);
int    decode_HostAddresses(const unsigned char *, size_t, HostAddresses *, size_t *);
void   free_HostAddresses  (HostAddresses *);
size_t length_HostAddresses(const HostAddresses *);
int    copy_HostAddresses  (const HostAddresses *, HostAddresses *);


/*
KerberosTime ::= GeneralizedTime
*/

typedef time_t KerberosTime;

int    encode_KerberosTime(unsigned char *, size_t, const KerberosTime *, size_t *);
int    decode_KerberosTime(const unsigned char *, size_t, KerberosTime *, size_t *);
void   free_KerberosTime  (KerberosTime *);
size_t length_KerberosTime(const KerberosTime *);
int    copy_KerberosTime  (const KerberosTime *, KerberosTime *);


/*
AuthorizationData ::= SEQUENCE OF SEQUENCE {
  ad-type[0]      INTEGER,
  ad-data[1]      OCTET STRING
}
*/

typedef struct AuthorizationData {
  unsigned int len;
  struct  {
    int ad_type;
    octet_string ad_data;
  } *val;
} AuthorizationData;

int    encode_AuthorizationData(unsigned char *, size_t, const AuthorizationData *, size_t *);
int    decode_AuthorizationData(const unsigned char *, size_t, AuthorizationData *, size_t *);
void   free_AuthorizationData  (AuthorizationData *);
size_t length_AuthorizationData(const AuthorizationData *);
int    copy_AuthorizationData  (const AuthorizationData *, AuthorizationData *);


/*
APOptions ::= BIT STRING {
  reserved(0),
  use-session-key(1),
  mutual-required(2)
}
*/

typedef struct APOptions {
  unsigned int reserved:1;
  unsigned int use_session_key:1;
  unsigned int mutual_required:1;
} APOptions;


int    encode_APOptions(unsigned char *, size_t, const APOptions *, size_t *);
int    decode_APOptions(const unsigned char *, size_t, APOptions *, size_t *);
void   free_APOptions  (APOptions *);
size_t length_APOptions(const APOptions *);
int    copy_APOptions  (const APOptions *, APOptions *);
unsigned APOptions2int(APOptions);
APOptions int2APOptions(unsigned);
extern struct units APOptions_units[];

/*
TicketFlags ::= BIT STRING {
  reserved(0),
  forwardable(1),
  forwarded(2),
  proxiable(3),
  proxy(4),
  may-postdate(5),
  postdated(6),
  invalid(7),
  renewable(8),
  initial(9),
  pre-authent(10),
  hw-authent(11),
  transited-policy-checked(12),
  ok-as-delegate(13),
  anonymous(14)
}
*/

typedef struct TicketFlags {
  unsigned int reserved:1;
  unsigned int forwardable:1;
  unsigned int forwarded:1;
  unsigned int proxiable:1;
  unsigned int proxy:1;
  unsigned int may_postdate:1;
  unsigned int postdated:1;
  unsigned int invalid:1;
  unsigned int renewable:1;
  unsigned int initial:1;
  unsigned int pre_authent:1;
  unsigned int hw_authent:1;
  unsigned int transited_policy_checked:1;
  unsigned int ok_as_delegate:1;
  unsigned int anonymous:1;
} TicketFlags;


int    encode_TicketFlags(unsigned char *, size_t, const TicketFlags *, size_t *);
int    decode_TicketFlags(const unsigned char *, size_t, TicketFlags *, size_t *);
void   free_TicketFlags  (TicketFlags *);
size_t length_TicketFlags(const TicketFlags *);
int    copy_TicketFlags  (const TicketFlags *, TicketFlags *);
unsigned TicketFlags2int(TicketFlags);
TicketFlags int2TicketFlags(unsigned);
extern struct units TicketFlags_units[];

/*
KDCOptions ::= BIT STRING {
  reserved(0),
  forwardable(1),
  forwarded(2),
  proxiable(3),
  proxy(4),
  allow-postdate(5),
  postdated(6),
  unused7(7),
  renewable(8),
  unused9(9),
  unused10(10),
  unused11(11),
  request-anonymous(14),
  canonicalize(15),
  disable-transited-check(26),
  renewable-ok(27),
  enc-tkt-in-skey(28),
  renew(30),
  validate(31)
}
*/

typedef struct KDCOptions {
  unsigned int reserved:1;
  unsigned int forwardable:1;
  unsigned int forwarded:1;
  unsigned int proxiable:1;
  unsigned int proxy:1;
  unsigned int allow_postdate:1;
  unsigned int postdated:1;
  unsigned int unused7:1;
  unsigned int renewable:1;
  unsigned int unused9:1;
  unsigned int unused10:1;
  unsigned int unused11:1;
  unsigned int request_anonymous:1;
  unsigned int canonicalize:1;
  unsigned int disable_transited_check:1;
  unsigned int renewable_ok:1;
  unsigned int enc_tkt_in_skey:1;
  unsigned int renew:1;
  unsigned int validate:1;
} KDCOptions;


int    encode_KDCOptions(unsigned char *, size_t, const KDCOptions *, size_t *);
int    decode_KDCOptions(const unsigned char *, size_t, KDCOptions *, size_t *);
void   free_KDCOptions  (KDCOptions *);
size_t length_KDCOptions(const KDCOptions *);
int    copy_KDCOptions  (const KDCOptions *, KDCOptions *);
unsigned KDCOptions2int(KDCOptions);
KDCOptions int2KDCOptions(unsigned);
extern struct units KDCOptions_units[];

/*
LR-TYPE ::= INTEGER
*/

typedef enum LR_TYPE {
  LR_NONE = 0,
  LR_INITIAL_TGT = 1,
  LR_INITIAL = 2,
  LR_ISSUE_USE_TGT = 3,
  LR_RENEWAL = 4,
  LR_REQUEST = 5,
  LR_PW_EXPTIME = 6,
  LR_ACCT_EXPTIME = 7
} LR_TYPE;

int    encode_LR_TYPE(unsigned char *, size_t, const LR_TYPE *, size_t *);
int    decode_LR_TYPE(const unsigned char *, size_t, LR_TYPE *, size_t *);
void   free_LR_TYPE  (LR_TYPE *);
size_t length_LR_TYPE(const LR_TYPE *);
int    copy_LR_TYPE  (const LR_TYPE *, LR_TYPE *);


/*
LastReq ::= SEQUENCE OF SEQUENCE {
  lr-type[0]      LR-TYPE,
  lr-value[1]     KerberosTime
}
*/

typedef struct LastReq {
  unsigned int len;
  struct  {
    LR_TYPE lr_type;
    KerberosTime lr_value;
  } *val;
} LastReq;

int    encode_LastReq(unsigned char *, size_t, const LastReq *, size_t *);
int    decode_LastReq(const unsigned char *, size_t, LastReq *, size_t *);
void   free_LastReq  (LastReq *);
size_t length_LastReq(const LastReq *);
int    copy_LastReq  (const LastReq *, LastReq *);


/*
EncryptedData ::= SEQUENCE {
  etype[0]        ENCTYPE,
  kvno[1]         INTEGER OPTIONAL,
  cipher[2]       OCTET STRING
}
*/

typedef struct EncryptedData {
  ENCTYPE etype;
  int *kvno;
  octet_string cipher;
} EncryptedData;

int    encode_EncryptedData(unsigned char *, size_t, const EncryptedData *, size_t *);
int    decode_EncryptedData(const unsigned char *, size_t, EncryptedData *, size_t *);
void   free_EncryptedData  (EncryptedData *);
size_t length_EncryptedData(const EncryptedData *);
int    copy_EncryptedData  (const EncryptedData *, EncryptedData *);


/*
EncryptionKey ::= SEQUENCE {
  keytype[0]      INTEGER,
  keyvalue[1]     OCTET STRING
}
*/

typedef struct EncryptionKey {
  int keytype;
  octet_string keyvalue;
} EncryptionKey;

int    encode_EncryptionKey(unsigned char *, size_t, const EncryptionKey *, size_t *);
int    decode_EncryptionKey(const unsigned char *, size_t, EncryptionKey *, size_t *);
void   free_EncryptionKey  (EncryptionKey *);
size_t length_EncryptionKey(const EncryptionKey *);
int    copy_EncryptionKey  (const EncryptionKey *, EncryptionKey *);


/*
TransitedEncoding ::= SEQUENCE {
  tr-type[0]      INTEGER,
  contents[1]     OCTET STRING
}
*/

typedef struct TransitedEncoding {
  int tr_type;
  octet_string contents;
} TransitedEncoding;

int    encode_TransitedEncoding(unsigned char *, size_t, const TransitedEncoding *, size_t *);
int    decode_TransitedEncoding(const unsigned char *, size_t, TransitedEncoding *, size_t *);
void   free_TransitedEncoding  (TransitedEncoding *);
size_t length_TransitedEncoding(const TransitedEncoding *);
int    copy_TransitedEncoding  (const TransitedEncoding *, TransitedEncoding *);


/*
Ticket ::= [APPLICATION 1] SEQUENCE {
  tkt-vno[0]      INTEGER,
  realm[1]        Realm,
  sname[2]        PrincipalName,
  enc-part[3]     EncryptedData
}
*/

typedef struct  {
  int tkt_vno;
  Realm realm;
  PrincipalName sname;
  EncryptedData enc_part;
} Ticket;

int    encode_Ticket(unsigned char *, size_t, const Ticket *, size_t *);
int    decode_Ticket(const unsigned char *, size_t, Ticket *, size_t *);
void   free_Ticket  (Ticket *);
size_t length_Ticket(const Ticket *);
int    copy_Ticket  (const Ticket *, Ticket *);


/*
EncTicketPart ::= [APPLICATION 3] SEQUENCE {
  flags[0]                TicketFlags,
  key[1]                  EncryptionKey,
  crealm[2]               Realm,
  cname[3]                PrincipalName,
  transited[4]            TransitedEncoding,
  authtime[5]             KerberosTime,
  starttime[6]            KerberosTime OPTIONAL,
  endtime[7]              KerberosTime,
  renew-till[8]           KerberosTime OPTIONAL,
  caddr[9]                HostAddresses OPTIONAL,
  authorization-data[10]  AuthorizationData OPTIONAL
}
*/

typedef struct  {
  TicketFlags flags;
  EncryptionKey key;
  Realm crealm;
  PrincipalName cname;
  TransitedEncoding transited;
  KerberosTime authtime;
  KerberosTime *starttime;
  KerberosTime endtime;
  KerberosTime *renew_till;
  HostAddresses *caddr;
  AuthorizationData *authorization_data;
} EncTicketPart;

int    encode_EncTicketPart(unsigned char *, size_t, const EncTicketPart *, size_t *);
int    decode_EncTicketPart(const unsigned char *, size_t, EncTicketPart *, size_t *);
void   free_EncTicketPart  (EncTicketPart *);
size_t length_EncTicketPart(const EncTicketPart *);
int    copy_EncTicketPart  (const EncTicketPart *, EncTicketPart *);


/*
Checksum ::= SEQUENCE {
  cksumtype[0]    CKSUMTYPE,
  checksum[1]     OCTET STRING
}
*/

typedef struct Checksum {
  CKSUMTYPE cksumtype;
  octet_string checksum;
} Checksum;

int    encode_Checksum(unsigned char *, size_t, const Checksum *, size_t *);
int    decode_Checksum(const unsigned char *, size_t, Checksum *, size_t *);
void   free_Checksum  (Checksum *);
size_t length_Checksum(const Checksum *);
int    copy_Checksum  (const Checksum *, Checksum *);


/*
Authenticator ::= [APPLICATION 2] SEQUENCE {
  authenticator-vno[0]   INTEGER,
  crealm[1]              Realm,
  cname[2]               PrincipalName,
  cksum[3]               Checksum OPTIONAL,
  cusec[4]               INTEGER,
  ctime[5]               KerberosTime,
  subkey[6]              EncryptionKey OPTIONAL,
  seq-number[7]          UNSIGNED OPTIONAL,
  authorization-data[8]  AuthorizationData OPTIONAL
}
*/

typedef struct  {
  int authenticator_vno;
  Realm crealm;
  PrincipalName cname;
  Checksum *cksum;
  int cusec;
  KerberosTime ctime;
  EncryptionKey *subkey;
  UNSIGNED *seq_number;
  AuthorizationData *authorization_data;
} Authenticator;

int    encode_Authenticator(unsigned char *, size_t, const Authenticator *, size_t *);
int    decode_Authenticator(const unsigned char *, size_t, Authenticator *, size_t *);
void   free_Authenticator  (Authenticator *);
size_t length_Authenticator(const Authenticator *);
int    copy_Authenticator  (const Authenticator *, Authenticator *);


/*
PA-DATA ::= SEQUENCE {
  padata-type[1]   PADATA-TYPE,
  padata-value[2]  OCTET STRING
}
*/

typedef struct PA_DATA {
  PADATA_TYPE padata_type;
  octet_string padata_value;
} PA_DATA;

int    encode_PA_DATA(unsigned char *, size_t, const PA_DATA *, size_t *);
int    decode_PA_DATA(const unsigned char *, size_t, PA_DATA *, size_t *);
void   free_PA_DATA  (PA_DATA *);
size_t length_PA_DATA(const PA_DATA *);
int    copy_PA_DATA  (const PA_DATA *, PA_DATA *);


/*
ETYPE-INFO-ENTRY ::= SEQUENCE {
  etype[0]        ENCTYPE,
  salt[1]         OCTET STRING OPTIONAL,
  salttype[2]     INTEGER OPTIONAL
}
*/

typedef struct ETYPE_INFO_ENTRY {
  ENCTYPE etype;
  octet_string *salt;
  int *salttype;
} ETYPE_INFO_ENTRY;

int    encode_ETYPE_INFO_ENTRY(unsigned char *, size_t, const ETYPE_INFO_ENTRY *, size_t *);
int    decode_ETYPE_INFO_ENTRY(const unsigned char *, size_t, ETYPE_INFO_ENTRY *, size_t *);
void   free_ETYPE_INFO_ENTRY  (ETYPE_INFO_ENTRY *);
size_t length_ETYPE_INFO_ENTRY(const ETYPE_INFO_ENTRY *);
int    copy_ETYPE_INFO_ENTRY  (const ETYPE_INFO_ENTRY *, ETYPE_INFO_ENTRY *);


/*
ETYPE-INFO ::= SEQUENCE OF ETYPE-INFO-ENTRY
*/

typedef struct ETYPE_INFO {
  unsigned int len;
  ETYPE_INFO_ENTRY *val;
} ETYPE_INFO;

int    encode_ETYPE_INFO(unsigned char *, size_t, const ETYPE_INFO *, size_t *);
int    decode_ETYPE_INFO(const unsigned char *, size_t, ETYPE_INFO *, size_t *);
void   free_ETYPE_INFO  (ETYPE_INFO *);
size_t length_ETYPE_INFO(const ETYPE_INFO *);
int    copy_ETYPE_INFO  (const ETYPE_INFO *, ETYPE_INFO *);


/*
METHOD-DATA ::= SEQUENCE OF PA-DATA
*/

typedef struct METHOD_DATA {
  unsigned int len;
  PA_DATA *val;
} METHOD_DATA;

int    encode_METHOD_DATA(unsigned char *, size_t, const METHOD_DATA *, size_t *);
int    decode_METHOD_DATA(const unsigned char *, size_t, METHOD_DATA *, size_t *);
void   free_METHOD_DATA  (METHOD_DATA *);
size_t length_METHOD_DATA(const METHOD_DATA *);
int    copy_METHOD_DATA  (const METHOD_DATA *, METHOD_DATA *);


/*
KDC-REQ-BODY ::= SEQUENCE {
  kdc-options[0]              KDCOptions,
  cname[1]                    PrincipalName OPTIONAL,
  realm[2]                    Realm,
  sname[3]                    PrincipalName OPTIONAL,
  from[4]                     KerberosTime OPTIONAL,
  till[5]                     KerberosTime OPTIONAL,
  rtime[6]                    KerberosTime OPTIONAL,
  nonce[7]                    INTEGER,
  etype[8]                    SEQUENCE OF ENCTYPE,
  addresses[9]                HostAddresses OPTIONAL,
  enc-authorization-data[10]  EncryptedData OPTIONAL,
  additional-tickets[11]      SEQUENCE OF Ticket OPTIONAL
}
*/

typedef struct KDC_REQ_BODY {
  KDCOptions kdc_options;
  PrincipalName *cname;
  Realm realm;
  PrincipalName *sname;
  KerberosTime *from;
  KerberosTime *till;
  KerberosTime *rtime;
  int nonce;
  struct  {
    unsigned int len;
    ENCTYPE *val;
  } etype;
  HostAddresses *addresses;
  EncryptedData *enc_authorization_data;
  struct  {
    unsigned int len;
    Ticket *val;
  } *additional_tickets;
} KDC_REQ_BODY;

int    encode_KDC_REQ_BODY(unsigned char *, size_t, const KDC_REQ_BODY *, size_t *);
int    decode_KDC_REQ_BODY(const unsigned char *, size_t, KDC_REQ_BODY *, size_t *);
void   free_KDC_REQ_BODY  (KDC_REQ_BODY *);
size_t length_KDC_REQ_BODY(const KDC_REQ_BODY *);
int    copy_KDC_REQ_BODY  (const KDC_REQ_BODY *, KDC_REQ_BODY *);


/*
KDC-REQ ::= SEQUENCE {
  pvno[1]         INTEGER,
  msg-type[2]     MESSAGE-TYPE,
  padata[3]       METHOD-DATA OPTIONAL,
  req-body[4]     KDC-REQ-BODY
}
*/

typedef struct KDC_REQ {
  int pvno;
  MESSAGE_TYPE msg_type;
  METHOD_DATA *padata;
  KDC_REQ_BODY req_body;
} KDC_REQ;

int    encode_KDC_REQ(unsigned char *, size_t, const KDC_REQ *, size_t *);
int    decode_KDC_REQ(const unsigned char *, size_t, KDC_REQ *, size_t *);
void   free_KDC_REQ  (KDC_REQ *);
size_t length_KDC_REQ(const KDC_REQ *);
int    copy_KDC_REQ  (const KDC_REQ *, KDC_REQ *);


/*
AS-REQ ::= [APPLICATION 10] KDC-REQ
*/

typedef KDC_REQ AS_REQ;

int    encode_AS_REQ(unsigned char *, size_t, const AS_REQ *, size_t *);
int    decode_AS_REQ(const unsigned char *, size_t, AS_REQ *, size_t *);
void   free_AS_REQ  (AS_REQ *);
size_t length_AS_REQ(const AS_REQ *);
int    copy_AS_REQ  (const AS_REQ *, AS_REQ *);


/*
TGS-REQ ::= [APPLICATION 12] KDC-REQ
*/

typedef KDC_REQ TGS_REQ;

int    encode_TGS_REQ(unsigned char *, size_t, const TGS_REQ *, size_t *);
int    decode_TGS_REQ(const unsigned char *, size_t, TGS_REQ *, size_t *);
void   free_TGS_REQ  (TGS_REQ *);
size_t length_TGS_REQ(const TGS_REQ *);
int    copy_TGS_REQ  (const TGS_REQ *, TGS_REQ *);


/*
PA-ENC-TS-ENC ::= SEQUENCE {
  patimestamp[0]  KerberosTime,
  pausec[1]       INTEGER OPTIONAL
}
*/

typedef struct PA_ENC_TS_ENC {
  KerberosTime patimestamp;
  int *pausec;
} PA_ENC_TS_ENC;

int    encode_PA_ENC_TS_ENC(unsigned char *, size_t, const PA_ENC_TS_ENC *, size_t *);
int    decode_PA_ENC_TS_ENC(const unsigned char *, size_t, PA_ENC_TS_ENC *, size_t *);
void   free_PA_ENC_TS_ENC  (PA_ENC_TS_ENC *);
size_t length_PA_ENC_TS_ENC(const PA_ENC_TS_ENC *);
int    copy_PA_ENC_TS_ENC  (const PA_ENC_TS_ENC *, PA_ENC_TS_ENC *);


/*
KDC-REP ::= SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  padata[2]       METHOD-DATA OPTIONAL,
  crealm[3]       Realm,
  cname[4]        PrincipalName,
  ticket[5]       Ticket,
  enc-part[6]     EncryptedData
}
*/

typedef struct KDC_REP {
  int pvno;
  MESSAGE_TYPE msg_type;
  METHOD_DATA *padata;
  Realm crealm;
  PrincipalName cname;
  Ticket ticket;
  EncryptedData enc_part;
} KDC_REP;

int    encode_KDC_REP(unsigned char *, size_t, const KDC_REP *, size_t *);
int    decode_KDC_REP(const unsigned char *, size_t, KDC_REP *, size_t *);
void   free_KDC_REP  (KDC_REP *);
size_t length_KDC_REP(const KDC_REP *);
int    copy_KDC_REP  (const KDC_REP *, KDC_REP *);


/*
AS-REP ::= [APPLICATION 11] KDC-REP
*/

typedef KDC_REP AS_REP;

int    encode_AS_REP(unsigned char *, size_t, const AS_REP *, size_t *);
int    decode_AS_REP(const unsigned char *, size_t, AS_REP *, size_t *);
void   free_AS_REP  (AS_REP *);
size_t length_AS_REP(const AS_REP *);
int    copy_AS_REP  (const AS_REP *, AS_REP *);


/*
TGS-REP ::= [APPLICATION 13] KDC-REP
*/

typedef KDC_REP TGS_REP;

int    encode_TGS_REP(unsigned char *, size_t, const TGS_REP *, size_t *);
int    decode_TGS_REP(const unsigned char *, size_t, TGS_REP *, size_t *);
void   free_TGS_REP  (TGS_REP *);
size_t length_TGS_REP(const TGS_REP *);
int    copy_TGS_REP  (const TGS_REP *, TGS_REP *);


/*
EncKDCRepPart ::= SEQUENCE {
  key[0]             EncryptionKey,
  last-req[1]        LastReq,
  nonce[2]           INTEGER,
  key-expiration[3]  KerberosTime OPTIONAL,
  flags[4]           TicketFlags,
  authtime[5]        KerberosTime,
  starttime[6]       KerberosTime OPTIONAL,
  endtime[7]         KerberosTime,
  renew-till[8]      KerberosTime OPTIONAL,
  srealm[9]          Realm,
  sname[10]          PrincipalName,
  caddr[11]          HostAddresses OPTIONAL
}
*/

typedef struct EncKDCRepPart {
  EncryptionKey key;
  LastReq last_req;
  int nonce;
  KerberosTime *key_expiration;
  TicketFlags flags;
  KerberosTime authtime;
  KerberosTime *starttime;
  KerberosTime endtime;
  KerberosTime *renew_till;
  Realm srealm;
  PrincipalName sname;
  HostAddresses *caddr;
} EncKDCRepPart;

int    encode_EncKDCRepPart(unsigned char *, size_t, const EncKDCRepPart *, size_t *);
int    decode_EncKDCRepPart(const unsigned char *, size_t, EncKDCRepPart *, size_t *);
void   free_EncKDCRepPart  (EncKDCRepPart *);
size_t length_EncKDCRepPart(const EncKDCRepPart *);
int    copy_EncKDCRepPart  (const EncKDCRepPart *, EncKDCRepPart *);


/*
EncASRepPart ::= [APPLICATION 25] EncKDCRepPart
*/

typedef EncKDCRepPart EncASRepPart;

int    encode_EncASRepPart(unsigned char *, size_t, const EncASRepPart *, size_t *);
int    decode_EncASRepPart(const unsigned char *, size_t, EncASRepPart *, size_t *);
void   free_EncASRepPart  (EncASRepPart *);
size_t length_EncASRepPart(const EncASRepPart *);
int    copy_EncASRepPart  (const EncASRepPart *, EncASRepPart *);


/*
EncTGSRepPart ::= [APPLICATION 26] EncKDCRepPart
*/

typedef EncKDCRepPart EncTGSRepPart;

int    encode_EncTGSRepPart(unsigned char *, size_t, const EncTGSRepPart *, size_t *);
int    decode_EncTGSRepPart(const unsigned char *, size_t, EncTGSRepPart *, size_t *);
void   free_EncTGSRepPart  (EncTGSRepPart *);
size_t length_EncTGSRepPart(const EncTGSRepPart *);
int    copy_EncTGSRepPart  (const EncTGSRepPart *, EncTGSRepPart *);


/*
AP-REQ ::= [APPLICATION 14] SEQUENCE {
  pvno[0]           INTEGER,
  msg-type[1]       MESSAGE-TYPE,
  ap-options[2]     APOptions,
  ticket[3]         Ticket,
  authenticator[4]  EncryptedData
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  APOptions ap_options;
  Ticket ticket;
  EncryptedData authenticator;
} AP_REQ;

int    encode_AP_REQ(unsigned char *, size_t, const AP_REQ *, size_t *);
int    decode_AP_REQ(const unsigned char *, size_t, AP_REQ *, size_t *);
void   free_AP_REQ  (AP_REQ *);
size_t length_AP_REQ(const AP_REQ *);
int    copy_AP_REQ  (const AP_REQ *, AP_REQ *);


/*
AP-REP ::= [APPLICATION 15] SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  enc-part[2]     EncryptedData
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  EncryptedData enc_part;
} AP_REP;

int    encode_AP_REP(unsigned char *, size_t, const AP_REP *, size_t *);
int    decode_AP_REP(const unsigned char *, size_t, AP_REP *, size_t *);
void   free_AP_REP  (AP_REP *);
size_t length_AP_REP(const AP_REP *);
int    copy_AP_REP  (const AP_REP *, AP_REP *);


/*
EncAPRepPart ::= [APPLICATION 27] SEQUENCE {
  ctime[0]        KerberosTime,
  cusec[1]        INTEGER,
  subkey[2]       EncryptionKey OPTIONAL,
  seq-number[3]   UNSIGNED OPTIONAL
}
*/

typedef struct  {
  KerberosTime ctime;
  int cusec;
  EncryptionKey *subkey;
  UNSIGNED *seq_number;
} EncAPRepPart;

int    encode_EncAPRepPart(unsigned char *, size_t, const EncAPRepPart *, size_t *);
int    decode_EncAPRepPart(const unsigned char *, size_t, EncAPRepPart *, size_t *);
void   free_EncAPRepPart  (EncAPRepPart *);
size_t length_EncAPRepPart(const EncAPRepPart *);
int    copy_EncAPRepPart  (const EncAPRepPart *, EncAPRepPart *);


/*
KRB-SAFE-BODY ::= SEQUENCE {
  user-data[0]    OCTET STRING,
  timestamp[1]    KerberosTime OPTIONAL,
  usec[2]         INTEGER OPTIONAL,
  seq-number[3]   UNSIGNED OPTIONAL,
  s-address[4]    HostAddress OPTIONAL,
  r-address[5]    HostAddress OPTIONAL
}
*/

typedef struct KRB_SAFE_BODY {
  octet_string user_data;
  KerberosTime *timestamp;
  int *usec;
  UNSIGNED *seq_number;
  HostAddress *s_address;
  HostAddress *r_address;
} KRB_SAFE_BODY;

int    encode_KRB_SAFE_BODY(unsigned char *, size_t, const KRB_SAFE_BODY *, size_t *);
int    decode_KRB_SAFE_BODY(const unsigned char *, size_t, KRB_SAFE_BODY *, size_t *);
void   free_KRB_SAFE_BODY  (KRB_SAFE_BODY *);
size_t length_KRB_SAFE_BODY(const KRB_SAFE_BODY *);
int    copy_KRB_SAFE_BODY  (const KRB_SAFE_BODY *, KRB_SAFE_BODY *);


/*
KRB-SAFE ::= [APPLICATION 20] SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  safe-body[2]    KRB-SAFE-BODY,
  cksum[3]        Checksum
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  KRB_SAFE_BODY safe_body;
  Checksum cksum;
} KRB_SAFE;

int    encode_KRB_SAFE(unsigned char *, size_t, const KRB_SAFE *, size_t *);
int    decode_KRB_SAFE(const unsigned char *, size_t, KRB_SAFE *, size_t *);
void   free_KRB_SAFE  (KRB_SAFE *);
size_t length_KRB_SAFE(const KRB_SAFE *);
int    copy_KRB_SAFE  (const KRB_SAFE *, KRB_SAFE *);


/*
KRB-PRIV ::= [APPLICATION 21] SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  enc-part[3]     EncryptedData
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  EncryptedData enc_part;
} KRB_PRIV;

int    encode_KRB_PRIV(unsigned char *, size_t, const KRB_PRIV *, size_t *);
int    decode_KRB_PRIV(const unsigned char *, size_t, KRB_PRIV *, size_t *);
void   free_KRB_PRIV  (KRB_PRIV *);
size_t length_KRB_PRIV(const KRB_PRIV *);
int    copy_KRB_PRIV  (const KRB_PRIV *, KRB_PRIV *);


/*
EncKrbPrivPart ::= [APPLICATION 28] SEQUENCE {
  user-data[0]    OCTET STRING,
  timestamp[1]    KerberosTime OPTIONAL,
  usec[2]         INTEGER OPTIONAL,
  seq-number[3]   UNSIGNED OPTIONAL,
  s-address[4]    HostAddress OPTIONAL,
  r-address[5]    HostAddress OPTIONAL
}
*/

typedef struct  {
  octet_string user_data;
  KerberosTime *timestamp;
  int *usec;
  UNSIGNED *seq_number;
  HostAddress *s_address;
  HostAddress *r_address;
} EncKrbPrivPart;

int    encode_EncKrbPrivPart(unsigned char *, size_t, const EncKrbPrivPart *, size_t *);
int    decode_EncKrbPrivPart(const unsigned char *, size_t, EncKrbPrivPart *, size_t *);
void   free_EncKrbPrivPart  (EncKrbPrivPart *);
size_t length_EncKrbPrivPart(const EncKrbPrivPart *);
int    copy_EncKrbPrivPart  (const EncKrbPrivPart *, EncKrbPrivPart *);


/*
KRB-CRED ::= [APPLICATION 22] SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  tickets[2]      SEQUENCE OF Ticket,
  enc-part[3]     EncryptedData
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  struct  {
    unsigned int len;
    Ticket *val;
  } tickets;
  EncryptedData enc_part;
} KRB_CRED;

int    encode_KRB_CRED(unsigned char *, size_t, const KRB_CRED *, size_t *);
int    decode_KRB_CRED(const unsigned char *, size_t, KRB_CRED *, size_t *);
void   free_KRB_CRED  (KRB_CRED *);
size_t length_KRB_CRED(const KRB_CRED *);
int    copy_KRB_CRED  (const KRB_CRED *, KRB_CRED *);


/*
KrbCredInfo ::= SEQUENCE {
  key[0]          EncryptionKey,
  prealm[1]       Realm OPTIONAL,
  pname[2]        PrincipalName OPTIONAL,
  flags[3]        TicketFlags OPTIONAL,
  authtime[4]     KerberosTime OPTIONAL,
  starttime[5]    KerberosTime OPTIONAL,
  endtime[6]      KerberosTime OPTIONAL,
  renew-till[7]   KerberosTime OPTIONAL,
  srealm[8]       Realm OPTIONAL,
  sname[9]        PrincipalName OPTIONAL,
  caddr[10]       HostAddresses OPTIONAL
}
*/

typedef struct KrbCredInfo {
  EncryptionKey key;
  Realm *prealm;
  PrincipalName *pname;
  TicketFlags *flags;
  KerberosTime *authtime;
  KerberosTime *starttime;
  KerberosTime *endtime;
  KerberosTime *renew_till;
  Realm *srealm;
  PrincipalName *sname;
  HostAddresses *caddr;
} KrbCredInfo;

int    encode_KrbCredInfo(unsigned char *, size_t, const KrbCredInfo *, size_t *);
int    decode_KrbCredInfo(const unsigned char *, size_t, KrbCredInfo *, size_t *);
void   free_KrbCredInfo  (KrbCredInfo *);
size_t length_KrbCredInfo(const KrbCredInfo *);
int    copy_KrbCredInfo  (const KrbCredInfo *, KrbCredInfo *);


/*
EncKrbCredPart ::= [APPLICATION 29] SEQUENCE {
  ticket-info[0]  SEQUENCE OF KrbCredInfo,
  nonce[1]        INTEGER OPTIONAL,
  timestamp[2]    KerberosTime OPTIONAL,
  usec[3]         INTEGER OPTIONAL,
  s-address[4]    HostAddress OPTIONAL,
  r-address[5]    HostAddress OPTIONAL
}
*/

typedef struct  {
  struct  {
    unsigned int len;
    KrbCredInfo *val;
  } ticket_info;
  int *nonce;
  KerberosTime *timestamp;
  int *usec;
  HostAddress *s_address;
  HostAddress *r_address;
} EncKrbCredPart;

int    encode_EncKrbCredPart(unsigned char *, size_t, const EncKrbCredPart *, size_t *);
int    decode_EncKrbCredPart(const unsigned char *, size_t, EncKrbCredPart *, size_t *);
void   free_EncKrbCredPart  (EncKrbCredPart *);
size_t length_EncKrbCredPart(const EncKrbCredPart *);
int    copy_EncKrbCredPart  (const EncKrbCredPart *, EncKrbCredPart *);


/*
KRB-ERROR ::= [APPLICATION 30] SEQUENCE {
  pvno[0]         INTEGER,
  msg-type[1]     MESSAGE-TYPE,
  ctime[2]        KerberosTime OPTIONAL,
  cusec[3]        INTEGER OPTIONAL,
  stime[4]        KerberosTime,
  susec[5]        INTEGER,
  error-code[6]   INTEGER,
  crealm[7]       Realm OPTIONAL,
  cname[8]        PrincipalName OPTIONAL,
  realm[9]        Realm,
  sname[10]       PrincipalName,
  e-text[11]      GeneralString OPTIONAL,
  e-data[12]      OCTET STRING OPTIONAL
}
*/

typedef struct  {
  int pvno;
  MESSAGE_TYPE msg_type;
  KerberosTime *ctime;
  int *cusec;
  KerberosTime stime;
  int susec;
  int error_code;
  Realm *crealm;
  PrincipalName *cname;
  Realm realm;
  PrincipalName sname;
  general_string *e_text;
  octet_string *e_data;
} KRB_ERROR;

int    encode_KRB_ERROR(unsigned char *, size_t, const KRB_ERROR *, size_t *);
int    decode_KRB_ERROR(const unsigned char *, size_t, KRB_ERROR *, size_t *);
void   free_KRB_ERROR  (KRB_ERROR *);
size_t length_KRB_ERROR(const KRB_ERROR *);
int    copy_KRB_ERROR  (const KRB_ERROR *, KRB_ERROR *);


enum { pvno = 5 };

enum { DOMAIN_X500_COMPRESS = 1 };

#endif /* __krb5_asn1_h__ */
