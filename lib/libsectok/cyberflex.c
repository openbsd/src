/* $Id: cyberflex.c,v 1.10 2001/08/02 17:02:05 rees Exp $ */

/*
copyright 2000
the regents of the university of michigan
all rights reserved

permission is granted to use, copy, create derivative works
and redistribute this software and such derivative works
for any purpose, so long as the name of the university of
michigan is not used in any advertising or publicity
pertaining to the use or distribution of this software
without specific, written prior authorization.  if the
above copyright notice or any other identification of the
university of michigan is included in any copy of any
portion of this software, then the disclaimer below must
also be included.

this software is provided as is, without representation
from the university of michigan as to its fitness for any
purpose, and without warranty by the university of
michigan of any kind, either express or implied, including
without limitation the implied warranties of
merchantability and fitness for a particular purpose. the
regents of the university of michigan shall not be liable
for any damages, including special, indirect, incidental, or
consequential damages, with respect to any claim arising
out of or in connection with the use of the software, even
if it has been or is hereafter advised of the possibility of
such damages.
*/

/*
 * Cyberflex routines
 *
 * University of Michigan CITI, July 2001
 */

#ifdef __palmos__
#include <Common.h>
#include <System/SysAll.h>
#include <System/Unix/unix_stdlib.h>
#include <System/Unix/unix_string.h>
#include <UI/UIAll.h>
#include "field.h"
#else
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#endif

#include "sectok.h"

#define MAX_APDU_SIZE 0xfa
#define MAX_KEY_FILE_SIZE 1024
#define PRV_KEY_SIZE 64*6
#define key_number 0x10
#define key_type 0xc8 /* key type 0xc8 (1024 bit RSA private) */
#define KEY_FILE_HEADER_SIZE 8
#define BLOCK_SIZE 8

int
cyberflex_create_file_acl(int fd, int cla, unsigned char *fid, int size, int ftype, unsigned char *acl, int *swp)
{
    unsigned char data[16];

    size += 16;

    data[0] = (size >> 8);
    data[1] = (size & 0xff);
    data[2] = fid[0];
    data[3] = fid[1];
    data[4] = ftype;
    data[5] = 0x01;		/* status = 1 */
    data[6] = data[7] = 0x00;	/* record related */
    memcpy(&data[8], acl, 8);

    sectok_apdu(fd, cla, 0xe0, 0, 0, 0x10, data, 0, NULL, swp);
    if (!sectok_swOK(*swp))
	return -1;

    return sectok_selectfile(fd, cla, fid, swp);
}

/* Create a file with default acl "world: r w x/a inval rehab dec inc" */

int
cyberflex_create_file(int fd, int cla, unsigned char *fid, int size, int ftype, int *swp)
{
    static unsigned char acl[] = {0xff, 0, 0, 0, 0, 0, 0, 0};

    return cyberflex_create_file_acl(fd, cla, fid, size, ftype, acl, swp);
}

int
cyberflex_delete_file(int fd, int cla, unsigned char *fid, int *swp)
{
    sectok_apdu(fd, cla, 0xe4, 0, 0, 0x02, fid, 0, NULL, swp);
    if (!sectok_swOK(*swp))
	return -1;

    return 0;
}

int
cyberflex_load_rsa_pub(int fd, int cla, unsigned char *key_fid,
		       int key_len, unsigned char *key_data, int *swp)
{
    static unsigned char acl[] = {0x1, 0, 0, 0xb, 0, 0, 0, 0};

    if (sectok_selectfile(fd, cla, root_fid, swp) < 0)
	return -1;

    if (sectok_selectfile(fd, cla, key_fid, swp) < 0 && *swp == STENOFILE) {
	if (cyberflex_create_file_acl(fd, cla, key_fid, key_len, 3, acl, swp) < 0)
	    return -1;
    }

    /* Write the key data */
    sectok_apdu(fd, cla, 0xd6, 0, 0, key_len, key_data, 0, NULL, swp);
    if (!sectok_swOK(*swp))
	return -1;

    return 0;
}

