/* $Id: sectok.h,v 1.16 2003/04/04 00:50:56 deraadt Exp $ */

/*
copyright 2001
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

/* Open flags */
#define STONOWAIT	0x1	/* don't wait for card present */
#define STOHUP		0x4	/* send signal on card removal */

/* Reset flags */
#define STRV		0x1	/* be verbose */
#define STRLEN		0x2	/* determine length by examing atr */
#define STRFORCE	0x8	/* Talk to card even if atr is bad */

/* Errors */
#define STEOK		0x9000
#define STENOTTY	0x0601	/* no such tty */
#define STENOMEM	0x0602	/* malloc (or similar) failed */
#define STTIMEO		0x0603	/* time out */
#define STESLAG		0x0604	/* slag (no atr) */
#define STENOSUPP	0x0605	/* card type not supported */
#define STENOCARD	0x0606	/* no card in reader */
#define STENOIMPL	0x0607
#define STEDRVR 	0x0608
#define STECOMM 	0x0609
#define STECLOSED	0x060a
#define STECNFFILES     0x060c      /* both config path and driver path are
				   specified.  thus conflict. */
#define STEUNKNOWN	0x060d
#define STENOFILE	0x6a82

/* Useful macros */
#define sectok_r1(sw) (((sw) >> 8) & 0xff)
#define sectok_r2(sw) ((sw) & 0xff)
#define sectok_mksw(r1, r2) (((r1) << 8) | (r2))
#define sectok_swOK(sw) (sectok_r1(sw) == 0x90 || sectok_r1(sw) == 0x61)

struct scparam {
    int t, etu, cwt, bwt, n;
};

extern unsigned char root_fid[];

/* Common card functions */
int sectok_open(int rn, int flags, int *swp);
int sectok_friendly_open(const char *rn, int flags, int *swp);
int sectok_xopen(int rn, int flags, char *config_path, char *driver_path, int *swp);
int sectok_reset(int fd, int flags, unsigned char *atr, int *swp);
int sectok_apdu(int fd, int cla, int ins, int p1, int p2,
		int ilen, unsigned char *ibuf, int olen, unsigned char *obuf, int *swp);
int sectok_cardpresent(int fd);
int sectok_close(int fd);
int sectok_selectfile(int fd, int cla, unsigned char *fid, int *swp);

/* Convenience functions */
void sectok_fmt_fid(char *fname, size_t fnamelen, unsigned char *fid);
int sectok_parse_atr(int fd, int flags, unsigned char *atr, int len, struct scparam *param);
void sectok_parse_fname(char *buf, unsigned char *fid);
int sectok_parse_input(char *ibuf, unsigned char *obuf, int olen);
#ifndef __palmos__
int sectok_get_input(FILE *f, unsigned char *obuf, int omin, int olen);
int sectok_fdump_reply(FILE *f, unsigned char *p, int n, int sw);
#endif
int sectok_dump_reply(unsigned char *p, int n, int sw);
void sectok_print_sw(int sw);
char *sectok_get_sw(int sw);
char *sectok_get_ins(int ins);

/* Cyberflex functions */
int cyberflex_create_file(int fd, int cla, unsigned char *fid, int size, int ftype, int *swp);
int cyberflex_create_file_acl(int fd, int cla, unsigned char *fid, int size, int ftype, unsigned char *acl, int *swp);
int cyberflex_delete_file(int fd, int cla, unsigned char *fid, int *swp);
int cyberflex_load_rsa_pub(int fd, int cla, unsigned char *key_fid,
			   int key_len, unsigned char *key_data, int *swp);
int cyberflex_load_rsa_priv(int fd, int cla, unsigned char *key_fid,
			    int nkey_elems, int key_len, unsigned char *key_elems[],
			    int *swp);
int cyberflex_verify_AUT0(int fd, int cla, unsigned char *aut0, int aut0len);
int cyberflex_inq_class(int fd);
void cyberflex_fill_key_block (unsigned char *dst, int key_num,
			       int alg_num, unsigned char *key);
