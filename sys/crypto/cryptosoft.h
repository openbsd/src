/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * This code was written by Angelos D. Keromytis in Athens, Greece, in
 * February 2000. Network Security Technologies Inc. (NSTI) kindly
 * supported the development of this code.
 *
 * Copyright (c) 2000 Angelos D. Keromytis
 *
 * Permission to use, copy, and modify this software without fee
 * is hereby granted, provided that this entire notice is included in
 * all source code copies of any software which is or includes a copy or
 * modification of this software. 
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#ifndef _CRYPTO_CRYPTOSOFT_H_
#define _CRYPTO_CRYPTOSOFT_H_

/* Software session entry */
struct swcr_data
{
    int               sw_alg;		/* Algorithm */
    union
    {
	struct
	{
            u_int8_t         *SW_ictx;
            u_int8_t         *SW_octx;
	    struct auth_hash *SW_axf;
	} SWCR_AUTH;

	struct
	{
            u_int8_t         *SW_kschedule;
            u_int8_t         *SW_iv;
	    struct enc_xform *SW_exf;
	} SWCR_ENC;
    } SWCR_UN;

#define sw_ictx      SWCR_UN.SWCR_AUTH.SW_ictx
#define sw_octx      SWCR_UN.SWCR_AUTH.SW_octx
#define sw_axf       SWCR_UN.SWCR_AUTH.SW_axf
#define sw_kschedule SWCR_UN.SWCR_ENC.SW_kschedule
#define sw_iv        SWCR_UN.SWCR_ENC.SW_iv
#define sw_exf       SWCR_UN.SWCR_ENC.SW_exf

    struct swcr_data *sw_next;
};

#ifdef _KERNEL
extern u_int8_t hmac_ipad_buffer[64];
extern u_int8_t hmac_opad_buffer[64];

extern int swcr_encdec(struct cryptodesc *, struct swcr_data *, caddr_t, int);
extern int swcr_authcompute(struct cryptodesc *, struct swcr_data *,
			    caddr_t, int);
extern int swcr_process(struct cryptop *);
extern int swcr_newsession(u_int32_t *, struct cryptoini *);
extern int swcr_freesession(u_int32_t);
extern void swcr_init(void);
#endif /* _KERNEL */

#endif /* _CRYPTO_CRYPTO_H_ */