/* download RSA private key into 3f.00/00.12 */
int
cyberflex_load_rsa_priv(int fd, int cla, unsigned char *key_fid,
			int nkey_elems, int key_len, unsigned char *key_elems[],
			int *swp)
{
    int i, j, offset = 0, size;
    unsigned char data[MAX_KEY_FILE_SIZE];
    static unsigned char acl[] = {0, 0, 0, 0xa, 0, 0, 0, 0}; /* AUT0: w inval */
    static unsigned char key_file_header[KEY_FILE_HEADER_SIZE] =
    {0xC2, 0x06, 0xC1, 0x08, 0x13, 0x00, 0x00, 0x05};
    static unsigned char key_header[3] = {0xC2, 0x41, 0x00};

    /* select 3f.00 */
    if (sectok_selectfile(fd, cla, root_fid, swp) < 0)
	return -1;

    /* select 00.12 */
    if (sectok_selectfile(fd, cla, key_fid, swp) < 0 && *swp == STENOFILE) {
	/* rv != 0, 00.12 does not exist.  create it. */
	if (cyberflex_create_file_acl(fd, cla, key_fid, PRV_KEY_SIZE, 3, acl, swp) < 0)
	    return -1;
    }

    /* burn the key */
    data[0] = 0x01;		/* key size, I guess */
    data[1] = 0x5b;		/* key size, I guess */
    data[2] = key_number;	/* key number */
    data[3] = key_type;
    offset = 4;
    for (j = 0 ; j < KEY_FILE_HEADER_SIZE ; j ++)
	data[offset++] = key_file_header[j];
    for (i = 0 ; i < nkey_elems; i ++) {
	/* put the key header */
	for (j = 0 ; j < 3 ; j ++) {
	    data[offset++] = key_header[j];
	}
	for (j = 0 ; j < key_len/2/8 ; j ++) {
	    data[offset++] = key_elems [i][j];
	}
    }
    for (j = 0 ; j < 2 ; j ++) data[offset++] = 0;

#ifdef DEBUG
    printf ("data:\n");
    for (i = 0 ; i < 0x015d; i ++) {
	printf ("%02x ", data[i]);
    }
    printf ("\n");
#endif

    /* now send this to the card */
    /* select private key file */
    if (sectok_selectfile(fd, cla, key_fid, swp) < 0)
	return -1;

    /* update binary */
    size = offset;

    for (i = 0; i < size; i += MAX_APDU_SIZE) {
	int send_size;

	/* compute the size to be sent */
	if (size - i > MAX_APDU_SIZE) send_size = MAX_APDU_SIZE;
	else send_size = size - i;

	sectok_apdu(fd, cla, 0xd6, i >> 8, i & 0xff, send_size, data + i, 0, NULL, swp);

	if (!sectok_swOK(*swp))
	    return -1;
    }

    return 0;
}

int
cyberflex_verify_AUT0(int fd, int cla, unsigned char *aut0, int aut0len)
{
    int sw;

    sectok_apdu(fd, cla, 0x2a, 0, 0, aut0len, aut0, 0, NULL, &sw);
    if (!sectok_swOK(sw))
	return -1;

    return 0;
}

/* fill the key block.

   Input
   dst     : destination buffer
   key_num : key number (0: AUT, 5: signed applet, etc.)
   alg_num : algorithm number
   key     : incoming 8 byte DES key

   The resulting format:
   00 0e key_num alg_num key(8 byte) 0a 0a

   total 14 byte
*/
void
cyberflex_fill_key_block (unsigned char *dst, int key_num,
			       int alg_num, unsigned char *key)
{
    int i;

    *(dst+0) = 0x00;		/* const */
    *(dst+1) = 0x0e;		/* const */
    *(dst+2) = key_num;		/* key number */
    *(dst+3) = alg_num;		/* algorithm number */
    for (i = 0; i < BLOCK_SIZE; i++)
	*(dst+i+4) = *(key+i);
    *(dst+12) = 0x0a;		/* const */
    *(dst+13) = 0x0a;		/* const */

    return;
}

int
cyberflex_inq_class(int fd)
{
    unsigned char buf[32];
    int n, sw;

    n = sectok_apdu(fd, 0x00, 0xca, 0, 1, 0, NULL, 0x16, buf, &sw);
    if (sectok_swOK(sw))
	return 0x00;

    if (n >= 0 && sectok_r1(sw) == 0x6d) {
        /* F0 card? */
        sectok_apdu(fd, 0xf0, 0xca, 0, 1, 0, NULL, 0x16, buf, &sw);
        if (sectok_swOK(sw))
	    return 0xf0;
    }

    return -1;
}
