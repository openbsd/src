/* $OpenBSD: math_group.c,v 1.23 2004/06/14 09:55:41 ho Exp $	 */
/* $EOM: math_group.c,v 1.25 2000/04/07 19:53:26 niklas Exp $	 */

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This code was written under funding by Ericsson Radio Systems.
 */

#include <sys/param.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "gmp_util.h"
#include "log.h"
#include "math_2n.h"
#include "math_ec2n.h"
#include "math_group.h"
#include "math_mp.h"

/* We do not want to export these definitions.  */
int	modp_getlen(struct group *);
void	modp_getraw(struct group *, math_mp_t, u_int8_t *);
int	modp_setraw(struct group *, math_mp_t, u_int8_t *, int);
int	modp_setrandom(struct group *, math_mp_t);
int	modp_operation(struct group *, math_mp_t, math_mp_t, math_mp_t);

int	ec2n_getlen(struct group *);
void	ec2n_getraw(struct group *, ec2np_ptr, u_int8_t *);
int	ec2n_setraw(struct group *, ec2np_ptr, u_int8_t *, int);
int	ec2n_setrandom(struct group *, ec2np_ptr);
int	ec2n_operation(struct group *, ec2np_ptr, ec2np_ptr, ec2np_ptr);

struct ec2n_group {
	ec2np_t         gen;	/* Generator */
	ec2ng_t         grp;
	ec2np_t         a, b, c, d;
};

struct modp_group {
	math_mp_t       gen;	/* Generator */
	math_mp_t       p;	/* Prime */
	math_mp_t       a, b, c, d;
};

/*
 * This module provides access to the operations on the specified group
 * and is absolutly free of any cryptographic devices. This is math :-).
 */

#define OAKLEY_GRP_1	1
#define OAKLEY_GRP_2	2
#define OAKLEY_GRP_3	3
#define OAKLEY_GRP_4	4
#define OAKLEY_GRP_5	5
#define OAKLEY_GRP_6	6
#define OAKLEY_GRP_7	7
#define OAKLEY_GRP_8	8
#define OAKLEY_GRP_9	9
#define OAKLEY_GRP_10	10
#define OAKLEY_GRP_11	11
#define OAKLEY_GRP_12	12
#define OAKLEY_GRP_13	13
#define OAKLEY_GRP_14	14
#define OAKLEY_GRP_15	15
#define OAKLEY_GRP_16	16
#define OAKLEY_GRP_17	17
#define OAKLEY_GRP_18	18

/* Describe preconfigured MODP groups */

/*
 * The Generalized Number Field Sieve has an asymptotic running time
 * of: O(exp(1.9223 * (ln q)^(1/3) (ln ln q)^(2/3))), where q is the
 * group order, e.g. q = 2**768.
 */

