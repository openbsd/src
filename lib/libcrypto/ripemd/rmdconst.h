/* $OpenBSD: rmdconst.h,v 1.5 2023/08/10 11:00:46 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

__BEGIN_HIDDEN_DECLS

#define KL0 0x00000000L
#define KL1 0x5A827999L
#define KL2 0x6ED9EBA1L
#define KL3 0x8F1BBCDCL
#define KL4 0xA953FD4EL

#define KR0 0x50A28BE6L
#define KR1 0x5C4DD124L
#define KR2 0x6D703EF3L
#define KR3 0x7A6D76E9L
#define KR4 0x00000000L

#define WL00  0
#define WL01  1
#define WL02  2
#define WL03  3
#define WL04  4
#define WL05  5
#define WL06  6
#define WL07  7
#define WL08  8
#define WL09  9
#define WL10 10
#define WL11 11
#define WL12 12
#define WL13 13
#define WL14 14
#define WL15 15

#define WL16  7
#define WL17  4
#define WL18 13
#define WL19  1
#define WL20 10
#define WL21  6
#define WL22 15
#define WL23  3
#define WL24 12
#define WL25  0
#define WL26  9
#define WL27  5
#define WL28  2
#define WL29 14
#define WL30 11
#define WL31  8

#define WL32  3
#define WL33 10
#define WL34 14
#define WL35  4
#define WL36  9
#define WL37 15
#define WL38  8
#define WL39  1
#define WL40  2
#define WL41  7
#define WL42  0
#define WL43  6
#define WL44 13
#define WL45 11
#define WL46  5
#define WL47 12

#define WL48  1
#define WL49  9
#define WL50 11
#define WL51 10
#define WL52  0
#define WL53  8
#define WL54 12
#define WL55  4
#define WL56 13
#define WL57  3
#define WL58  7
#define WL59 15
#define WL60 14
#define WL61  5
#define WL62  6
#define WL63  2

#define WL64  4
#define WL65  0
#define WL66  5
#define WL67  9
#define WL68  7
#define WL69 12
#define WL70  2
#define WL71 10
#define WL72 14
#define WL73  1
#define WL74  3
#define WL75  8
#define WL76 11
#define WL77  6
#define WL78 15
#define WL79 13

#define WR00  5
#define WR01 14
#define WR02  7
#define WR03  0
#define WR04  9
#define WR05  2
#define WR06 11
#define WR07  4
#define WR08 13
#define WR09  6
#define WR10 15
#define WR11  8
#define WR12  1
#define WR13 10
#define WR14  3
#define WR15 12

#define WR16  6
#define WR17 11
#define WR18  3
#define WR19  7
#define WR20  0
#define WR21 13
#define WR22  5
#define WR23 10
#define WR24 14
#define WR25 15
#define WR26  8
#define WR27 12
#define WR28  4
#define WR29  9
#define WR30  1
#define WR31  2

#define WR32 15
#define WR33  5
#define WR34  1
#define WR35  3
#define WR36  7
#define WR37 14
#define WR38  6
#define WR39  9
#define WR40 11
#define WR41  8
#define WR42 12
#define WR43  2
#define WR44 10
#define WR45  0
#define WR46  4
#define WR47 13

#define WR48  8
#define WR49  6
#define WR50  4
#define WR51  1
#define WR52  3
#define WR53 11
#define WR54 15
#define WR55  0
#define WR56  5
#define WR57 12
#define WR58  2
#define WR59 13
#define WR60  9
#define WR61  7
#define WR62 10
#define WR63 14

#define WR64 12
#define WR65 15
#define WR66 10
#define WR67  4
#define WR68  1
#define WR69  5
#define WR70  8
#define WR71  7
#define WR72  6
#define WR73  2
#define WR74 13
#define WR75 14
#define WR76  0
#define WR77  3
#define WR78  9
#define WR79 11

__END_HIDDEN_DECLS
