/*	$Id: des.h,v 1.1.1.1 1995/12/14 06:52:34 tholo Exp $	*/

/* Copyright (C) 1993 Eric Young - see README for more details */
#ifndef DES_DEFS
#define DES_DEFS

#include <sys/cdefs.h>
#include <sys/types.h>

typedef unsigned char des_cblock[8];
typedef struct des_ks_struct {
	union {
		des_cblock _;
		/* make sure things are correct size on machines with
		 * 8 byte longs */
		u_int32_t pad[2];
	} ks;
#define _	ks._
} des_key_schedule[16];

#define DES_KEY_SZ 	(sizeof(des_cblock))
#define DES_ENCRYPT	1
#define DES_DECRYPT	0

#define DES_CBC_MODE	0
#define DES_PCBC_MODE	1

#if !defined(NCOMPAT)
#define C_Block des_cblock
#define Key_schedule des_key_schedule
#define ENCRYPT DES_ENCRYPT
#define DECRYPT DES_DECRYPT
#define KEY_SZ DES_KEY_SZ
#define string_to_key des_string_to_key
#define read_pw_string des_read_pw_string
#define random_key des_random_key
#define pcbc_encrypt des_pcbc_encrypt
#define set_key des_set_key
#define key_sched des_key_sched
#define ecb_encrypt des_ecb_encrypt
#define cbc_encrypt des_cbc_encrypt
#define cbc_cksum des_cbc_cksum
#define quad_cksum des_quad_cksum

/* For compatibility with the MIT lib - eay 20/05/92 */
typedef struct des_ks_struct bit_64;
#endif

extern int des_check_key;	/* defaults to false */
extern int des_rw_mode;		/* defaults to DES_PCBC_MODE */

int des_3ecb_encrypt __P((des_cblock *input,des_cblock *output,des_key_schedule ks1,des_key_schedule ks2,int encrypt));
int des_3cbc_encrypt __P((des_cblock *input,des_cblock *output,long length,des_key_schedule sk1,des_key_schedule sk2,des_cblock *ivec1,des_cblock *ivec2,int encrypt));
u_int32_t des_cbc_cksum __P((des_cblock *input,des_cblock *output,long length,des_key_schedule schedule,des_cblock *ivec));
int des_cbc_encrypt __P((des_cblock *input,des_cblock *output,long length,des_key_schedule schedule,des_cblock *ivec,int encrypt));
int des_cfb_encrypt __P((unsigned char *in,unsigned char *out,int numbits,long length,des_key_schedule schedule,des_cblock *ivec,int encrypt));
int des_ecb_encrypt __P((des_cblock *input,des_cblock *output,des_key_schedule ks,int encrypt));
int des_encrypt __P((u_int32_t *input,u_int32_t *output,des_key_schedule ks, int encrypt));
int des_enc_read __P((int fd,char *buf,int len,des_key_schedule sched,des_cblock *iv));
int des_enc_write __P((int fd,char *buf,int len,des_key_schedule sched,des_cblock *iv));
int des_ofb_encrypt __P((unsigned char *in,unsigned char *out,int numbits,long length,des_key_schedule schedule,des_cblock *ivec));
int des_pcbc_encrypt __P((des_cblock *input,des_cblock *output,long length,des_key_schedule schedule,des_cblock *ivec,int encrypt));

void des_set_odd_parity __P((des_cblock *key));
int des_is_weak_key __P((des_cblock *key));
int des_set_key __P((des_cblock *key,des_key_schedule schedule));
int des_key_sched __P((des_cblock *key,des_key_schedule schedule));

int des_string_to_key __P((char *str,des_cblock *key));
int des_string_to_2keys __P((char *str,des_cblock *key1,des_cblock *key2));

void des_set_random_generator_seed __P((des_cblock *seed));
int des_new_random_key __P((des_cblock *key));
void des_init_random_number_generator __P((des_cblock *seed));
void des_random_key __P((des_cblock ret));
int des_read_password __P((des_cblock *key,char *prompt,int verify));
int des_read_2passwords __P((des_cblock *key1,des_cblock *key2,char *prompt,int verify));
int des_read_pw_string __P((char *buf,int length,char *prompt,int verify));

u_int32_t des_quad_cksum __P((des_cblock *input,des_cblock *output,long length,int out_count,des_cblock *seed));

/* MIT Link and source compatibility */
void des_fixup_key_parity __P((des_cblock *key));
#define des_fixup_key_parity des_set_odd_parity

#endif /* DES_DEFS */
