/*	$OpenBSD: crc.h,v 1.1 2004/05/02 17:53:29 millert Exp $	*/

/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define	CKSUM_DIGEST_LENGTH		4
#define	CKSUM_DIGEST_STRING_LENGTH	(10 + 1 + 19)

typedef struct CKSUMContext {
	u_int32_t crc;
	off_t len;
} CKSUM_CTX;

#define	SUM_DIGEST_LENGTH		4
#define	SUM_DIGEST_STRING_LENGTH	(10 + 1 + 16)

typedef struct SUMContext {
	u_int32_t crc;
	off_t len;
} SUM_CTX;

#define	SYSVSUM_DIGEST_LENGTH		4
#define	SYSVSUM_DIGEST_STRING_LENGTH	(10 + 1 + 16)

typedef struct SYSVSUMContext {
	u_int32_t crc;
	off_t len;
} SYSVSUM_CTX;

void	 CKSUM_Init(CKSUM_CTX *);
void	 CKSUM_Update(CKSUM_CTX *, const u_int8_t *, size_t);
void	 CKSUM_Final(CKSUM_CTX *);
char    *CKSUM_End(CKSUM_CTX *, char *);
char    *CKSUM_Data(const u_int8_t *, size_t, char *);

void	 SUM_Init(SUM_CTX *);
void	 SUM_Update(SUM_CTX *, const u_int8_t *, size_t);
void	 SUM_Final(SUM_CTX *);
char    *SUM_End(SUM_CTX *, char *);
char    *SUM_Data(const u_int8_t *, size_t, char *);

void	 SYSVSUM_Init(SYSVSUM_CTX *);
void	 SYSVSUM_Update(SYSVSUM_CTX *, const u_int8_t *, size_t);
void	 SYSVSUM_Final(SYSVSUM_CTX *);
char    *SYSVSUM_End(SYSVSUM_CTX *, char *);
char    *SYSVSUM_Data(const u_int8_t *, size_t, char *);
