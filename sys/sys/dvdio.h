/* $OpenBSD: dvdio.h,v 1.1 1999/11/03 01:47:47 angelos Exp $ */

#include <sys/types.h>
#include <sys/ioccom.h>

/* DVD-ROM Specific ioctls */
#define DVD_READ_STRUCT         _IOWR('d', 0, dvd_struct)
#define DVD_WRITE_STRUCT        _IOWR('d', 1, dvd_struct)
#define DVD_AUTH                _IOWR('d', 2, dvd_authinfo)

#define GPCMD_READ_DVD_STRUCTURE            0xad
#define GPCMD_SEND_DVD_STRUCTURE            0xad
#define GPCMD_REPORT_KEY                    0xa4
#define GPCMD_SEND_KEY                      0xa3

/* DVD struct types */
#define DVD_STRUCT_PHYSICAL     0x00
#define DVD_STRUCT_COPYRIGHT    0x01
#define DVD_STRUCT_DISCKEY      0x02
#define DVD_STRUCT_BCA          0x03
#define DVD_STRUCT_MANUFACT     0x04

struct dvd_layer {
        u_int8_t book_version       : 4;
        u_int8_t book_type  : 4;
        u_int8_t min_rate           : 4;
        u_int8_t disc_size  : 4;
        u_int8_t layer_type : 4;
        u_int8_t track_path : 1;
        u_int8_t nlayers            : 2;
        u_int8_t track_density      : 4;
        u_int8_t linear_density     : 4;
        u_int8_t bca                : 1;
        u_int32_t start_sector;
        u_int32_t end_sector;
        u_int32_t end_sector_l0;
};
 
struct dvd_physical {
        u_int8_t type;
        u_int8_t layer_num;
        struct dvd_layer layer[4];
};

struct dvd_copyright {
        u_int8_t type;

        u_int8_t layer_num;
        u_int8_t cpst;
        u_int8_t rmi;
};

struct dvd_disckey {
        u_int8_t type;

        unsigned agid                   : 2;
        u_int8_t value[2048];
};

struct dvd_bca {
        u_int8_t type;

        int len;
        u_int8_t value[188];
};

struct dvd_manufact {
        u_int8_t type;

        u_int8_t layer_num;
        int len;
        u_int8_t value[2048];
};

typedef union {
        u_int8_t type;

        struct dvd_physical     physical;
        struct dvd_copyright    copyright;
        struct dvd_disckey      disckey;
        struct dvd_bca          bca;
        struct dvd_manufact     manufact;
} dvd_struct;

/*
 * DVD authentication ioctl
 */

/* Authentication states */
#define DVD_LU_SEND_AGID        0
#define DVD_HOST_SEND_CHALLENGE 1
#define DVD_LU_SEND_KEY1        2
#define DVD_LU_SEND_CHALLENGE   3
#define DVD_HOST_SEND_KEY2      4

/* Termination states */
#define DVD_AUTH_ESTABLISHED    5
#define DVD_AUTH_FAILURE        6

/* Other functions */
#define DVD_LU_SEND_TITLE_KEY   7
#define DVD_LU_SEND_ASF         8
#define DVD_INVALIDATE_AGID     9

/* State data */
typedef u_int8_t dvd_key[5];                /* 40-bit value, MSB is first elem. */
typedef u_int8_t dvd_challenge[10]; /* 80-bit value, MSB is first elem. */

struct dvd_lu_send_agid {
        u_int8_t type;
        unsigned agid           : 2;
};

struct dvd_host_send_challenge {
        u_int8_t type;
        unsigned agid           : 2;

        dvd_challenge chal;
};

struct dvd_send_key {
        u_int8_t type;
        unsigned agid           : 2;

        dvd_key key;
};

struct dvd_lu_send_challenge {
        u_int8_t type;
        unsigned agid           : 2;

        dvd_challenge chal;
};

#define DVD_CPM_NO_COPYRIGHT    0
#define DVD_CPM_COPYRIGHTED     1

#define DVD_CP_SEC_NONE         0
#define DVD_CP_SEC_EXIST        1

#define DVD_CGMS_UNRESTRICTED   0
#define DVD_CGMS_SINGLE         2
#define DVD_CGMS_RESTRICTED     3

struct dvd_lu_send_title_key {
        u_int8_t type;
        unsigned agid           : 2;

        dvd_key title_key;
        int lba;
        unsigned cpm            : 1;
        unsigned cp_sec         : 1;
        unsigned cgms           : 2;
};

struct dvd_lu_send_asf {
        u_int8_t type;
        unsigned agid           : 2;

        unsigned asf            : 1;
};

typedef union {
        u_int8_t type;

        struct dvd_lu_send_agid         lsa;
        struct dvd_host_send_challenge  hsc;
        struct dvd_send_key             lsk;
        struct dvd_lu_send_challenge    lsc;
        struct dvd_send_key             hsk;
        struct dvd_lu_send_title_key    lstk;
        struct dvd_lu_send_asf          lsasf;
} dvd_authinfo;