struct modp_dscr oakley_modp[] =
{
	{OAKLEY_GRP_1, 72,	/* This group is insecure, only sufficient
				 * for DES */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A63A3620FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_2, 82,	/* This group is a bit better */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381"
		"FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_5, 102,
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_14, 135,	/* 2048 bit */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
		"E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
		"DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
		"15728E5A8AACAA68FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_15, 170,	/* 3072 bit */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
		"E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
		"DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
		"15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
		"ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
		"ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
		"F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
		"43DB5BFCE0FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_16, 195,	/* 4096 bit */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
		"E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
		"DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
		"15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
		"ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
		"ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
		"F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
		"43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
		"88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
		"2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
		"287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
		"1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
		"93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934063199"
		"FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_17, 220,	/* 6144 bit */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
		"E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
		"DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
		"15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
		"ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
		"ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
		"F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
		"43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
		"88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
		"2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
		"287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
		"1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
		"93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492"
		"36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD"
		"F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831"
		"179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
		"DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF"
		"5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6"
		"D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3"
		"23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA"
		"CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328"
		"06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C"
		"DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE"
		"12BF2D5B0B7474D6E694F91E6DCC4024FFFFFFFFFFFFFFFF",
		"0x02"
	},
	{OAKLEY_GRP_18, 250,	/* 8192 bit */
		"0xFFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1"
		"29024E088A67CC74020BBEA63B139B22514A08798E3404DD"
		"EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245"
		"E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED"
		"EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D"
		"C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F"
		"83655D23DCA3AD961C62F356208552BB9ED529077096966D"
		"670C354E4ABC9804F1746C08CA18217C32905E462E36CE3B"
		"E39E772C180E86039B2783A2EC07A28FB5C55DF06F4C52C9"
		"DE2BCBF6955817183995497CEA956AE515D2261898FA0510"
		"15728E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64"
		"ECFB850458DBEF0A8AEA71575D060C7DB3970F85A6E1E4C7"
		"ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6B"
		"F12FFA06D98A0864D87602733EC86A64521F2B18177B200C"
		"BBE117577A615D6C770988C0BAD946E208E24FA074E5AB31"
		"43DB5BFCE0FD108E4B82D120A92108011A723C12A787E6D7"
		"88719A10BDBA5B2699C327186AF4E23C1A946834B6150BDA"
		"2583E9CA2AD44CE8DBBBC2DB04DE8EF92E8EFC141FBECAA6"
		"287C59474E6BC05D99B2964FA090C3A2233BA186515BE7ED"
		"1F612970CEE2D7AFB81BDD762170481CD0069127D5B05AA9"
		"93B4EA988D8FDDC186FFB7DC90A6C08F4DF435C934028492"
		"36C3FAB4D27C7026C1D4DCB2602646DEC9751E763DBA37BD"
		"F8FF9406AD9E530EE5DB382F413001AEB06A53ED9027D831"
		"179727B0865A8918DA3EDBEBCF9B14ED44CE6CBACED4BB1B"
		"DB7F1447E6CC254B332051512BD7AF426FB8F401378CD2BF"
		"5983CA01C64B92ECF032EA15D1721D03F482D7CE6E74FEF6"
		"D55E702F46980C82B5A84031900B1C9E59E7C97FBEC7E8F3"
		"23A97A7E36CC88BE0F1D45B7FF585AC54BD407B22B4154AA"
		"CC8F6D7EBF48E1D814CC5ED20F8037E0A79715EEF29BE328"
		"06A1D58BB7C5DA76F550AA3D8A1FBFF0EB19CCB1A313D55C"
		"DA56C9EC2EF29632387FE8D76E3C0468043E8F663F4860EE"
		"12BF2D5B0B7474D6E694F91E6DBE115974A3926F12FEE5E4"
		"38777CB6A932DF8CD8BEC4D073B931BA3BC832B68D9DD300"
		"741FA7BF8AFC47ED2576F6936BA424663AAB639C5AE4F568"
		"3423B4742BF1C978238F16CBE39D652DE3FDB8BEFC848AD9"
		"22222E04A4037C0713EB57A81A23F0C73473FC646CEA306B"
		"4BCBC8862F8385DDFA9D4B7FA2C087E879683303ED5BDD3A"
		"062B3CF5B3A278A66D2A13F83F44F82DDF310EE074AB6A36"
		"4597E899A0255DC164F31CC50846851DF9AB48195DED7EA1"
		"B1D510BD7EE74D73FAF36BC31ECFA268359046F4EB879F92"
		"4009438B481C6CD7889A002ED5EE382BC9190DA6FC026E47"
		"9558E4475677E9AA9E3050E2765694DFC81F56E880B96E71"
		"60C980DD98EDD3DFFFFFFFFFFFFFFFFF",
		"0x02"
	},
};

#ifdef USE_EC
/* Describe preconfigured EC2N groups */

/*
 * Related collision-search methods can compute discrete logarithmns
 * in O(sqrt(r)), r being the subgroup order.
 */

struct ec2n_dscr oakley_ec2n[] = {
	{ OAKLEY_GRP_3, 76,	/* This group is also considered insecure
				 * (P1363) */
	"0x0800000000000000000000004000000000000001",
	"0x7b",
	"0x00",
	"0x7338f" },
	{ OAKLEY_GRP_4, 91,
	"0x020000000000000000000000000000200000000000000001",
	"0x18",
	"0x00",
	"0x1ee9" },
};
#endif				/* USE_EC */

/* XXX I want to get rid of the casting here.  */
struct group    groups[] = {
	{
		MODP, OAKLEY_GRP_1, 0, &oakley_modp[0], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
	{
		MODP, OAKLEY_GRP_2, 0, &oakley_modp[1], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
#ifdef USE_EC
	{
		EC2N, OAKLEY_GRP_3, 0, &oakley_ec2n[0], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) ec2n_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) ec2n_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) ec2n_setraw,
		(int (*) (struct group *, void *)) ec2n_setrandom,
		(int (*) (struct group *, void *, void *, void *)) ec2n_operation
	},
	{
		EC2N, OAKLEY_GRP_4, 0, &oakley_ec2n[1], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) ec2n_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) ec2n_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) ec2n_setraw,
		(int (*) (struct group *, void *)) ec2n_setrandom,
		(int (*) (struct group *, void *, void *, void *)) ec2n_operation
	},
#endif				/* USE_EC */
	{
		MODP, OAKLEY_GRP_5, 0, &oakley_modp[2], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
#ifdef USE_EC
	/* XXX Higher EC2N group go here... */
#endif				/* USE_EC */
	/* XXX group 6 to 13 are not yet defined (draft-ike-ecc) */
	{
		NOTYET, OAKLEY_GRP_6, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_7, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_8, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_9, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_10, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_11, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_12, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		NOTYET, OAKLEY_GRP_13, 0, NULL, 0, 0, 0, 0, 0,
		NULL, NULL, NULL, NULL, NULL
	},
	{
		MODP, OAKLEY_GRP_14, 0, &oakley_modp[3], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
	{
		MODP, OAKLEY_GRP_15, 0, &oakley_modp[4], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
	{
		MODP, OAKLEY_GRP_16, 0, &oakley_modp[5], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
	{
		MODP, OAKLEY_GRP_17, 0, &oakley_modp[6], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
	{
		MODP, OAKLEY_GRP_18, 0, &oakley_modp[7], 0, 0, 0, 0, 0,
		(int (*) (struct group *)) modp_getlen,
		(void (*) (struct group *, void *, u_int8_t *)) modp_getraw,
		(int (*) (struct group *, void *, u_int8_t *, int)) modp_setraw,
		(int (*) (struct group *, void *)) modp_setrandom,
		(int (*) (struct group *, void *, void *, void *)) modp_operation
	},
};

/*
 * Initialize the group structure for later use,
 * this is done by converting the values given in the describtion
 * and converting them to their native representation.
 */
void
group_init(void)
{
	int	i;

	for (i = sizeof(groups) / sizeof(groups[0]) - 1; i >= 0; i--)
		switch (groups[i].type) {
#ifdef USE_EC
		case EC2N:  /* Initialize an Elliptic Curve over GF(2**n) */
			ec2n_init(&groups[i]);
			break;
#endif

		case MODP:	/* Initialize an over GF(p) */
			modp_init(&groups[i]);
			break;

		case NOTYET:	/* Not yet assigned, drop silently */
			break;

		default:
			log_print("Unknown group type %d at index %d in "
			    "group_init().", groups[i].type, i);
			break;
		}
}

struct group *
group_get(u_int32_t id)
{
	struct group   *new, *clone;

	if (id < 1 || id > (sizeof(groups) / sizeof(groups[0]))) {
		log_print("group_get: group ID (%u) out of range", id);
		return 0;
	}
	clone = &groups[id - 1];

	new = malloc(sizeof *new);
	if (!new) {
		log_error("group_get: malloc (%lu) failed",
		    (unsigned long)sizeof *new);
		return 0;
	}
	switch (clone->type) {
#ifdef USE_EC
	case EC2N:
		new = ec2n_clone(new, clone);
		break;
#endif
	case MODP:
		new = modp_clone(new, clone);
		break;
	default:
		log_print("group_get: unknown group type %d", clone->type);
		free(new);
		return 0;
	}
	LOG_DBG((LOG_MISC, 70, "group_get: returning %p of group %d", new,
		 new->id));
	return new;
}

void
group_free(struct group *grp)
{
	switch (grp->type) {
#ifdef USE_EC
		case EC2N:
		ec2n_free(grp);
		break;
#endif
	case MODP:
		modp_free(grp);
		break;
	default:
		log_print("group_free: unknown group type %d", grp->type);
		break;
	}
	free(grp);
}

struct group *
modp_clone(struct group *new, struct group *clone)
{
	struct modp_group *new_grp, *clone_grp = clone->group;

	new_grp = malloc(sizeof *new_grp);
	if (!new_grp) {
		log_print("modp_clone: malloc (%lu) failed",
		    (unsigned long)sizeof *new_grp);
		free(new);
		return 0;
	}
	memcpy(new, clone, sizeof(struct group));

	new->group = new_grp;
#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_init_set(new_grp->p, clone_grp->p);
	mpz_init_set(new_grp->gen, clone_grp->gen);

	mpz_init(new_grp->a);
	mpz_init(new_grp->b);
	mpz_init(new_grp->c);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	new_grp->p = BN_dup(clone_grp->p);
	new_grp->gen = BN_dup(clone_grp->gen);

	new_grp->a = BN_new();
	new_grp->b = BN_new();
	new_grp->c = BN_new();
#endif

	new->gen = new_grp->gen;
	new->a = new_grp->a;
	new->b = new_grp->b;
	new->c = new_grp->c;

	return new;
}

void
modp_free(struct group *old)
{
	struct modp_group *grp = old->group;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_clear(grp->p);
	mpz_clear(grp->gen);
	mpz_clear(grp->a);
	mpz_clear(grp->b);
	mpz_clear(grp->c);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	BN_clear_free(grp->p);
	BN_clear_free(grp->gen);
	BN_clear_free(grp->a);
	BN_clear_free(grp->b);
	BN_clear_free(grp->c);
#endif

	free(grp);
}

void
modp_init(struct group *group)
{
	struct modp_dscr *dscr = (struct modp_dscr *)group->group;
	struct modp_group *grp;

	grp = malloc(sizeof *grp);
	if (!grp)
		log_fatal("modp_init: malloc (%lu) failed",
		    (unsigned long)sizeof *grp);

	group->bits = dscr->bits;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_init_set_str(grp->p, dscr->prime, 0);
	mpz_init_set_str(grp->gen, dscr->gen, 0);

	mpz_init(grp->a);
	mpz_init(grp->b);
	mpz_init(grp->c);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	grp->p = BN_new();
	BN_hex2bn(&grp->p, dscr->prime + 2);
	grp->gen = BN_new();
	BN_hex2bn(&grp->gen, dscr->gen + 2);

	grp->a = BN_new();
	grp->b = BN_new();
	grp->c = BN_new();
#endif

	group->gen = grp->gen;
	group->a = grp->a;
	group->b = grp->b;
	group->c = grp->c;

	group->group = grp;
}

#ifdef USE_EC
struct group *
ec2n_clone(struct group *new, struct group *clone)
{
	struct ec2n_group *new_grp, *clone_grp = clone->group;

	new_grp = malloc(sizeof *new_grp);
	if (!new_grp) {
		log_error("ec2n_clone: malloc (%lu) failed",
		    (unsigned long)sizeof *new_grp);
		free(new);
		return 0;
	}
	memcpy(new, clone, sizeof(struct group));

	new->group = new_grp;
	ec2ng_init(new_grp->grp);
	ec2np_init(new_grp->gen);
	ec2np_init(new_grp->a);
	ec2np_init(new_grp->b);
	ec2np_init(new_grp->c);

	if (ec2ng_set(new_grp->grp, clone_grp->grp))
		goto fail;
	if (ec2np_set(new_grp->gen, clone_grp->gen))
		goto fail;

	new->gen = new_grp->gen;
	new->a = new_grp->a;
	new->b = new_grp->b;
	new->c = new_grp->c;
	new->d = ((ec2np_ptr) new->a)->x;

	return new;

fail:
	ec2ng_clear(new_grp->grp);
	ec2np_clear(new_grp->gen);
	ec2np_clear(new_grp->a);
	ec2np_clear(new_grp->b);
	ec2np_clear(new_grp->c);
	free(new_grp);
	free(new);
	return 0;
}

void
ec2n_free(struct group *old)
{
	struct ec2n_group *grp = old->group;

	ec2ng_clear(grp->grp);
	ec2np_clear(grp->gen);
	ec2np_clear(grp->a);
	ec2np_clear(grp->b);
	ec2np_clear(grp->c);

	free(grp);
}

void
ec2n_init(struct group *group)
{
	struct ec2n_dscr *dscr = (struct ec2n_dscr *)group->group;
	struct ec2n_group *grp;

	grp = malloc(sizeof *grp);
	if (!grp)
		log_fatal("ec2n_init: malloc (%lu) failed",
		    (unsigned long)sizeof *grp);

	group->bits = dscr->bits;

	ec2ng_init(grp->grp);
	ec2np_init(grp->gen);
	ec2np_init(grp->a);
	ec2np_init(grp->b);
	ec2np_init(grp->c);

	if (ec2ng_set_p_str(grp->grp, dscr->polynomial))
		goto fail;
	grp->grp->p->bits = b2n_sigbit(grp->grp->p);
	if (ec2ng_set_a_str(grp->grp, dscr->a))
		goto fail;
	if (ec2ng_set_b_str(grp->grp, dscr->b))
		goto fail;

	if (ec2np_set_x_str(grp->gen, dscr->gen_x))
		goto fail;
	if (ec2np_find_y(grp->gen, grp->grp))
		goto fail;

	/* Sanity check */
	if (!ec2np_ison(grp->gen, grp->grp))
		log_fatal("ec2n_init: generator is not on curve");

	group->gen = grp->gen;
	group->a = grp->a;
	group->b = grp->b;
	group->c = grp->c;
	group->d = ((ec2np_ptr) group->a)->x;

	group->group = grp;
	return;

fail:
	log_fatal("ec2n_init: general failure");
}
#endif				/* USE_EC */

int
modp_getlen(struct group *group)
{
	struct modp_group *grp = (struct modp_group *)group->group;

	return mpz_sizeinoctets(grp->p);
}

void
modp_getraw(struct group *grp, math_mp_t v, u_int8_t *d)
{
	mpz_getraw(d, v, grp->getlen(grp));
}

int
modp_setraw(struct group *grp, math_mp_t d, u_int8_t *s, int l)
{
	mpz_setraw(d, s, l);
	return 0;
}

int
modp_setrandom(struct group *grp, math_mp_t d)
{
	int             i, l = grp->getlen(grp);
	u_int32_t       tmp = 0;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_set_ui(d, 0);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	BN_set_word(d, 0);
#endif

	for (i = 0; i < l; i++) {
		if (i % 4)
			tmp = sysdep_random();

#if MP_FLAVOUR == MP_FLAVOUR_GMP
		mpz_mul_2exp(d, d, 8);
		mpz_add_ui(d, d, tmp & 0xFF);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
		BN_lshift(d, d, 8);
		BN_add_word(d, tmp & 0xFF);
#endif
		tmp >>= 8;
	}
	return 0;
}

int
modp_operation(struct group *group, math_mp_t d, math_mp_t a, math_mp_t e)
{
	struct modp_group *grp = (struct modp_group *)group->group;

#if MP_FLAVOUR == MP_FLAVOUR_GMP
	mpz_powm(d, a, e, grp->p);
#elif MP_FLAVOUR == MP_FLAVOUR_OPENSSL
	BN_CTX         *ctx = BN_CTX_new();
	BN_mod_exp(d, a, e, grp->p, ctx);
	BN_CTX_free(ctx);
#endif
	return 0;
}

#ifdef USE_EC
int
ec2n_getlen(struct group *group)
{
	struct ec2n_group *grp = (struct ec2n_group *)group->group;
	int		bits = b2n_sigbit(grp->grp->p) - 1;

	return (7 + bits) >> 3;
}

void
ec2n_getraw(struct group *group, ec2np_ptr xo, u_int8_t *e)
{
	struct ec2n_group *grp = (struct ec2n_group *) group->group;
	int             chunks, bytes, i, j;
	b2n_ptr         x = xo->x;
	CHUNK_TYPE      tmp;

	bytes = b2n_sigbit(grp->grp->p) - 1;
	chunks = (CHUNK_MASK + bytes) >> CHUNK_SHIFTS;
	bytes = ((7 + (bytes & CHUNK_MASK)) >> 3);

	for (i = chunks - 1; i >= 0; i--) {
		tmp = (i >= x->chunks ? 0 : x->limp[i]);
		for (j = (i == chunks - 1 ? bytes : CHUNK_BYTES) - 1; j >= 0;
		    j--) {
			e[j] = tmp & 0xff;
			tmp >>= 8;
		}
		e += (i == chunks - 1 ? bytes : CHUNK_BYTES);
	}
}

int
ec2n_setraw(struct group *grp, ec2np_ptr out, u_int8_t *s, int l)
{
	int             len, bytes, i, j;
	b2n_ptr         outx = out->x;
	CHUNK_TYPE      tmp;

	len = (CHUNK_BYTES - 1 + l) / CHUNK_BYTES;
	if (b2n_resize(outx, len))
		return -1;

	bytes = ((l - 1) % CHUNK_BYTES) + 1;

	for (i = len - 1; i >= 0; i--) {
		tmp = 0;
		for (j = (i == len - 1 ? bytes : CHUNK_BYTES); j > 0; j--) {
			tmp <<= 8;
			tmp |= *s++;
		}
		outx->limp[i] = tmp;
	}
	return 0;
}

int
ec2n_setrandom(struct group *group, ec2np_ptr x)
{
	b2n_ptr         d = x->x;
	struct ec2n_group *grp = (struct ec2n_group *) group->group;

	return b2n_random(d, b2n_sigbit(grp->grp->p) - 1);
}

/*
 * This is an attempt at operation abstraction. It can happen
 * that we need to initialize the y variable for the operation
 * to proceed correctly. When this is the case operation has
 * to supply the variable 'a' with the chunks of the Y cooridnate
 * set to zero.
 */
int
ec2n_operation(struct group *grp, ec2np_ptr d, ec2np_ptr a, ec2np_ptr e)
{
	b2n_ptr         ex = e->x;
	struct ec2n_group *group = (struct ec2n_group *)grp->group;

	if (a->y->chunks == 0)
		if (ec2np_find_y(a, group->grp))
			return -1;

	return ec2np_mul(d, a, ex, group->grp);
}
#endif				/* USE_EC */
