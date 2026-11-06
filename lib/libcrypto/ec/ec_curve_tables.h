/*	$OpenBSD: ec_curve_tables.h,v 1.1 2026/11/06 13:09:18 beck Exp $ */
/*
 * Copyright (c) 2026 Bob Beck <beck@openbsd.org>
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

/*
 * The built-in curves as static EC_GROUPs.
 *
 * Word arrays are least significant word first, without leading zero
 * words.  a, b and the generator are in Montgomery form for the field.
 */

#ifndef HEADER_EC_CURVE_TABLES_H
#define HEADER_EC_CURVE_TABLES_H

#include <stdint.h>

#include <openssl/bn.h>
#include <openssl/ec.h>
#include <openssl/objects.h>

#include "ec_local.h"

#define EC_CURVE_WORDS(words) (sizeof(words) / sizeof((words)[0]))

#define EC_CURVE_BN(words) {						\
	.d = (BN_ULONG *)(words),					\
	.top = EC_CURVE_WORDS(words),					\
	.dmax = EC_CURVE_WORDS(words),					\
	.flags = BN_FLG_STATIC_DATA,					\
}

#if BN_BITS2 == 64

#define EC_CURVE_N0(n0) { (n0), 0 }

static const BN_ULONG secp224k1_p[] = {
	0xfffffffeffffe56d, 0xffffffffffffffff, 0xffffffffffffffff,
	0x00000000ffffffff,
};
static const BN_ULONG secp224k1_b[] = {
	0x000084df00000000, 0x0000000000000005,
};
static const BN_ULONG secp224k1_x[] = {
	0x2dff5b37dad1f42d, 0xa80520b613d4a70c, 0x269021544065e22a,
	0x0000000061c52ec4,
};
static const BN_ULONG secp224k1_y[] = {
	0x29339c519dbb0f01, 0x7b8227819953f15c, 0xb0058b9528bbd02f,
	0x00000000c405bf50,
};
static const BN_ULONG secp224k1_z[] = {
	0x00001a9300000000, 0x0000000000000001,
};
static const BN_ULONG secp224k1_order[] = {
	0xcaf0a971769fb1f7, 0x0001dce8d2ec6184, 0x0000000000000000,
	0x0000000100000000,
};
static const BN_ULONG secp224k1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG secp224k1_rr[] = {
	0x0000000000000000, 0x0000352602c23069, 0x0000000000000001,
};

static const BN_ULONG secp224r1_p[] = {
	0x0000000000000001, 0xffffffff00000000, 0xffffffffffffffff,
	0x00000000ffffffff,
};
static const BN_ULONG secp224r1_a[] = {
	0x0000000300000001, 0xffffffff00000000, 0xfffffffffffffffc,
	0x00000000ffffffff,
};
static const BN_ULONG secp224r1_b[] = {
	0xe768cdf663c059cd, 0x107ac2f3ccf01310, 0x3dceba98c8528151,
	0x000000007fc02f93,
};
static const BN_ULONG secp224r1_x[] = {
	0xbc9052266d0a4aea, 0x852597366018bfaa, 0x6dd3af9bf96bec05,
	0x00000000a21b5e60,
};
static const BN_ULONG secp224r1_y[] = {
	0x2edca1e5eff3ede8, 0xf8cd672b05335a6b, 0xaea9c5ae03dfe878,
	0x00000000614786f1,
};
static const BN_ULONG secp224r1_z[] = {
	0xffffffff00000000, 0xffffffffffffffff,
};
static const BN_ULONG secp224r1_order[] = {
	0x13dd29455c5c2a3d, 0xffff16a2e0b8f03e, 0xffffffffffffffff,
	0x00000000ffffffff,
};
static const BN_ULONG secp224r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG secp224r1_rr[] = {
	0xffffffff00000001, 0xffffffff00000000, 0xfffffffe00000000,
	0x00000000ffffffff,
};

static const BN_ULONG secp256k1_p[] = {
	0xfffffffefffffc2f, 0xffffffffffffffff, 0xffffffffffffffff,
	0xffffffffffffffff,
};
static const BN_ULONG secp256k1_b[] = {
	0x0000000700001ab7,
};
static const BN_ULONG secp256k1_x[] = {
	0xd7362e5a487e2097, 0x231e295329bc66db, 0x979f48c033fd129c,
	0x9981e643e9089f48,
};
static const BN_ULONG secp256k1_y[] = {
	0xb15ea6d2d3dbabe2, 0x8dfc5d5d1f1dc64d, 0x70b6b59aac19c136,
	0xcf3f851fd4a582d6,
};
static const BN_ULONG secp256k1_z[] = {
	0x00000001000003d1,
};
static const BN_ULONG secp256k1_order[] = {
	0xbfd25e8cd0364141, 0xbaaedce6af48a03b, 0xfffffffffffffffe,
	0xffffffffffffffff,
};
static const BN_ULONG secp256k1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG secp256k1_rr[] = {
	0x000007a2000e90a1, 0x0000000000000001,
};

static const BN_ULONG prime256v1_p[] = {
	0xffffffffffffffff, 0x00000000ffffffff, 0x0000000000000000,
	0xffffffff00000001,
};
static const BN_ULONG prime256v1_a[] = {
	0xfffffffffffffffc, 0x00000003ffffffff, 0x0000000000000000,
	0xfffffffc00000004,
};
static const BN_ULONG prime256v1_b[] = {
	0xd89cdf6229c4bddf, 0xacf005cd78843090, 0xe5a220abf7212ed6,
	0xdc30061d04874834,
};
static const BN_ULONG prime256v1_x[] = {
	0x79e730d418a9143c, 0x75ba95fc5fedb601, 0x79fb732b77622510,
	0x18905f76a53755c6,
};
static const BN_ULONG prime256v1_y[] = {
	0xddf25357ce95560a, 0x8b4ab8e4ba19e45c, 0xd2e88688dd21f325,
	0x8571ff1825885d85,
};
static const BN_ULONG prime256v1_z[] = {
	0x0000000000000001, 0xffffffff00000000, 0xffffffffffffffff,
	0x00000000fffffffe,
};
static const BN_ULONG prime256v1_order[] = {
	0xf3b9cac2fc632551, 0xbce6faada7179e84, 0xffffffffffffffff,
	0xffffffff00000000,
};
static const BN_ULONG prime256v1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG prime256v1_rr[] = {
	0x0000000000000003, 0xfffffffbffffffff, 0xfffffffffffffffe,
	0x00000004fffffffd,
};

static const BN_ULONG secp384r1_p[] = {
	0x00000000ffffffff, 0xffffffff00000000, 0xfffffffffffffffe,
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
};
static const BN_ULONG secp384r1_a[] = {
	0x00000003fffffffc, 0xfffffffc00000000, 0xfffffffffffffffb,
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
};
static const BN_ULONG secp384r1_b[] = {
	0x081188719d412dcc, 0xf729add87a4c32ec, 0x77f2209b1920022e,
	0xe3374bee94938ae2, 0xb62b21f41f022094, 0xcd08114b604fbff9,
};
static const BN_ULONG secp384r1_x[] = {
	0x3dd0756649c0b528, 0x20e378e2a0d6ce38, 0x879c3afc541b4d6e,
	0x6454868459a30eff, 0x812ff723614ede2b, 0x4d3aadc2299e1513,
};
static const BN_ULONG secp384r1_y[] = {
	0x23043dad4b03a4fe, 0xa1bfa8bf7bb4a9ac, 0x8bade7562e83b050,
	0xc6c3521968f4ffd9, 0xdd8002263969a840, 0x2b78abc25a15c5e9,
};
static const BN_ULONG secp384r1_z[] = {
	0xffffffff00000001, 0x00000000ffffffff, 0x0000000000000001,
};
static const BN_ULONG secp384r1_order[] = {
	0xecec196accc52973, 0x581a0db248b0a77a, 0xc7634d81f4372ddf,
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
};
static const BN_ULONG secp384r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG secp384r1_rr[] = {
	0xfffffffe00000001, 0x0000000200000000, 0xfffffffe00000000,
	0x0000000200000000, 0x0000000000000001,
};

static const BN_ULONG secp521r1_p[] = {
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
	0xffffffffffffffff, 0xffffffffffffffff, 0x00000000000001ff,
};
static const BN_ULONG secp521r1_a[] = {
	0xfe7fffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
	0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff,
	0xffffffffffffffff, 0xffffffffffffffff, 0x00000000000001ff,
};
static const BN_ULONG secp521r1_b[] = {
	0x8014654fae586387, 0x78f7a28fea35a81f, 0x839ab9efc41e961a,
	0xbd8b29605e9dd8df, 0xf0ab0c9ca8f63f49, 0xf9dc5a44c8c77884,
	0x77516d392dccd98a, 0x0fc94d10d05b42a0, 0x000000000000004d,
};
static const BN_ULONG secp521r1_x[] = {
	0xb331a16381adc101, 0x4dfcbf3f18e172de, 0x6f19a459e0c2b521,
	0x947f0ee093d17fd4, 0xdd50a5af3bf7f3ac, 0x90fc1457b035a69e,
	0x214e32409c829fda, 0xe6cf1f65b311cada, 0x0000000000000074,
};
static const BN_ULONG secp521r1_y[] = {
	0x28460e4a5a9e268e, 0x20445f4a3b4fe8b3, 0xb09a9e3843513961,
	0x2062a85c809fd683, 0x164bf7394caf7a13, 0x340bd7de8b939f33,
	0xeccc7aa224abcda2, 0x022e452fda163e8d, 0x00000000000001e0,
};
static const BN_ULONG secp521r1_z[] = {
	0x0080000000000000,
};
static const BN_ULONG secp521r1_order[] = {
	0xbb6fb71e91386409, 0x3bb5c9b8899c47ae, 0x7fcc0148f709a5d0,
	0x51868783bf2f966b, 0xfffffffffffffffa, 0xffffffffffffffff,
	0xffffffffffffffff, 0xffffffffffffffff, 0x00000000000001ff,
};
static const BN_ULONG secp521r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG secp521r1_rr[] = {
	0x0000000000000000, 0x0000400000000000,
};

static const BN_ULONG brainpoolp224r1_p[] = {
	0x97da89f57ec8c0ff, 0x75d1d787b09f0757, 0x264366862a183025,
	0x00000000d7c134aa,
};
static const BN_ULONG brainpoolp224r1_a[] = {
	0x2445385352d02a7e, 0x138210f178836b75, 0x44cb7837c437d367,
	0x000000004acba11e,
};
static const BN_ULONG brainpoolp224r1_b[] = {
	0xa959a2ca9f9ac530, 0xf8d8ad2184f93130, 0x4a11d01123c3229d,
	0x00000000508fa146,
};
static const BN_ULONG brainpoolp224r1_x[] = {
	0x7993ea716ad29d1b, 0x4d402109cf9a7441, 0x5ddc6352bebd634f,
	0x000000000211cad8,
};
static const BN_ULONG brainpoolp224r1_y[] = {
	0xc4e76c808e956e51, 0x8f3e2bd5f943b361, 0xcac7fbecf98919fa,
	0x00000000285b6e63,
};
static const BN_ULONG brainpoolp224r1_z[] = {
	0xd682785e20b562f7, 0xa8e8428f5cbf0389, 0x1c3fb2447b576cf5,
	0x0000000079360611,
};
static const BN_ULONG brainpoolp224r1_order[] = {
	0x6ddebca3a5a7939f, 0x75d0fb98d116bc4b, 0x264366862a183025,
	0x00000000d7c134aa,
};
static const BN_ULONG brainpoolp224r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp224r1_rr[] = {
	0x3bfbc9bf6b3d58ff, 0x76f9ee46f691d105, 0x7ea5577c77e3d7e4,
	0x000000002b3d40dd,
};

static const BN_ULONG brainpoolp224t1_p[] = {
	0x97da89f57ec8c0ff, 0x75d1d787b09f0757, 0x264366862a183025,
	0x00000000d7c134aa,
};
static const BN_ULONG brainpoolp224t1_a[] = {
	0xac2daad09b715919, 0xf0eae7614b010411, 0xf7c7b63ee22a1969,
	0x0000000043e05720,
};
static const BN_ULONG brainpoolp224t1_b[] = {
	0x057d098fce65a487, 0x6b12ecc40d2f0f82, 0xc329b33f3d2e9dfe,
	0x000000006ed68110,
};
static const BN_ULONG brainpoolp224t1_x[] = {
	0x2d8667c9da4fd726, 0x3467cffc44736c58, 0x0d227d30024e2730,
	0x000000003b7fb6ce,
};
static const BN_ULONG brainpoolp224t1_y[] = {
	0x35d404e885b8dc86, 0x83ff6fcbd6880c25, 0x25011a7f351e184d,
	0x00000000cf9d2caf,
};
static const BN_ULONG brainpoolp224t1_z[] = {
	0xd682785e20b562f7, 0xa8e8428f5cbf0389, 0x1c3fb2447b576cf5,
	0x0000000079360611,
};
static const BN_ULONG brainpoolp224t1_order[] = {
	0x6ddebca3a5a7939f, 0x75d0fb98d116bc4b, 0x264366862a183025,
	0x00000000d7c134aa,
};
static const BN_ULONG brainpoolp224t1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp224t1_rr[] = {
	0x3bfbc9bf6b3d58ff, 0x76f9ee46f691d105, 0x7ea5577c77e3d7e4,
	0x000000002b3d40dd,
};

static const BN_ULONG brainpoolp256r1_p[] = {
	0x2013481d1f6e5377, 0x6e3bf623d5262028, 0x3e660a909d838d72,
	0xa9fb57dba1eea9bc,
};
static const BN_ULONG brainpoolp256r1_a[] = {
	0xd5d18edf69696261, 0xa68123f1c1d20c64, 0x95ec1e5e6398556e,
	0x1e4676abd666bc17,
};
static const BN_ULONG brainpoolp256r1_b[] = {
	0x05d24d72c0c0f36f, 0x0ac34a49cc51bf59, 0x64ca989357f2e9d9,
	0x1634f57646a3c93e,
};
static const BN_ULONG brainpoolp256r1_x[] = {
	0x27c0d92d351fd10c, 0x80de4d9ab97cf30a, 0x704c311d6b892ad3,
	0x8e1f767a9e119bdf,
};
static const BN_ULONG brainpoolp256r1_y[] = {
	0x9a4fe948a0917a17, 0xa618f259cd950162, 0x16fdf6e8dfbd8b03,
	0x14eb78c6026eb0a2,
};
static const BN_ULONG brainpoolp256r1_z[] = {
	0xdfecb7e2e091ac89, 0x91c409dc2ad9dfd7, 0xc199f56f627c728d,
	0x5604a8245e115643,
};
static const BN_ULONG brainpoolp256r1_order[] = {
	0x901e0e82974856a7, 0x8c397aa3b561a6f7, 0x3e660a909d838d71,
	0xa9fb57dba1eea9bc,
};
static const BN_ULONG brainpoolp256r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp256r1_rr[] = {
	0x8cfedf7ba6465b6c, 0x5cce4c26614d4f4d, 0xa1ecdacd6b1ac807,
	0x4717aa21e5957fa8,
};

static const BN_ULONG brainpoolp256t1_p[] = {
	0x2013481d1f6e5377, 0x6e3bf623d5262028, 0x3e660a909d838d72,
	0xa9fb57dba1eea9bc,
};
static const BN_ULONG brainpoolp256t1_a[] = {
	0xa06068919d27a153, 0x272bceb329bea0c8, 0x37fe34d31391c33c,
	0x51e8b74a29a950ad,
};
static const BN_ULONG brainpoolp256t1_b[] = {
	0xec09f8253e842369, 0x5378207cc1d93e3d, 0xd36e5b31172a7c7a,
	0x1969294c24b0c673,
};
static const BN_ULONG brainpoolp256t1_x[] = {
	0x39769890d918b12c, 0x5e24bdc5b0d7666d, 0xd5295b441bee1da0,
	0x2971dc4068633fae,
};
static const BN_ULONG brainpoolp256t1_y[] = {
	0x189d1ebbb96c41ca, 0x0fee4bdd7b35abec, 0x37e448676a7e0bc0,
	0x8bb1e31279f17e30,
};
static const BN_ULONG brainpoolp256t1_z[] = {
	0xdfecb7e2e091ac89, 0x91c409dc2ad9dfd7, 0xc199f56f627c728d,
	0x5604a8245e115643,
};
static const BN_ULONG brainpoolp256t1_order[] = {
	0x901e0e82974856a7, 0x8c397aa3b561a6f7, 0x3e660a909d838d71,
	0xa9fb57dba1eea9bc,
};
static const BN_ULONG brainpoolp256t1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp256t1_rr[] = {
	0x8cfedf7ba6465b6c, 0x5cce4c26614d4f4d, 0xa1ecdacd6b1ac807,
	0x4717aa21e5957fa8,
};

static const BN_ULONG brainpoolp320r1_p[] = {
	0xfcd412b1f1b32e27, 0x4f92b9ec7893ec28, 0xf98fcfa6f6f40def,
	0xe13c785ed201e065, 0xd35e472036bc4fb7,
};
static const BN_ULONG brainpoolp320r1_a[] = {
	0x20b3f27b0f7135a2, 0x03f46a6da3c2df86, 0xa139a3e1bb98b013,
	0x3a3598e8cdeb36df, 0x467bb8b9cec29d66,
};
static const BN_ULONG brainpoolp320r1_b[] = {
	0xc92e58f8bff1db1a, 0x14babbde20847b05, 0x1856e1da965d8ef9,
	0x917a9c72229d010d, 0x9336afca29885368,
};
static const BN_ULONG brainpoolp320r1_x[] = {
	0xbd925e705d7c0ab9, 0xee70fea4deccf0fb, 0x702afc301cfd939e,
	0x564fee03bf5221f3, 0x446ef8b341b3f4e4,
};
static const BN_ULONG brainpoolp320r1_y[] = {
	0x5401378747cc614c, 0xc67f8607801ee279, 0x54134356af3f0bf7,
	0xfb6a1a2181e5be79, 0x769a9382bd74ab8a,
};
static const BN_ULONG brainpoolp320r1_z[] = {
	0x032bed4e0e4cd1d9, 0xb06d4613876c13d7, 0x06703059090bf210,
	0x1ec387a12dfe1f9a, 0x2ca1b8dfc943b048,
};
static const BN_ULONG brainpoolp320r1_order[] = {
	0x8691555b44c59311, 0x2d482ec7ee8658e9, 0xf98fcfa5b68f12a3,
	0xe13c785ed201e065, 0xd35e472036bc4fb7,
};
static const BN_ULONG brainpoolp320r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp320r1_rr[] = {
	0x994ee88a743b52f9, 0xc2478a8d906978ef, 0x1f4c881f30c5b676,
	0x5455a964e614d6d2, 0xa259ba4a6c2d9252,
};

static const BN_ULONG brainpoolp320t1_p[] = {
	0xfcd412b1f1b32e27, 0x4f92b9ec7893ec28, 0xf98fcfa6f6f40def,
	0xe13c785ed201e065, 0xd35e472036bc4fb7,
};
static const BN_ULONG brainpoolp320t1_a[] = {
	0xf3504ac7c6ccb89c, 0x3e4ae7b1e24fb0a3, 0xe63f3e9bdbd037bd,
	0x84f1e17b48078197, 0x4d791c80daf13edf,
};
static const BN_ULONG brainpoolp320t1_b[] = {
	0x01a0fb35bb2459ac, 0x33b46b93e8f7f22e, 0x667b83703ccad628,
	0x60e43c5abd93ab4a, 0x5ca6fa362424a1ad,
};
static const BN_ULONG brainpoolp320t1_x[] = {
	0xa8065a4e256923e4, 0x9666799f7accce90, 0x4e865d270c0ebebf,
	0xd75c73f5cf4bd3c7, 0x2d0c8339c3bd9974,
};
static const BN_ULONG brainpoolp320t1_y[] = {
	0x54495f8485357a64, 0x615d69cb95b03811, 0x98092f43a260d9cd,
	0xad119ff881efafa7, 0x7932c774671c40de,
};
static const BN_ULONG brainpoolp320t1_z[] = {
	0x032bed4e0e4cd1d9, 0xb06d4613876c13d7, 0x06703059090bf210,
	0x1ec387a12dfe1f9a, 0x2ca1b8dfc943b048,
};
static const BN_ULONG brainpoolp320t1_order[] = {
	0x8691555b44c59311, 0x2d482ec7ee8658e9, 0xf98fcfa5b68f12a3,
	0xe13c785ed201e065, 0xd35e472036bc4fb7,
};
static const BN_ULONG brainpoolp320t1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp320t1_rr[] = {
	0x994ee88a743b52f9, 0xc2478a8d906978ef, 0x1f4c881f30c5b676,
	0x5455a964e614d6d2, 0xa259ba4a6c2d9252,
};

static const BN_ULONG brainpoolp384r1_p[] = {
	0x874700133107ec53, 0xacd3a729901d1a71, 0x12b1da197fb71123,
	0x152f7109ed5456b4, 0x0f5d6f7e50e641df, 0x8cb91e82a3386d28,
};
static const BN_ULONG brainpoolp384r1_a[] = {
	0xdb26b895466c3c99, 0x75d7f3fef157b07b, 0x936771b9d7f10db4,
	0xe7ffe9e535529374, 0x400a8fdf42b00c60, 0x7c338021a2e8c0d1,
};
static const BN_ULONG brainpoolp384r1_b[] = {
	0x1f05fdea00c8e16d, 0x362ef7c8205a0fe3, 0xcdb456c3f7216eda,
	0x17413827fe77fed8, 0x2b335681d1cd255d, 0x453dcefae84686aa,
};
static const BN_ULONG brainpoolp384r1_x[] = {
	0xa189deebd438fbc1, 0x66fc80e8d5a886bf, 0x94c378e99d202f23,
	0x068b264ef95c2164, 0x9cdd0dcfbacd0099, 0x8500753388f53fc1,
};
static const BN_ULONG brainpoolp384r1_y[] = {
	0xe738b3310de140a5, 0xf5e0d246c7996f55, 0xf88309a38f0737fc,
	0xa180acd4d5719217, 0xc61625664f21ddb6, 0x2cf4a062458968b5,
};
static const BN_ULONG brainpoolp384r1_z[] = {
	0x78b8ffeccef813ad, 0x532c58d66fe2e58e, 0xed4e25e68048eedc,
	0xead08ef612aba94b, 0xf0a29081af19be20, 0x7346e17d5cc792d7,
};
static const BN_ULONG brainpoolp384r1_order[] = {
	0x3b883202e9046565, 0xcf3ab6af6b7fc310, 0x1f166e6cac0425a7,
	0x152f7109ed5456b3, 0x0f5d6f7e50e641df, 0x8cb91e82a3386d28,
};
static const BN_ULONG brainpoolp384r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp384r1_rr[] = {
	0x087cefff40b64bde, 0x535283343d7fd965, 0x8e28f99cc9940899,
	0x621401919918d5af, 0xd5c6ef3ba57e052c, 0x36bf6883178df842,
};

static const BN_ULONG brainpoolp384t1_p[] = {
	0x874700133107ec53, 0xacd3a729901d1a71, 0x12b1da197fb71123,
	0x152f7109ed5456b4, 0x0f5d6f7e50e641df, 0x8cb91e82a3386d28,
};
static const BN_ULONG brainpoolp384t1_a[] = {
	0x2baa0073262f89f2, 0x0cf5eaf960ae9ea9, 0x702b1c98fe4a66d6,
	0x7f1ca63b8ffa0838, 0x5c309cf5e5658b3a, 0x4c56b70fd3528ef0,
};
static const BN_ULONG brainpoolp384t1_b[] = {
	0x1e2135dfd5a65daf, 0xc67f028805dc58c5, 0x8388fba9348aa1c9,
	0xe8274aaefcee4563, 0x5937313bb6fe4ab1, 0x36e0cfc6856368b8,
};
static const BN_ULONG brainpoolp384t1_x[] = {
	0xce88dfbc3de0b08b, 0x2bc03b2864ce6e29, 0x69ef5593998db1b9,
	0x8d4c0e46d6ca31c1, 0x9468c5c5d97aa97d, 0x261906d793862a17,
};
static const BN_ULONG brainpoolp384t1_y[] = {
	0xc0f65320570260ee, 0x392a96b76afb221a, 0xe7ddc7469c71b0a9,
	0xc9d614da79b715ad, 0xf1d2e291150def13, 0x10cd2001db014219,
};
static const BN_ULONG brainpoolp384t1_z[] = {
	0x78b8ffeccef813ad, 0x532c58d66fe2e58e, 0xed4e25e68048eedc,
	0xead08ef612aba94b, 0xf0a29081af19be20, 0x7346e17d5cc792d7,
};
static const BN_ULONG brainpoolp384t1_order[] = {
	0x3b883202e9046565, 0xcf3ab6af6b7fc310, 0x1f166e6cac0425a7,
	0x152f7109ed5456b3, 0x0f5d6f7e50e641df, 0x8cb91e82a3386d28,
};
static const BN_ULONG brainpoolp384t1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp384t1_rr[] = {
	0x087cefff40b64bde, 0x535283343d7fd965, 0x8e28f99cc9940899,
	0x621401919918d5af, 0xd5c6ef3ba57e052c, 0x36bf6883178df842,
};

static const BN_ULONG brainpoolp512r1_p[] = {
	0x28aa6056583a48f3, 0x2881ff2f2d82c685, 0xaecda12ae6a380e6,
	0x7d4d9b009bc66842, 0xd6639cca70330871, 0xcb308db3b3c9d20e,
	0x3fd4e6ae33c9fc07, 0xaadd9db8dbe9c48b,
};
static const BN_ULONG brainpoolp512r1_a[] = {
	0xda1f8a34ea10c446, 0x14e4957dafa7d283, 0x40b04b724675bbab,
	0xcf8f01119e6e87ff, 0xa5ec30c83f80d1c7, 0x182d0f59f41e8778,
	0xb83b84fae2d0850c, 0x5ec4f187227d2a83,
};
static const BN_ULONG brainpoolp512r1_b[] = {
	0x507e839620e92a34, 0x009b63c7e58e5a34, 0xe16ba4562d8724aa,
	0xc73e30e89877be02, 0x97e00c63fe222433, 0xcbda57ac6d17d81d,
	0x642312a50bb5aaa2, 0x6a4aabb4471e8ea7,
};
static const BN_ULONG brainpoolp512r1_x[] = {
	0xc4ce96095161d9d3, 0x683e4d64272c02a4, 0x34ab04146df55e8f,
	0x8550539514c01fc8, 0x2433d76f905c8737, 0xb2b6ea37f36d3cf7,
	0x871cb5ca006d4573, 0x5a2ba14c0994e981,
};
static const BN_ULONG brainpoolp512r1_y[] = {
	0x2f90662925042a6d, 0x7518df6f4742f325, 0xbf8455534c859490,
	0x360ec775598ecc3e, 0x7c170b888fe62fdc, 0x585d2b77cd9d3f8c,
	0x9a5ed7da870f3f9b, 0x8c50c9d12acb7281,
};
static const BN_ULONG brainpoolp512r1_z[] = {
	0xd7559fa9a7c5b70d, 0xd77e00d0d27d397a, 0x51325ed5195c7f19,
	0x82b264ff643997bd, 0x299c63358fccf78e, 0x34cf724c4c362df1,
	0xc02b1951cc3603f8, 0x5522624724163b74,
};
static const BN_ULONG brainpoolp512r1_order[] = {
	0xb58796829ca90069, 0x1db1d381085ddadd, 0x418661197fac1047,
	0x553e5c414ca92619, 0xd6639cca70330870, 0xcb308db3b3c9d20e,
	0x3fd4e6ae33c9fc07, 0xaadd9db8dbe9c48b,
};
static const BN_ULONG brainpoolp512r1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp512r1_rr[] = {
	0x49ad144a6158f205, 0x793fb13027157905, 0x53b7f9bc905affd3,
	0xe0c19a7783514a25, 0x19486fd8d5898057, 0xa16daa5fd42bff83,
	0x202e19402056eecc, 0x3c4c9d05a9ff6450,
};

static const BN_ULONG brainpoolp512t1_p[] = {
	0x28aa6056583a48f3, 0x2881ff2f2d82c685, 0xaecda12ae6a380e6,
	0x7d4d9b009bc66842, 0xd6639cca70330871, 0xcb308db3b3c9d20e,
	0x3fd4e6ae33c9fc07, 0xaadd9db8dbe9c48b,
};
static const BN_ULONG brainpoolp512t1_a[] = {
	0xcb53e1afb9236cbf, 0xca89fbebe38de099, 0x6a0425d68131847e,
	0x728407030ae0094d, 0x2ff20ff430ff2a37, 0xf7f2c48282f11a4a,
	0x3f28816702f1ec26, 0x5654149c4b90d6b8,
};
static const BN_ULONG brainpoolp512t1_b[] = {
	0xc7c0d84b03b6d155, 0x57827b891a251c87, 0xae3c6c59ae6c8ba5,
	0x8613cba9edf66553, 0xca8a2ab6a009cee9, 0xb42e6975ff16cc9f,
	0xb5b14d01a3a9d8a5, 0x355016b2e0549b5b,
};
static const BN_ULONG brainpoolp512t1_x[] = {
	0x8ed65743b232b5a4, 0x9fd730c8ac58271e, 0xf5adb86a61bb0d99,
	0x8126e25003786475, 0x758b15caa8c6172d, 0x13eae9f0e7829cf3,
	0x028a7e0e9f782221, 0x8a904453bd3f1d32,
};
static const BN_ULONG brainpoolp512t1_y[] = {
	0x4875ebddf44bb245, 0x930ea0fb27af00ed, 0x8f1a9fc4e730f651,
	0xe6fca4c6734b44dc, 0x145cbeafda79a289, 0xf9aa9b5a975fd7a8,
	0x58b1657517f8d745, 0x76a0a40a6dc5f1ef,
};
static const BN_ULONG brainpoolp512t1_z[] = {
	0xd7559fa9a7c5b70d, 0xd77e00d0d27d397a, 0x51325ed5195c7f19,
	0x82b264ff643997bd, 0x299c63358fccf78e, 0x34cf724c4c362df1,
	0xc02b1951cc3603f8, 0x5522624724163b74,
};
static const BN_ULONG brainpoolp512t1_order[] = {
	0xb58796829ca90069, 0x1db1d381085ddadd, 0x418661197fac1047,
	0x553e5c414ca92619, 0xd6639cca70330870, 0xcb308db3b3c9d20e,
	0x3fd4e6ae33c9fc07, 0xaadd9db8dbe9c48b,
};
static const BN_ULONG brainpoolp512t1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG brainpoolp512t1_rr[] = {
	0x49ad144a6158f205, 0x793fb13027157905, 0x53b7f9bc905affd3,
	0xe0c19a7783514a25, 0x19486fd8d5898057, 0xa16daa5fd42bff83,
	0x202e19402056eecc, 0x3c4c9d05a9ff6450,
};

static const BN_ULONG frp256v1_p[] = {
	0xe8fcf353d86e9c03, 0x3961adbcabc8ca6d, 0x10126de8ce42435b,
	0xf1fd178c0b3ad58f,
};
static const BN_ULONG frp256v1_a[] = {
	0xa3f3cd4f61ba700c, 0xe586b6f2af2329b7, 0x4049b7a339090d6c,
	0xc7f45e302ceb563c,
};
static const BN_ULONG frp256v1_b[] = {
	0x60283fc7c3fc9ffc, 0x827d0092818f7cf2, 0x282145677ffcbe09,
	0x51c3e4c2f6ec2915,
};
static const BN_ULONG frp256v1_x[] = {
	0x439f9765bc60dadf, 0xaa9c2e9515127d45, 0xca4d4c55492a4c99,
	0x9c0a06c3b87a3296,
};
static const BN_ULONG frp256v1_y[] = {
	0xf632a0a386ba31e8, 0x742efd111abdd91b, 0x8748d1df4d6d6e05,
	0x5862d17c999a5d68,
};
static const BN_ULONG frp256v1_z[] = {
	0x17030cac279163fd, 0xc69e524354373592, 0xefed921731bdbca4,
	0x0e02e873f4c52a70,
};
static const BN_ULONG frp256v1_order[] = {
	0x1ffdd459c6d655e1, 0x53dc67e140d2bf94, 0x10126de8ce42435b,
	0xf1fd178c0b3ad58f,
};
static const BN_ULONG frp256v1_cofactor[] = {
	0x0000000000000001,
};
static const BN_ULONG frp256v1_rr[] = {
	0xb0c24e77c99f1513, 0x846f80830c960f92, 0x62b7012fce137eee,
	0xb02c8f9f88eb98ac,
};

#else

#define EC_CURVE_N0(n0) { (BN_ULONG)(n0), (BN_ULONG)((n0) >> 32) }

static const BN_ULONG secp224k1_p[] = {
	0xffffe56d, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff,
};
static const BN_ULONG secp224k1_b[] = {
	0x000084df, 0x00000005,
};
static const BN_ULONG secp224k1_x[] = {
	0x39f813a4, 0x13d4a70b, 0xa80520b6, 0x4065e22a, 0x26902154, 0x61c52ec4,
	0xf4072e3f,
};
static const BN_ULONG secp224k1_y[] = {
	0x6a475fe5, 0x9953f15b, 0x7b822781, 0x28bbd02f, 0xb0058b95, 0xc405bf50,
	0xbeec289b,
};
static const BN_ULONG secp224k1_z[] = {
	0x00001a93, 0x00000001,
};
static const BN_ULONG secp224k1_order[] = {
	0x769fb1f7, 0xcaf0a971, 0xd2ec6184, 0x0001dce8, 0x00000000, 0x00000000,
	0x00000000, 0x00000001,
};
static const BN_ULONG secp224k1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG secp224k1_rr[] = {
	0x02c23069, 0x00003526, 0x00000001,
};

static const BN_ULONG secp224r1_p[] = {
	0x00000001, 0x00000000, 0x00000000, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff,
};
static const BN_ULONG secp224r1_a[] = {
	0x00000004, 0x00000000, 0x00000000, 0xfffffffc, 0xffffffff, 0xffffffff,
	0xffffffff,
};
static const BN_ULONG secp224r1_b[] = {
	0xe768cdf7, 0xccf01310, 0x743b1cc0, 0xc8528150, 0x3dceba98, 0x7fc02f93,
	0x9c3fa633,
};
static const BN_ULONG secp224r1_x[] = {
	0xbc905227, 0x6018bfaa, 0xf22fe220, 0xf96bec04, 0x6dd3af9b, 0xa21b5e60,
	0x92f5b516,
};
static const BN_ULONG secp224r1_y[] = {
	0x2edca1e6, 0x05335a6b, 0xe8c15513, 0x03dfe878, 0xaea9c5ae, 0x614786f1,
	0x100c1218,
};
static const BN_ULONG secp224r1_z[] = {
	0xffffffff, 0xffffffff, 0xffffffff,
};
static const BN_ULONG secp224r1_order[] = {
	0x5c5c2a3d, 0x13dd2945, 0xe0b8f03e, 0xffff16a2, 0xffffffff, 0xffffffff,
	0xffffffff,
};
static const BN_ULONG secp224r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG secp224r1_rr[] = {
	0x00000001, 0x00000000, 0x00000000, 0xfffffffe, 0xffffffff, 0xffffffff,
};

static const BN_ULONG secp256k1_p[] = {
	0xfffffc2f, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff,
};
static const BN_ULONG secp256k1_b[] = {
	0x00001ab7, 0x00000007,
};
static const BN_ULONG secp256k1_x[] = {
	0x487e2097, 0xd7362e5a, 0x29bc66db, 0x231e2953, 0x33fd129c, 0x979f48c0,
	0xe9089f48, 0x9981e643,
};
static const BN_ULONG secp256k1_y[] = {
	0xd3dbabe2, 0xb15ea6d2, 0x1f1dc64d, 0x8dfc5d5d, 0xac19c136, 0x70b6b59a,
	0xd4a582d6, 0xcf3f851f,
};
static const BN_ULONG secp256k1_z[] = {
	0x000003d1, 0x00000001,
};
static const BN_ULONG secp256k1_order[] = {
	0xd0364141, 0xbfd25e8c, 0xaf48a03b, 0xbaaedce6, 0xfffffffe, 0xffffffff,
	0xffffffff, 0xffffffff,
};
static const BN_ULONG secp256k1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG secp256k1_rr[] = {
	0x000e90a1, 0x000007a2, 0x00000001,
};

static const BN_ULONG prime256v1_p[] = {
	0xffffffff, 0xffffffff, 0xffffffff, 0x00000000, 0x00000000, 0x00000000,
	0x00000001, 0xffffffff,
};
static const BN_ULONG prime256v1_a[] = {
	0xfffffffc, 0xffffffff, 0xffffffff, 0x00000003, 0x00000000, 0x00000000,
	0x00000004, 0xfffffffc,
};
static const BN_ULONG prime256v1_b[] = {
	0x29c4bddf, 0xd89cdf62, 0x78843090, 0xacf005cd, 0xf7212ed6, 0xe5a220ab,
	0x04874834, 0xdc30061d,
};
static const BN_ULONG prime256v1_x[] = {
	0x18a9143c, 0x79e730d4, 0x5fedb601, 0x75ba95fc, 0x77622510, 0x79fb732b,
	0xa53755c6, 0x18905f76,
};
static const BN_ULONG prime256v1_y[] = {
	0xce95560a, 0xddf25357, 0xba19e45c, 0x8b4ab8e4, 0xdd21f325, 0xd2e88688,
	0x25885d85, 0x8571ff18,
};
static const BN_ULONG prime256v1_z[] = {
	0x00000001, 0x00000000, 0x00000000, 0xffffffff, 0xffffffff, 0xffffffff,
	0xfffffffe,
};
static const BN_ULONG prime256v1_order[] = {
	0xfc632551, 0xf3b9cac2, 0xa7179e84, 0xbce6faad, 0xffffffff, 0xffffffff,
	0x00000000, 0xffffffff,
};
static const BN_ULONG prime256v1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG prime256v1_rr[] = {
	0x00000003, 0x00000000, 0xffffffff, 0xfffffffb, 0xfffffffe, 0xffffffff,
	0xfffffffd, 0x00000004,
};

static const BN_ULONG secp384r1_p[] = {
	0xffffffff, 0x00000000, 0x00000000, 0xffffffff, 0xfffffffe, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
};
static const BN_ULONG secp384r1_a[] = {
	0xfffffffc, 0x00000003, 0x00000000, 0xfffffffc, 0xfffffffb, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
};
static const BN_ULONG secp384r1_b[] = {
	0x9d412dcc, 0x08118871, 0x7a4c32ec, 0xf729add8, 0x1920022e, 0x77f2209b,
	0x94938ae2, 0xe3374bee, 0x1f022094, 0xb62b21f4, 0x604fbff9, 0xcd08114b,
};
static const BN_ULONG secp384r1_x[] = {
	0x49c0b528, 0x3dd07566, 0xa0d6ce38, 0x20e378e2, 0x541b4d6e, 0x879c3afc,
	0x59a30eff, 0x64548684, 0x614ede2b, 0x812ff723, 0x299e1513, 0x4d3aadc2,
};
static const BN_ULONG secp384r1_y[] = {
	0x4b03a4fe, 0x23043dad, 0x7bb4a9ac, 0xa1bfa8bf, 0x2e83b050, 0x8bade756,
	0x68f4ffd9, 0xc6c35219, 0x3969a840, 0xdd800226, 0x5a15c5e9, 0x2b78abc2,
};
static const BN_ULONG secp384r1_z[] = {
	0x00000001, 0xffffffff, 0xffffffff, 0x00000000, 0x00000001,
};
static const BN_ULONG secp384r1_order[] = {
	0xccc52973, 0xecec196a, 0x48b0a77a, 0x581a0db2, 0xf4372ddf, 0xc7634d81,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
};
static const BN_ULONG secp384r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG secp384r1_rr[] = {
	0x00000001, 0xfffffffe, 0x00000000, 0x00000002, 0x00000000, 0xfffffffe,
	0x00000000, 0x00000002, 0x00000001,
};

static const BN_ULONG secp521r1_p[] = {
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000001ff,
};
static const BN_ULONG secp521r1_a[] = {
	0xfe7fffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000001ff,
};
static const BN_ULONG secp521r1_b[] = {
	0x8014654f, 0xea35a81f, 0x78f7a28f, 0xc41e961a, 0x839ab9ef, 0x5e9dd8df,
	0xbd8b2960, 0xa8f63f49, 0xf0ab0c9c, 0xc8c77884, 0xf9dc5a44, 0x2dccd98a,
	0x77516d39, 0xd05b42a0, 0x0fc94d10, 0xb0c70e4d, 0x0000015c,
};
static const BN_ULONG secp521r1_x[] = {
	0xb331a163, 0x18e172de, 0x4dfcbf3f, 0xe0c2b521, 0x6f19a459, 0x93d17fd4,
	0x947f0ee0, 0x3bf7f3ac, 0xdd50a5af, 0xb035a69e, 0x90fc1457, 0x9c829fda,
	0x214e3240, 0xb311cada, 0xe6cf1f65, 0x5b820274, 0x00000103,
};
static const BN_ULONG secp521r1_y[] = {
	0x28460e4a, 0x3b4fe8b3, 0x20445f4a, 0x43513961, 0xb09a9e38, 0x809fd683,
	0x2062a85c, 0x4caf7a13, 0x164bf739, 0x8b939f33, 0x340bd7de, 0x24abcda2,
	0xeccc7aa2, 0xda163e8d, 0x022e452f, 0x3c4d1de0, 0x000000b5,
};
static const BN_ULONG secp521r1_z[] = {
	0x00800000,
};
static const BN_ULONG secp521r1_order[] = {
	0x91386409, 0xbb6fb71e, 0x899c47ae, 0x3bb5c9b8, 0xf709a5d0, 0x7fcc0148,
	0xbf2f966b, 0x51868783, 0xfffffffa, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x000001ff,
};
static const BN_ULONG secp521r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG secp521r1_rr[] = {
	0x00000000, 0x00004000,
};

static const BN_ULONG brainpoolp224r1_p[] = {
	0x7ec8c0ff, 0x97da89f5, 0xb09f0757, 0x75d1d787, 0x2a183025, 0x26436686,
	0xd7c134aa,
};
static const BN_ULONG brainpoolp224r1_a[] = {
	0x0a148fa3, 0x38d15f58, 0x4ba447ce, 0x14928da0, 0x0d367346, 0x8cddc740,
	0x19f07d7c,
};
static const BN_ULONG brainpoolp224r1_b[] = {
	0xaf83166f, 0xc21b9d8a, 0x0034e6d5, 0xce9dac7f, 0xb76ec712, 0xf5331b76,
	0x9be078e3,
};
static const BN_ULONG brainpoolp224r1_x[] = {
	0x234bfb4b, 0x31fdc9d7, 0xb5e4939a, 0x19886fa3, 0xb1cc327a, 0x29c4eb1d,
	0x9d928506,
};
static const BN_ULONG brainpoolp224r1_y[] = {
	0x258adb1a, 0x3ff9013c, 0x3e5d0f80, 0x4c141f60, 0x89de7323, 0x22b9dc72,
	0xba215755,
};
static const BN_ULONG brainpoolp224r1_z[] = {
	0x81373f01, 0x6825760a, 0x4f60f8a8, 0x8a2e2878, 0xd5e7cfda, 0xd9bc9979,
	0x283ecb55,
};
static const BN_ULONG brainpoolp224r1_order[] = {
	0xa5a7939f, 0x6ddebca3, 0xd116bc4b, 0x75d0fb98, 0x2a183025, 0x26436686,
	0xd7c134aa,
};
static const BN_ULONG brainpoolp224r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp224r1_rr[] = {
	0x64dcd04f, 0x7867ca80, 0x43c20e72, 0x96af774c, 0x3fe8a2aa, 0x2e6a6ce4,
	0x0578fd59,
};

static const BN_ULONG brainpoolp224t1_p[] = {
	0x7ec8c0ff, 0x97da89f5, 0xb09f0757, 0x75d1d787, 0x2a183025, 0x26436686,
	0xd7c134aa,
};
static const BN_ULONG brainpoolp224t1_a[] = {
	0xfb2303fc, 0x5f6a27d5, 0xc27c1d5e, 0xd7475e1e, 0xa860c095, 0x990d9a18,
	0x5f04d2a8,
};
static const BN_ULONG brainpoolp224t1_b[] = {
	0xb3a144a7, 0x7c8cb857, 0xa1f77ffd, 0x4ee33314, 0x6d49fa14, 0xecebf772,
	0x00c8f03e,
};
static const BN_ULONG brainpoolp224t1_x[] = {
	0x0f3605f8, 0x25825ede, 0xaf05917a, 0x82daf323, 0xac39083e, 0xd4635c79,
	0xab61a067,
};
static const BN_ULONG brainpoolp224t1_y[] = {
	0x356b05bd, 0xee619c9f, 0x369c6ff4, 0xeb97c151, 0xde722aa7, 0x0766d5f7,
	0x76260430,
};
static const BN_ULONG brainpoolp224t1_z[] = {
	0x81373f01, 0x6825760a, 0x4f60f8a8, 0x8a2e2878, 0xd5e7cfda, 0xd9bc9979,
	0x283ecb55,
};
static const BN_ULONG brainpoolp224t1_order[] = {
	0xa5a7939f, 0x6ddebca3, 0xd116bc4b, 0x75d0fb98, 0x2a183025, 0x26436686,
	0xd7c134aa,
};
static const BN_ULONG brainpoolp224t1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp224t1_rr[] = {
	0x64dcd04f, 0x7867ca80, 0x43c20e72, 0x96af774c, 0x3fe8a2aa, 0x2e6a6ce4,
	0x0578fd59,
};

static const BN_ULONG brainpoolp256r1_p[] = {
	0x1f6e5377, 0x2013481d, 0xd5262028, 0x6e3bf623, 0x9d838d72, 0x3e660a90,
	0xa1eea9bc, 0xa9fb57db,
};
static const BN_ULONG brainpoolp256r1_a[] = {
	0x69696261, 0xd5d18edf, 0xc1d20c64, 0xa68123f1, 0x6398556e, 0x95ec1e5e,
	0xd666bc17, 0x1e4676ab,
};
static const BN_ULONG brainpoolp256r1_b[] = {
	0xc0c0f36f, 0x05d24d72, 0xcc51bf59, 0x0ac34a49, 0x57f2e9d9, 0x64ca9893,
	0x46a3c93e, 0x1634f576,
};
static const BN_ULONG brainpoolp256r1_x[] = {
	0x351fd10c, 0x27c0d92d, 0xb97cf30a, 0x80de4d9a, 0x6b892ad3, 0x704c311d,
	0x9e119bdf, 0x8e1f767a,
};
static const BN_ULONG brainpoolp256r1_y[] = {
	0xa0917a17, 0x9a4fe948, 0xcd950162, 0xa618f259, 0xdfbd8b03, 0x16fdf6e8,
	0x026eb0a2, 0x14eb78c6,
};
static const BN_ULONG brainpoolp256r1_z[] = {
	0xe091ac89, 0xdfecb7e2, 0x2ad9dfd7, 0x91c409dc, 0x627c728d, 0xc199f56f,
	0x5e115643, 0x5604a824,
};
static const BN_ULONG brainpoolp256r1_order[] = {
	0x974856a7, 0x901e0e82, 0xb561a6f7, 0x8c397aa3, 0x9d838d71, 0x3e660a90,
	0xa1eea9bc, 0xa9fb57db,
};
static const BN_ULONG brainpoolp256r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp256r1_rr[] = {
	0xa6465b6c, 0x8cfedf7b, 0x614d4f4d, 0x5cce4c26, 0x6b1ac807, 0xa1ecdacd,
	0xe5957fa8, 0x4717aa21,
};

static const BN_ULONG brainpoolp256t1_p[] = {
	0x1f6e5377, 0x2013481d, 0xd5262028, 0x6e3bf623, 0x9d838d72, 0x3e660a90,
	0xa1eea9bc, 0xa9fb57db,
};
static const BN_ULONG brainpoolp256t1_a[] = {
	0x9d27a153, 0xa0606891, 0x29bea0c8, 0x272bceb3, 0x1391c33c, 0x37fe34d3,
	0x29a950ad, 0x51e8b74a,
};
static const BN_ULONG brainpoolp256t1_b[] = {
	0x3e842369, 0xec09f825, 0xc1d93e3d, 0x5378207c, 0x172a7c7a, 0xd36e5b31,
	0x24b0c673, 0x1969294c,
};
static const BN_ULONG brainpoolp256t1_x[] = {
	0xd918b12c, 0x39769890, 0xb0d7666d, 0x5e24bdc5, 0x1bee1da0, 0xd5295b44,
	0x68633fae, 0x2971dc40,
};
static const BN_ULONG brainpoolp256t1_y[] = {
	0xb96c41ca, 0x189d1ebb, 0x7b35abec, 0x0fee4bdd, 0x6a7e0bc0, 0x37e44867,
	0x79f17e30, 0x8bb1e312,
};
static const BN_ULONG brainpoolp256t1_z[] = {
	0xe091ac89, 0xdfecb7e2, 0x2ad9dfd7, 0x91c409dc, 0x627c728d, 0xc199f56f,
	0x5e115643, 0x5604a824,
};
static const BN_ULONG brainpoolp256t1_order[] = {
	0x974856a7, 0x901e0e82, 0xb561a6f7, 0x8c397aa3, 0x9d838d71, 0x3e660a90,
	0xa1eea9bc, 0xa9fb57db,
};
static const BN_ULONG brainpoolp256t1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp256t1_rr[] = {
	0xa6465b6c, 0x8cfedf7b, 0x614d4f4d, 0x5cce4c26, 0x6b1ac807, 0xa1ecdacd,
	0xe5957fa8, 0x4717aa21,
};

static const BN_ULONG brainpoolp320r1_p[] = {
	0xf1b32e27, 0xfcd412b1, 0x7893ec28, 0x4f92b9ec, 0xf6f40def, 0xf98fcfa6,
	0xd201e065, 0xe13c785e, 0x36bc4fb7, 0xd35e4720,
};
static const BN_ULONG brainpoolp320r1_a[] = {
	0x0f7135a2, 0x20b3f27b, 0xa3c2df86, 0x03f46a6d, 0xbb98b013, 0xa139a3e1,
	0xcdeb36df, 0x3a3598e8, 0xcec29d66, 0x467bb8b9,
};
static const BN_ULONG brainpoolp320r1_b[] = {
	0xbff1db1a, 0xc92e58f8, 0x20847b05, 0x14babbde, 0x965d8ef9, 0x1856e1da,
	0x229d010d, 0x917a9c72, 0x29885368, 0x9336afca,
};
static const BN_ULONG brainpoolp320r1_x[] = {
	0x5d7c0ab9, 0xbd925e70, 0xdeccf0fb, 0xee70fea4, 0x1cfd939e, 0x702afc30,
	0xbf5221f3, 0x564fee03, 0x41b3f4e4, 0x446ef8b3,
};
static const BN_ULONG brainpoolp320r1_y[] = {
	0x47cc614c, 0x54013787, 0x801ee279, 0xc67f8607, 0xaf3f0bf7, 0x54134356,
	0x81e5be79, 0xfb6a1a21, 0xbd74ab8a, 0x769a9382,
};
static const BN_ULONG brainpoolp320r1_z[] = {
	0x0e4cd1d9, 0x032bed4e, 0x876c13d7, 0xb06d4613, 0x090bf210, 0x06703059,
	0x2dfe1f9a, 0x1ec387a1, 0xc943b048, 0x2ca1b8df,
};
static const BN_ULONG brainpoolp320r1_order[] = {
	0x44c59311, 0x8691555b, 0xee8658e9, 0x2d482ec7, 0xb68f12a3, 0xf98fcfa5,
	0xd201e065, 0xe13c785e, 0x36bc4fb7, 0xd35e4720,
};
static const BN_ULONG brainpoolp320r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp320r1_rr[] = {
	0x743b52f9, 0x994ee88a, 0x906978ef, 0xc2478a8d, 0x30c5b676, 0x1f4c881f,
	0xe614d6d2, 0x5455a964, 0x6c2d9252, 0xa259ba4a,
};

static const BN_ULONG brainpoolp320t1_p[] = {
	0xf1b32e27, 0xfcd412b1, 0x7893ec28, 0x4f92b9ec, 0xf6f40def, 0xf98fcfa6,
	0xd201e065, 0xe13c785e, 0x36bc4fb7, 0xd35e4720,
};
static const BN_ULONG brainpoolp320t1_a[] = {
	0xc6ccb89c, 0xf3504ac7, 0xe24fb0a3, 0x3e4ae7b1, 0xdbd037bd, 0xe63f3e9b,
	0x48078197, 0x84f1e17b, 0xdaf13edf, 0x4d791c80,
};
static const BN_ULONG brainpoolp320t1_b[] = {
	0xbb2459ac, 0x01a0fb35, 0xe8f7f22e, 0x33b46b93, 0x3ccad628, 0x667b8370,
	0xbd93ab4a, 0x60e43c5a, 0x2424a1ad, 0x5ca6fa36,
};
static const BN_ULONG brainpoolp320t1_x[] = {
	0x256923e4, 0xa8065a4e, 0x7accce90, 0x9666799f, 0x0c0ebebf, 0x4e865d27,
	0xcf4bd3c7, 0xd75c73f5, 0xc3bd9974, 0x2d0c8339,
};
static const BN_ULONG brainpoolp320t1_y[] = {
	0x85357a64, 0x54495f84, 0x95b03811, 0x615d69cb, 0xa260d9cd, 0x98092f43,
	0x81efafa7, 0xad119ff8, 0x671c40de, 0x7932c774,
};
static const BN_ULONG brainpoolp320t1_z[] = {
	0x0e4cd1d9, 0x032bed4e, 0x876c13d7, 0xb06d4613, 0x090bf210, 0x06703059,
	0x2dfe1f9a, 0x1ec387a1, 0xc943b048, 0x2ca1b8df,
};
static const BN_ULONG brainpoolp320t1_order[] = {
	0x44c59311, 0x8691555b, 0xee8658e9, 0x2d482ec7, 0xb68f12a3, 0xf98fcfa5,
	0xd201e065, 0xe13c785e, 0x36bc4fb7, 0xd35e4720,
};
static const BN_ULONG brainpoolp320t1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp320t1_rr[] = {
	0x743b52f9, 0x994ee88a, 0x906978ef, 0xc2478a8d, 0x30c5b676, 0x1f4c881f,
	0xe614d6d2, 0x5455a964, 0x6c2d9252, 0xa259ba4a,
};

static const BN_ULONG brainpoolp384r1_p[] = {
	0x3107ec53, 0x87470013, 0x901d1a71, 0xacd3a729, 0x7fb71123, 0x12b1da19,
	0xed5456b4, 0x152f7109, 0x50e641df, 0x0f5d6f7e, 0xa3386d28, 0x8cb91e82,
};
static const BN_ULONG brainpoolp384r1_a[] = {
	0x466c3c99, 0xdb26b895, 0xf157b07b, 0x75d7f3fe, 0xd7f10db4, 0x936771b9,
	0x35529374, 0xe7ffe9e5, 0x42b00c60, 0x400a8fdf, 0xa2e8c0d1, 0x7c338021,
};
static const BN_ULONG brainpoolp384r1_b[] = {
	0x00c8e16d, 0x1f05fdea, 0x205a0fe3, 0x362ef7c8, 0xf7216eda, 0xcdb456c3,
	0xfe77fed8, 0x17413827, 0xd1cd255d, 0x2b335681, 0xe84686aa, 0x453dcefa,
};
static const BN_ULONG brainpoolp384r1_x[] = {
	0xd438fbc1, 0xa189deeb, 0xd5a886bf, 0x66fc80e8, 0x9d202f23, 0x94c378e9,
	0xf95c2164, 0x068b264e, 0xbacd0099, 0x9cdd0dcf, 0x88f53fc1, 0x85007533,
};
static const BN_ULONG brainpoolp384r1_y[] = {
	0x0de140a5, 0xe738b331, 0xc7996f55, 0xf5e0d246, 0x8f0737fc, 0xf88309a3,
	0xd5719217, 0xa180acd4, 0x4f21ddb6, 0xc6162566, 0x458968b5, 0x2cf4a062,
};
static const BN_ULONG brainpoolp384r1_z[] = {
	0xcef813ad, 0x78b8ffec, 0x6fe2e58e, 0x532c58d6, 0x8048eedc, 0xed4e25e6,
	0x12aba94b, 0xead08ef6, 0xaf19be20, 0xf0a29081, 0x5cc792d7, 0x7346e17d,
};
static const BN_ULONG brainpoolp384r1_order[] = {
	0xe9046565, 0x3b883202, 0x6b7fc310, 0xcf3ab6af, 0xac0425a7, 0x1f166e6c,
	0xed5456b3, 0x152f7109, 0x50e641df, 0x0f5d6f7e, 0xa3386d28, 0x8cb91e82,
};
static const BN_ULONG brainpoolp384r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp384r1_rr[] = {
	0x40b64bde, 0x087cefff, 0x3d7fd965, 0x53528334, 0xc9940899, 0x8e28f99c,
	0x9918d5af, 0x62140191, 0xa57e052c, 0xd5c6ef3b, 0x178df842, 0x36bf6883,
};

static const BN_ULONG brainpoolp384t1_p[] = {
	0x3107ec53, 0x87470013, 0x901d1a71, 0xacd3a729, 0x7fb71123, 0x12b1da19,
	0xed5456b4, 0x152f7109, 0x50e641df, 0x0f5d6f7e, 0xa3386d28, 0x8cb91e82,
};
static const BN_ULONG brainpoolp384t1_a[] = {
	0x262f89f2, 0x2baa0073, 0x60ae9ea9, 0x0cf5eaf9, 0xfe4a66d6, 0x702b1c98,
	0x8ffa0838, 0x7f1ca63b, 0xe5658b3a, 0x5c309cf5, 0xd3528ef0, 0x4c56b70f,
};
static const BN_ULONG brainpoolp384t1_b[] = {
	0xd5a65daf, 0x1e2135df, 0x05dc58c5, 0xc67f0288, 0x348aa1c9, 0x8388fba9,
	0xfcee4563, 0xe8274aae, 0xb6fe4ab1, 0x5937313b, 0x856368b8, 0x36e0cfc6,
};
static const BN_ULONG brainpoolp384t1_x[] = {
	0x3de0b08b, 0xce88dfbc, 0x64ce6e29, 0x2bc03b28, 0x998db1b9, 0x69ef5593,
	0xd6ca31c1, 0x8d4c0e46, 0xd97aa97d, 0x9468c5c5, 0x93862a17, 0x261906d7,
};
static const BN_ULONG brainpoolp384t1_y[] = {
	0x570260ee, 0xc0f65320, 0x6afb221a, 0x392a96b7, 0x9c71b0a9, 0xe7ddc746,
	0x79b715ad, 0xc9d614da, 0x150def13, 0xf1d2e291, 0xdb014219, 0x10cd2001,
};
static const BN_ULONG brainpoolp384t1_z[] = {
	0xcef813ad, 0x78b8ffec, 0x6fe2e58e, 0x532c58d6, 0x8048eedc, 0xed4e25e6,
	0x12aba94b, 0xead08ef6, 0xaf19be20, 0xf0a29081, 0x5cc792d7, 0x7346e17d,
};
static const BN_ULONG brainpoolp384t1_order[] = {
	0xe9046565, 0x3b883202, 0x6b7fc310, 0xcf3ab6af, 0xac0425a7, 0x1f166e6c,
	0xed5456b3, 0x152f7109, 0x50e641df, 0x0f5d6f7e, 0xa3386d28, 0x8cb91e82,
};
static const BN_ULONG brainpoolp384t1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp384t1_rr[] = {
	0x40b64bde, 0x087cefff, 0x3d7fd965, 0x53528334, 0xc9940899, 0x8e28f99c,
	0x9918d5af, 0x62140191, 0xa57e052c, 0xd5c6ef3b, 0x178df842, 0x36bf6883,
};

static const BN_ULONG brainpoolp512r1_p[] = {
	0x583a48f3, 0x28aa6056, 0x2d82c685, 0x2881ff2f, 0xe6a380e6, 0xaecda12a,
	0x9bc66842, 0x7d4d9b00, 0x70330871, 0xd6639cca, 0xb3c9d20e, 0xcb308db3,
	0x33c9fc07, 0x3fd4e6ae, 0xdbe9c48b, 0xaadd9db8,
};
static const BN_ULONG brainpoolp512r1_a[] = {
	0xea10c446, 0xda1f8a34, 0xafa7d283, 0x14e4957d, 0x4675bbab, 0x40b04b72,
	0x9e6e87ff, 0xcf8f0111, 0x3f80d1c7, 0xa5ec30c8, 0xf41e8778, 0x182d0f59,
	0xe2d0850c, 0xb83b84fa, 0x227d2a83, 0x5ec4f187,
};
static const BN_ULONG brainpoolp512r1_b[] = {
	0x20e92a34, 0x507e8396, 0xe58e5a34, 0x009b63c7, 0x2d8724aa, 0xe16ba456,
	0x9877be02, 0xc73e30e8, 0xfe222433, 0x97e00c63, 0x6d17d81d, 0xcbda57ac,
	0x0bb5aaa2, 0x642312a5, 0x471e8ea7, 0x6a4aabb4,
};
static const BN_ULONG brainpoolp512r1_x[] = {
	0x5161d9d3, 0xc4ce9609, 0x272c02a4, 0x683e4d64, 0x6df55e8f, 0x34ab0414,
	0x14c01fc8, 0x85505395, 0x905c8737, 0x2433d76f, 0xf36d3cf7, 0xb2b6ea37,
	0x006d4573, 0x871cb5ca, 0x0994e981, 0x5a2ba14c,
};
static const BN_ULONG brainpoolp512r1_y[] = {
	0x25042a6d, 0x2f906629, 0x4742f325, 0x7518df6f, 0x4c859490, 0xbf845553,
	0x598ecc3e, 0x360ec775, 0x8fe62fdc, 0x7c170b88, 0xcd9d3f8c, 0x585d2b77,
	0x870f3f9b, 0x9a5ed7da, 0x2acb7281, 0x8c50c9d1,
};
static const BN_ULONG brainpoolp512r1_z[] = {
	0xa7c5b70d, 0xd7559fa9, 0xd27d397a, 0xd77e00d0, 0x195c7f19, 0x51325ed5,
	0x643997bd, 0x82b264ff, 0x8fccf78e, 0x299c6335, 0x4c362df1, 0x34cf724c,
	0xcc3603f8, 0xc02b1951, 0x24163b74, 0x55226247,
};
static const BN_ULONG brainpoolp512r1_order[] = {
	0x9ca90069, 0xb5879682, 0x085ddadd, 0x1db1d381, 0x7fac1047, 0x41866119,
	0x4ca92619, 0x553e5c41, 0x70330870, 0xd6639cca, 0xb3c9d20e, 0xcb308db3,
	0x33c9fc07, 0x3fd4e6ae, 0xdbe9c48b, 0xaadd9db8,
};
static const BN_ULONG brainpoolp512r1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp512r1_rr[] = {
	0x6158f205, 0x49ad144a, 0x27157905, 0x793fb130, 0x905affd3, 0x53b7f9bc,
	0x83514a25, 0xe0c19a77, 0xd5898057, 0x19486fd8, 0xd42bff83, 0xa16daa5f,
	0x2056eecc, 0x202e1940, 0xa9ff6450, 0x3c4c9d05,
};

static const BN_ULONG brainpoolp512t1_p[] = {
	0x583a48f3, 0x28aa6056, 0x2d82c685, 0x2881ff2f, 0xe6a380e6, 0xaecda12a,
	0x9bc66842, 0x7d4d9b00, 0x70330871, 0xd6639cca, 0xb3c9d20e, 0xcb308db3,
	0x33c9fc07, 0x3fd4e6ae, 0xdbe9c48b, 0xaadd9db8,
};
static const BN_ULONG brainpoolp512t1_a[] = {
	0xb9236cbf, 0xcb53e1af, 0xe38de099, 0xca89fbeb, 0x8131847e, 0x6a0425d6,
	0x0ae0094d, 0x72840703, 0x30ff2a37, 0x2ff20ff4, 0x82f11a4a, 0xf7f2c482,
	0x02f1ec26, 0x3f288167, 0x4b90d6b8, 0x5654149c,
};
static const BN_ULONG brainpoolp512t1_b[] = {
	0x03b6d155, 0xc7c0d84b, 0x1a251c87, 0x57827b89, 0xae6c8ba5, 0xae3c6c59,
	0xedf66553, 0x8613cba9, 0xa009cee9, 0xca8a2ab6, 0xff16cc9f, 0xb42e6975,
	0xa3a9d8a5, 0xb5b14d01, 0xe0549b5b, 0x355016b2,
};
static const BN_ULONG brainpoolp512t1_x[] = {
	0xb232b5a4, 0x8ed65743, 0xac58271e, 0x9fd730c8, 0x61bb0d99, 0xf5adb86a,
	0x03786475, 0x8126e250, 0xa8c6172d, 0x758b15ca, 0xe7829cf3, 0x13eae9f0,
	0x9f782221, 0x028a7e0e, 0xbd3f1d32, 0x8a904453,
};
static const BN_ULONG brainpoolp512t1_y[] = {
	0xf44bb245, 0x4875ebdd, 0x27af00ed, 0x930ea0fb, 0xe730f651, 0x8f1a9fc4,
	0x734b44dc, 0xe6fca4c6, 0xda79a289, 0x145cbeaf, 0x975fd7a8, 0xf9aa9b5a,
	0x17f8d745, 0x58b16575, 0x6dc5f1ef, 0x76a0a40a,
};
static const BN_ULONG brainpoolp512t1_z[] = {
	0xa7c5b70d, 0xd7559fa9, 0xd27d397a, 0xd77e00d0, 0x195c7f19, 0x51325ed5,
	0x643997bd, 0x82b264ff, 0x8fccf78e, 0x299c6335, 0x4c362df1, 0x34cf724c,
	0xcc3603f8, 0xc02b1951, 0x24163b74, 0x55226247,
};
static const BN_ULONG brainpoolp512t1_order[] = {
	0x9ca90069, 0xb5879682, 0x085ddadd, 0x1db1d381, 0x7fac1047, 0x41866119,
	0x4ca92619, 0x553e5c41, 0x70330870, 0xd6639cca, 0xb3c9d20e, 0xcb308db3,
	0x33c9fc07, 0x3fd4e6ae, 0xdbe9c48b, 0xaadd9db8,
};
static const BN_ULONG brainpoolp512t1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG brainpoolp512t1_rr[] = {
	0x6158f205, 0x49ad144a, 0x27157905, 0x793fb130, 0x905affd3, 0x53b7f9bc,
	0x83514a25, 0xe0c19a77, 0xd5898057, 0x19486fd8, 0xd42bff83, 0xa16daa5f,
	0x2056eecc, 0x202e1940, 0xa9ff6450, 0x3c4c9d05,
};

static const BN_ULONG frp256v1_p[] = {
	0xd86e9c03, 0xe8fcf353, 0xabc8ca6d, 0x3961adbc, 0xce42435b, 0x10126de8,
	0x0b3ad58f, 0xf1fd178c,
};
static const BN_ULONG frp256v1_a[] = {
	0x61ba700c, 0xa3f3cd4f, 0xaf2329b7, 0xe586b6f2, 0x39090d6c, 0x4049b7a3,
	0x2ceb563c, 0xc7f45e30,
};
static const BN_ULONG frp256v1_b[] = {
	0xc3fc9ffc, 0x60283fc7, 0x818f7cf2, 0x827d0092, 0x7ffcbe09, 0x28214567,
	0xf6ec2915, 0x51c3e4c2,
};
static const BN_ULONG frp256v1_x[] = {
	0xbc60dadf, 0x439f9765, 0x15127d45, 0xaa9c2e95, 0x492a4c99, 0xca4d4c55,
	0xb87a3296, 0x9c0a06c3,
};
static const BN_ULONG frp256v1_y[] = {
	0x86ba31e8, 0xf632a0a3, 0x1abdd91b, 0x742efd11, 0x4d6d6e05, 0x8748d1df,
	0x999a5d68, 0x5862d17c,
};
static const BN_ULONG frp256v1_z[] = {
	0x279163fd, 0x17030cac, 0x54373592, 0xc69e5243, 0x31bdbca4, 0xefed9217,
	0xf4c52a70, 0x0e02e873,
};
static const BN_ULONG frp256v1_order[] = {
	0xc6d655e1, 0x1ffdd459, 0x40d2bf94, 0x53dc67e1, 0xce42435b, 0x10126de8,
	0x0b3ad58f, 0xf1fd178c,
};
static const BN_ULONG frp256v1_cofactor[] = {
	0x00000001,
};
static const BN_ULONG frp256v1_rr[] = {
	0xc99f1513, 0xb0c24e77, 0x0c960f92, 0x846f8083, 0xce137eee, 0x62b7012f,
	0x88eb98ac, 0xb02c8f9f,
};

#endif

/* SECG curve secp224k1 */

static const BIGNUM secp224k1_p_bn =
    EC_CURVE_BN(secp224k1_p);
static const BIGNUM secp224k1_a_bn = {
	.flags = BN_FLG_STATIC_DATA,
};
static const BIGNUM secp224k1_b_bn =
    EC_CURVE_BN(secp224k1_b);
static const BIGNUM secp224k1_x_bn =
    EC_CURVE_BN(secp224k1_x);
static const BIGNUM secp224k1_y_bn =
    EC_CURVE_BN(secp224k1_y);
static const BIGNUM secp224k1_z_bn =
    EC_CURVE_BN(secp224k1_z);
static const BIGNUM secp224k1_order_bn =
    EC_CURVE_BN(secp224k1_order);
static const BIGNUM secp224k1_cofactor_bn =
    EC_CURVE_BN(secp224k1_cofactor);

static const BN_MONT_CTX secp224k1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(secp224k1_p),
	.RR = EC_CURVE_BN(secp224k1_rr),
	.N = EC_CURVE_BN(secp224k1_p),
	.n0 = EC_CURVE_N0(0x5a92a00a198d139bULL),
};

static const EC_POINT secp224k1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_secp224k1,
	.X = (BIGNUM *)&secp224k1_x_bn,
	.Y = (BIGNUM *)&secp224k1_y_bn,
	.Z = (BIGNUM *)&secp224k1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP secp224k1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&secp224k1_generator,
	.order = (BIGNUM *)&secp224k1_order_bn,
	.cofactor = (BIGNUM *)&secp224k1_cofactor_bn,
	.nid = NID_secp224k1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&secp224k1_p_bn,
	.a = (BIGNUM *)&secp224k1_a_bn,
	.b = (BIGNUM *)&secp224k1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&secp224k1_mont,
	.static_params = 1,
};

/* NIST/SECG curve secp224r1 */

static const uint8_t secp224r1_seed[] = {
	0xbd, 0x71, 0x34, 0x47, 0x99, 0xd5, 0xc7, 0xfc,
	0xdc, 0x45, 0xb5, 0x9f, 0xa3, 0xb9, 0xab, 0x8f,
	0x6a, 0x94, 0x8b, 0xc5,
};

static const BIGNUM secp224r1_p_bn =
    EC_CURVE_BN(secp224r1_p);
static const BIGNUM secp224r1_a_bn =
    EC_CURVE_BN(secp224r1_a);
static const BIGNUM secp224r1_b_bn =
    EC_CURVE_BN(secp224r1_b);
static const BIGNUM secp224r1_x_bn =
    EC_CURVE_BN(secp224r1_x);
static const BIGNUM secp224r1_y_bn =
    EC_CURVE_BN(secp224r1_y);
static const BIGNUM secp224r1_z_bn =
    EC_CURVE_BN(secp224r1_z);
static const BIGNUM secp224r1_order_bn =
    EC_CURVE_BN(secp224r1_order);
static const BIGNUM secp224r1_cofactor_bn =
    EC_CURVE_BN(secp224r1_cofactor);

static const BN_MONT_CTX secp224r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(secp224r1_p),
	.RR = EC_CURVE_BN(secp224r1_rr),
	.N = EC_CURVE_BN(secp224r1_p),
	.n0 = EC_CURVE_N0(0xffffffffffffffffULL),
};

static const EC_POINT secp224r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_secp224r1,
	.X = (BIGNUM *)&secp224r1_x_bn,
	.Y = (BIGNUM *)&secp224r1_y_bn,
	.Z = (BIGNUM *)&secp224r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP secp224r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&secp224r1_generator,
	.order = (BIGNUM *)&secp224r1_order_bn,
	.cofactor = (BIGNUM *)&secp224r1_cofactor_bn,
	.nid = NID_secp224r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.seed = (unsigned char *)secp224r1_seed,
	.seed_len = sizeof(secp224r1_seed),
	.static_seed = 1,
	.p = (BIGNUM *)&secp224r1_p_bn,
	.a = (BIGNUM *)&secp224r1_a_bn,
	.b = (BIGNUM *)&secp224r1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&secp224r1_mont,
	.static_params = 1,
};

/* SECG curve secp256k1 */

static const BIGNUM secp256k1_p_bn =
    EC_CURVE_BN(secp256k1_p);
static const BIGNUM secp256k1_a_bn = {
	.flags = BN_FLG_STATIC_DATA,
};
static const BIGNUM secp256k1_b_bn =
    EC_CURVE_BN(secp256k1_b);
static const BIGNUM secp256k1_x_bn =
    EC_CURVE_BN(secp256k1_x);
static const BIGNUM secp256k1_y_bn =
    EC_CURVE_BN(secp256k1_y);
static const BIGNUM secp256k1_z_bn =
    EC_CURVE_BN(secp256k1_z);
static const BIGNUM secp256k1_order_bn =
    EC_CURVE_BN(secp256k1_order);
static const BIGNUM secp256k1_cofactor_bn =
    EC_CURVE_BN(secp256k1_cofactor);

static const BN_MONT_CTX secp256k1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(secp256k1_p),
	.RR = EC_CURVE_BN(secp256k1_rr),
	.N = EC_CURVE_BN(secp256k1_p),
	.n0 = EC_CURVE_N0(0xd838091dd2253531ULL),
};

static const EC_POINT secp256k1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_secp256k1,
	.X = (BIGNUM *)&secp256k1_x_bn,
	.Y = (BIGNUM *)&secp256k1_y_bn,
	.Z = (BIGNUM *)&secp256k1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP secp256k1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&secp256k1_generator,
	.order = (BIGNUM *)&secp256k1_order_bn,
	.cofactor = (BIGNUM *)&secp256k1_cofactor_bn,
	.nid = NID_secp256k1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&secp256k1_p_bn,
	.a = (BIGNUM *)&secp256k1_a_bn,
	.b = (BIGNUM *)&secp256k1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&secp256k1_mont,
	.static_params = 1,
};

/* X9.62/SECG curve prime256v1 */

static const uint8_t prime256v1_seed[] = {
	0xc4, 0x9d, 0x36, 0x08, 0x86, 0xe7, 0x04, 0x93,
	0x6a, 0x66, 0x78, 0xe1, 0x13, 0x9d, 0x26, 0xb7,
	0x81, 0x9f, 0x7e, 0x90,
};

static const BIGNUM prime256v1_p_bn =
    EC_CURVE_BN(prime256v1_p);
static const BIGNUM prime256v1_a_bn =
    EC_CURVE_BN(prime256v1_a);
static const BIGNUM prime256v1_b_bn =
    EC_CURVE_BN(prime256v1_b);
static const BIGNUM prime256v1_x_bn =
    EC_CURVE_BN(prime256v1_x);
static const BIGNUM prime256v1_y_bn =
    EC_CURVE_BN(prime256v1_y);
static const BIGNUM prime256v1_z_bn =
    EC_CURVE_BN(prime256v1_z);
static const BIGNUM prime256v1_order_bn =
    EC_CURVE_BN(prime256v1_order);
static const BIGNUM prime256v1_cofactor_bn =
    EC_CURVE_BN(prime256v1_cofactor);

static const BN_MONT_CTX prime256v1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(prime256v1_p),
	.RR = EC_CURVE_BN(prime256v1_rr),
	.N = EC_CURVE_BN(prime256v1_p),
	.n0 = EC_CURVE_N0(0x0000000000000001ULL),
};

static const EC_POINT prime256v1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_X9_62_prime256v1,
	.X = (BIGNUM *)&prime256v1_x_bn,
	.Y = (BIGNUM *)&prime256v1_y_bn,
	.Z = (BIGNUM *)&prime256v1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP prime256v1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&prime256v1_generator,
	.order = (BIGNUM *)&prime256v1_order_bn,
	.cofactor = (BIGNUM *)&prime256v1_cofactor_bn,
	.nid = NID_X9_62_prime256v1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.seed = (unsigned char *)prime256v1_seed,
	.seed_len = sizeof(prime256v1_seed),
	.static_seed = 1,
	.p = (BIGNUM *)&prime256v1_p_bn,
	.a = (BIGNUM *)&prime256v1_a_bn,
	.b = (BIGNUM *)&prime256v1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&prime256v1_mont,
	.static_params = 1,
};

/* NIST/SECG curve secp384r1 */

static const uint8_t secp384r1_seed[] = {
	0xa3, 0x35, 0x92, 0x6a, 0xa3, 0x19, 0xa2, 0x7a,
	0x1d, 0x00, 0x89, 0x6a, 0x67, 0x73, 0xa4, 0x82,
	0x7a, 0xcd, 0xac, 0x73,
};

static const BIGNUM secp384r1_p_bn =
    EC_CURVE_BN(secp384r1_p);
static const BIGNUM secp384r1_a_bn =
    EC_CURVE_BN(secp384r1_a);
static const BIGNUM secp384r1_b_bn =
    EC_CURVE_BN(secp384r1_b);
static const BIGNUM secp384r1_x_bn =
    EC_CURVE_BN(secp384r1_x);
static const BIGNUM secp384r1_y_bn =
    EC_CURVE_BN(secp384r1_y);
static const BIGNUM secp384r1_z_bn =
    EC_CURVE_BN(secp384r1_z);
static const BIGNUM secp384r1_order_bn =
    EC_CURVE_BN(secp384r1_order);
static const BIGNUM secp384r1_cofactor_bn =
    EC_CURVE_BN(secp384r1_cofactor);

static const BN_MONT_CTX secp384r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(secp384r1_p),
	.RR = EC_CURVE_BN(secp384r1_rr),
	.N = EC_CURVE_BN(secp384r1_p),
	.n0 = EC_CURVE_N0(0x0000000100000001ULL),
};

static const EC_POINT secp384r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_secp384r1,
	.X = (BIGNUM *)&secp384r1_x_bn,
	.Y = (BIGNUM *)&secp384r1_y_bn,
	.Z = (BIGNUM *)&secp384r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP secp384r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&secp384r1_generator,
	.order = (BIGNUM *)&secp384r1_order_bn,
	.cofactor = (BIGNUM *)&secp384r1_cofactor_bn,
	.nid = NID_secp384r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.seed = (unsigned char *)secp384r1_seed,
	.seed_len = sizeof(secp384r1_seed),
	.static_seed = 1,
	.p = (BIGNUM *)&secp384r1_p_bn,
	.a = (BIGNUM *)&secp384r1_a_bn,
	.b = (BIGNUM *)&secp384r1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&secp384r1_mont,
	.static_params = 1,
};

/* NIST/SECG curve secp521r1 */

static const uint8_t secp521r1_seed[] = {
	0xd0, 0x9e, 0x88, 0x00, 0x29, 0x1c, 0xb8, 0x53,
	0x96, 0xcc, 0x67, 0x17, 0x39, 0x32, 0x84, 0xaa,
	0xa0, 0xda, 0x64, 0xba,
};

static const BIGNUM secp521r1_p_bn =
    EC_CURVE_BN(secp521r1_p);
static const BIGNUM secp521r1_a_bn =
    EC_CURVE_BN(secp521r1_a);
static const BIGNUM secp521r1_b_bn =
    EC_CURVE_BN(secp521r1_b);
static const BIGNUM secp521r1_x_bn =
    EC_CURVE_BN(secp521r1_x);
static const BIGNUM secp521r1_y_bn =
    EC_CURVE_BN(secp521r1_y);
static const BIGNUM secp521r1_z_bn =
    EC_CURVE_BN(secp521r1_z);
static const BIGNUM secp521r1_order_bn =
    EC_CURVE_BN(secp521r1_order);
static const BIGNUM secp521r1_cofactor_bn =
    EC_CURVE_BN(secp521r1_cofactor);

static const BN_MONT_CTX secp521r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(secp521r1_p),
	.RR = EC_CURVE_BN(secp521r1_rr),
	.N = EC_CURVE_BN(secp521r1_p),
	.n0 = EC_CURVE_N0(0x0000000000000001ULL),
};

static const EC_POINT secp521r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_secp521r1,
	.X = (BIGNUM *)&secp521r1_x_bn,
	.Y = (BIGNUM *)&secp521r1_y_bn,
	.Z = (BIGNUM *)&secp521r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP secp521r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&secp521r1_generator,
	.order = (BIGNUM *)&secp521r1_order_bn,
	.cofactor = (BIGNUM *)&secp521r1_cofactor_bn,
	.nid = NID_secp521r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.seed = (unsigned char *)secp521r1_seed,
	.seed_len = sizeof(secp521r1_seed),
	.static_seed = 1,
	.p = (BIGNUM *)&secp521r1_p_bn,
	.a = (BIGNUM *)&secp521r1_a_bn,
	.b = (BIGNUM *)&secp521r1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&secp521r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP224r1 */

static const BIGNUM brainpoolp224r1_p_bn =
    EC_CURVE_BN(brainpoolp224r1_p);
static const BIGNUM brainpoolp224r1_a_bn =
    EC_CURVE_BN(brainpoolp224r1_a);
static const BIGNUM brainpoolp224r1_b_bn =
    EC_CURVE_BN(brainpoolp224r1_b);
static const BIGNUM brainpoolp224r1_x_bn =
    EC_CURVE_BN(brainpoolp224r1_x);
static const BIGNUM brainpoolp224r1_y_bn =
    EC_CURVE_BN(brainpoolp224r1_y);
static const BIGNUM brainpoolp224r1_z_bn =
    EC_CURVE_BN(brainpoolp224r1_z);
static const BIGNUM brainpoolp224r1_order_bn =
    EC_CURVE_BN(brainpoolp224r1_order);
static const BIGNUM brainpoolp224r1_cofactor_bn =
    EC_CURVE_BN(brainpoolp224r1_cofactor);

static const BN_MONT_CTX brainpoolp224r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp224r1_p),
	.RR = EC_CURVE_BN(brainpoolp224r1_rr),
	.N = EC_CURVE_BN(brainpoolp224r1_p),
	.n0 = EC_CURVE_N0(0xe24d216ae149c101ULL),
};

static const EC_POINT brainpoolp224r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP224r1,
	.X = (BIGNUM *)&brainpoolp224r1_x_bn,
	.Y = (BIGNUM *)&brainpoolp224r1_y_bn,
	.Z = (BIGNUM *)&brainpoolp224r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp224r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp224r1_generator,
	.order = (BIGNUM *)&brainpoolp224r1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp224r1_cofactor_bn,
	.nid = NID_brainpoolP224r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp224r1_p_bn,
	.a = (BIGNUM *)&brainpoolp224r1_a_bn,
	.b = (BIGNUM *)&brainpoolp224r1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp224r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP224t1 */

static const BIGNUM brainpoolp224t1_p_bn =
    EC_CURVE_BN(brainpoolp224t1_p);
static const BIGNUM brainpoolp224t1_a_bn =
    EC_CURVE_BN(brainpoolp224t1_a);
static const BIGNUM brainpoolp224t1_b_bn =
    EC_CURVE_BN(brainpoolp224t1_b);
static const BIGNUM brainpoolp224t1_x_bn =
    EC_CURVE_BN(brainpoolp224t1_x);
static const BIGNUM brainpoolp224t1_y_bn =
    EC_CURVE_BN(brainpoolp224t1_y);
static const BIGNUM brainpoolp224t1_z_bn =
    EC_CURVE_BN(brainpoolp224t1_z);
static const BIGNUM brainpoolp224t1_order_bn =
    EC_CURVE_BN(brainpoolp224t1_order);
static const BIGNUM brainpoolp224t1_cofactor_bn =
    EC_CURVE_BN(brainpoolp224t1_cofactor);

static const BN_MONT_CTX brainpoolp224t1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp224t1_p),
	.RR = EC_CURVE_BN(brainpoolp224t1_rr),
	.N = EC_CURVE_BN(brainpoolp224t1_p),
	.n0 = EC_CURVE_N0(0xe24d216ae149c101ULL),
};

static const EC_POINT brainpoolp224t1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP224t1,
	.X = (BIGNUM *)&brainpoolp224t1_x_bn,
	.Y = (BIGNUM *)&brainpoolp224t1_y_bn,
	.Z = (BIGNUM *)&brainpoolp224t1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp224t1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp224t1_generator,
	.order = (BIGNUM *)&brainpoolp224t1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp224t1_cofactor_bn,
	.nid = NID_brainpoolP224t1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp224t1_p_bn,
	.a = (BIGNUM *)&brainpoolp224t1_a_bn,
	.b = (BIGNUM *)&brainpoolp224t1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp224t1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP256r1 */

static const BIGNUM brainpoolp256r1_p_bn =
    EC_CURVE_BN(brainpoolp256r1_p);
static const BIGNUM brainpoolp256r1_a_bn =
    EC_CURVE_BN(brainpoolp256r1_a);
static const BIGNUM brainpoolp256r1_b_bn =
    EC_CURVE_BN(brainpoolp256r1_b);
static const BIGNUM brainpoolp256r1_x_bn =
    EC_CURVE_BN(brainpoolp256r1_x);
static const BIGNUM brainpoolp256r1_y_bn =
    EC_CURVE_BN(brainpoolp256r1_y);
static const BIGNUM brainpoolp256r1_z_bn =
    EC_CURVE_BN(brainpoolp256r1_z);
static const BIGNUM brainpoolp256r1_order_bn =
    EC_CURVE_BN(brainpoolp256r1_order);
static const BIGNUM brainpoolp256r1_cofactor_bn =
    EC_CURVE_BN(brainpoolp256r1_cofactor);

static const BN_MONT_CTX brainpoolp256r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp256r1_p),
	.RR = EC_CURVE_BN(brainpoolp256r1_rr),
	.N = EC_CURVE_BN(brainpoolp256r1_p),
	.n0 = EC_CURVE_N0(0xc6a75590cefd89b9ULL),
};

static const EC_POINT brainpoolp256r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP256r1,
	.X = (BIGNUM *)&brainpoolp256r1_x_bn,
	.Y = (BIGNUM *)&brainpoolp256r1_y_bn,
	.Z = (BIGNUM *)&brainpoolp256r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp256r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp256r1_generator,
	.order = (BIGNUM *)&brainpoolp256r1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp256r1_cofactor_bn,
	.nid = NID_brainpoolP256r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp256r1_p_bn,
	.a = (BIGNUM *)&brainpoolp256r1_a_bn,
	.b = (BIGNUM *)&brainpoolp256r1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp256r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP256t1 */

static const BIGNUM brainpoolp256t1_p_bn =
    EC_CURVE_BN(brainpoolp256t1_p);
static const BIGNUM brainpoolp256t1_a_bn =
    EC_CURVE_BN(brainpoolp256t1_a);
static const BIGNUM brainpoolp256t1_b_bn =
    EC_CURVE_BN(brainpoolp256t1_b);
static const BIGNUM brainpoolp256t1_x_bn =
    EC_CURVE_BN(brainpoolp256t1_x);
static const BIGNUM brainpoolp256t1_y_bn =
    EC_CURVE_BN(brainpoolp256t1_y);
static const BIGNUM brainpoolp256t1_z_bn =
    EC_CURVE_BN(brainpoolp256t1_z);
static const BIGNUM brainpoolp256t1_order_bn =
    EC_CURVE_BN(brainpoolp256t1_order);
static const BIGNUM brainpoolp256t1_cofactor_bn =
    EC_CURVE_BN(brainpoolp256t1_cofactor);

static const BN_MONT_CTX brainpoolp256t1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp256t1_p),
	.RR = EC_CURVE_BN(brainpoolp256t1_rr),
	.N = EC_CURVE_BN(brainpoolp256t1_p),
	.n0 = EC_CURVE_N0(0xc6a75590cefd89b9ULL),
};

static const EC_POINT brainpoolp256t1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP256t1,
	.X = (BIGNUM *)&brainpoolp256t1_x_bn,
	.Y = (BIGNUM *)&brainpoolp256t1_y_bn,
	.Z = (BIGNUM *)&brainpoolp256t1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp256t1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp256t1_generator,
	.order = (BIGNUM *)&brainpoolp256t1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp256t1_cofactor_bn,
	.nid = NID_brainpoolP256t1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp256t1_p_bn,
	.a = (BIGNUM *)&brainpoolp256t1_a_bn,
	.b = (BIGNUM *)&brainpoolp256t1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp256t1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP320r1 */

static const BIGNUM brainpoolp320r1_p_bn =
    EC_CURVE_BN(brainpoolp320r1_p);
static const BIGNUM brainpoolp320r1_a_bn =
    EC_CURVE_BN(brainpoolp320r1_a);
static const BIGNUM brainpoolp320r1_b_bn =
    EC_CURVE_BN(brainpoolp320r1_b);
static const BIGNUM brainpoolp320r1_x_bn =
    EC_CURVE_BN(brainpoolp320r1_x);
static const BIGNUM brainpoolp320r1_y_bn =
    EC_CURVE_BN(brainpoolp320r1_y);
static const BIGNUM brainpoolp320r1_z_bn =
    EC_CURVE_BN(brainpoolp320r1_z);
static const BIGNUM brainpoolp320r1_order_bn =
    EC_CURVE_BN(brainpoolp320r1_order);
static const BIGNUM brainpoolp320r1_cofactor_bn =
    EC_CURVE_BN(brainpoolp320r1_cofactor);

static const BN_MONT_CTX brainpoolp320r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp320r1_p),
	.RR = EC_CURVE_BN(brainpoolp320r1_rr),
	.N = EC_CURVE_BN(brainpoolp320r1_p),
	.n0 = EC_CURVE_N0(0x3d1e9ba22a8a9e69ULL),
};

static const EC_POINT brainpoolp320r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP320r1,
	.X = (BIGNUM *)&brainpoolp320r1_x_bn,
	.Y = (BIGNUM *)&brainpoolp320r1_y_bn,
	.Z = (BIGNUM *)&brainpoolp320r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp320r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp320r1_generator,
	.order = (BIGNUM *)&brainpoolp320r1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp320r1_cofactor_bn,
	.nid = NID_brainpoolP320r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp320r1_p_bn,
	.a = (BIGNUM *)&brainpoolp320r1_a_bn,
	.b = (BIGNUM *)&brainpoolp320r1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp320r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP320t1 */

static const BIGNUM brainpoolp320t1_p_bn =
    EC_CURVE_BN(brainpoolp320t1_p);
static const BIGNUM brainpoolp320t1_a_bn =
    EC_CURVE_BN(brainpoolp320t1_a);
static const BIGNUM brainpoolp320t1_b_bn =
    EC_CURVE_BN(brainpoolp320t1_b);
static const BIGNUM brainpoolp320t1_x_bn =
    EC_CURVE_BN(brainpoolp320t1_x);
static const BIGNUM brainpoolp320t1_y_bn =
    EC_CURVE_BN(brainpoolp320t1_y);
static const BIGNUM brainpoolp320t1_z_bn =
    EC_CURVE_BN(brainpoolp320t1_z);
static const BIGNUM brainpoolp320t1_order_bn =
    EC_CURVE_BN(brainpoolp320t1_order);
static const BIGNUM brainpoolp320t1_cofactor_bn =
    EC_CURVE_BN(brainpoolp320t1_cofactor);

static const BN_MONT_CTX brainpoolp320t1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp320t1_p),
	.RR = EC_CURVE_BN(brainpoolp320t1_rr),
	.N = EC_CURVE_BN(brainpoolp320t1_p),
	.n0 = EC_CURVE_N0(0x3d1e9ba22a8a9e69ULL),
};

static const EC_POINT brainpoolp320t1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP320t1,
	.X = (BIGNUM *)&brainpoolp320t1_x_bn,
	.Y = (BIGNUM *)&brainpoolp320t1_y_bn,
	.Z = (BIGNUM *)&brainpoolp320t1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp320t1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp320t1_generator,
	.order = (BIGNUM *)&brainpoolp320t1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp320t1_cofactor_bn,
	.nid = NID_brainpoolP320t1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp320t1_p_bn,
	.a = (BIGNUM *)&brainpoolp320t1_a_bn,
	.b = (BIGNUM *)&brainpoolp320t1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp320t1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP384r1 */

static const BIGNUM brainpoolp384r1_p_bn =
    EC_CURVE_BN(brainpoolp384r1_p);
static const BIGNUM brainpoolp384r1_a_bn =
    EC_CURVE_BN(brainpoolp384r1_a);
static const BIGNUM brainpoolp384r1_b_bn =
    EC_CURVE_BN(brainpoolp384r1_b);
static const BIGNUM brainpoolp384r1_x_bn =
    EC_CURVE_BN(brainpoolp384r1_x);
static const BIGNUM brainpoolp384r1_y_bn =
    EC_CURVE_BN(brainpoolp384r1_y);
static const BIGNUM brainpoolp384r1_z_bn =
    EC_CURVE_BN(brainpoolp384r1_z);
static const BIGNUM brainpoolp384r1_order_bn =
    EC_CURVE_BN(brainpoolp384r1_order);
static const BIGNUM brainpoolp384r1_cofactor_bn =
    EC_CURVE_BN(brainpoolp384r1_cofactor);

static const BN_MONT_CTX brainpoolp384r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp384r1_p),
	.RR = EC_CURVE_BN(brainpoolp384r1_rr),
	.N = EC_CURVE_BN(brainpoolp384r1_p),
	.n0 = EC_CURVE_N0(0x9a6ea96cea9ec825ULL),
};

static const EC_POINT brainpoolp384r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP384r1,
	.X = (BIGNUM *)&brainpoolp384r1_x_bn,
	.Y = (BIGNUM *)&brainpoolp384r1_y_bn,
	.Z = (BIGNUM *)&brainpoolp384r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp384r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp384r1_generator,
	.order = (BIGNUM *)&brainpoolp384r1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp384r1_cofactor_bn,
	.nid = NID_brainpoolP384r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp384r1_p_bn,
	.a = (BIGNUM *)&brainpoolp384r1_a_bn,
	.b = (BIGNUM *)&brainpoolp384r1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp384r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP384t1 */

static const BIGNUM brainpoolp384t1_p_bn =
    EC_CURVE_BN(brainpoolp384t1_p);
static const BIGNUM brainpoolp384t1_a_bn =
    EC_CURVE_BN(brainpoolp384t1_a);
static const BIGNUM brainpoolp384t1_b_bn =
    EC_CURVE_BN(brainpoolp384t1_b);
static const BIGNUM brainpoolp384t1_x_bn =
    EC_CURVE_BN(brainpoolp384t1_x);
static const BIGNUM brainpoolp384t1_y_bn =
    EC_CURVE_BN(brainpoolp384t1_y);
static const BIGNUM brainpoolp384t1_z_bn =
    EC_CURVE_BN(brainpoolp384t1_z);
static const BIGNUM brainpoolp384t1_order_bn =
    EC_CURVE_BN(brainpoolp384t1_order);
static const BIGNUM brainpoolp384t1_cofactor_bn =
    EC_CURVE_BN(brainpoolp384t1_cofactor);

static const BN_MONT_CTX brainpoolp384t1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp384t1_p),
	.RR = EC_CURVE_BN(brainpoolp384t1_rr),
	.N = EC_CURVE_BN(brainpoolp384t1_p),
	.n0 = EC_CURVE_N0(0x9a6ea96cea9ec825ULL),
};

static const EC_POINT brainpoolp384t1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP384t1,
	.X = (BIGNUM *)&brainpoolp384t1_x_bn,
	.Y = (BIGNUM *)&brainpoolp384t1_y_bn,
	.Z = (BIGNUM *)&brainpoolp384t1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp384t1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp384t1_generator,
	.order = (BIGNUM *)&brainpoolp384t1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp384t1_cofactor_bn,
	.nid = NID_brainpoolP384t1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp384t1_p_bn,
	.a = (BIGNUM *)&brainpoolp384t1_a_bn,
	.b = (BIGNUM *)&brainpoolp384t1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp384t1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP512r1 */

static const BIGNUM brainpoolp512r1_p_bn =
    EC_CURVE_BN(brainpoolp512r1_p);
static const BIGNUM brainpoolp512r1_a_bn =
    EC_CURVE_BN(brainpoolp512r1_a);
static const BIGNUM brainpoolp512r1_b_bn =
    EC_CURVE_BN(brainpoolp512r1_b);
static const BIGNUM brainpoolp512r1_x_bn =
    EC_CURVE_BN(brainpoolp512r1_x);
static const BIGNUM brainpoolp512r1_y_bn =
    EC_CURVE_BN(brainpoolp512r1_y);
static const BIGNUM brainpoolp512r1_z_bn =
    EC_CURVE_BN(brainpoolp512r1_z);
static const BIGNUM brainpoolp512r1_order_bn =
    EC_CURVE_BN(brainpoolp512r1_order);
static const BIGNUM brainpoolp512r1_cofactor_bn =
    EC_CURVE_BN(brainpoolp512r1_cofactor);

static const BN_MONT_CTX brainpoolp512r1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp512r1_p),
	.RR = EC_CURVE_BN(brainpoolp512r1_rr),
	.N = EC_CURVE_BN(brainpoolp512r1_p),
	.n0 = EC_CURVE_N0(0x839b32207d89efc5ULL),
};

static const EC_POINT brainpoolp512r1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP512r1,
	.X = (BIGNUM *)&brainpoolp512r1_x_bn,
	.Y = (BIGNUM *)&brainpoolp512r1_y_bn,
	.Z = (BIGNUM *)&brainpoolp512r1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp512r1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp512r1_generator,
	.order = (BIGNUM *)&brainpoolp512r1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp512r1_cofactor_bn,
	.nid = NID_brainpoolP512r1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp512r1_p_bn,
	.a = (BIGNUM *)&brainpoolp512r1_a_bn,
	.b = (BIGNUM *)&brainpoolp512r1_b_bn,
	.a_is_minus3 = 0,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp512r1_mont,
	.static_params = 1,
};

/* RFC 5639 curve brainpoolP512t1 */

static const BIGNUM brainpoolp512t1_p_bn =
    EC_CURVE_BN(brainpoolp512t1_p);
static const BIGNUM brainpoolp512t1_a_bn =
    EC_CURVE_BN(brainpoolp512t1_a);
static const BIGNUM brainpoolp512t1_b_bn =
    EC_CURVE_BN(brainpoolp512t1_b);
static const BIGNUM brainpoolp512t1_x_bn =
    EC_CURVE_BN(brainpoolp512t1_x);
static const BIGNUM brainpoolp512t1_y_bn =
    EC_CURVE_BN(brainpoolp512t1_y);
static const BIGNUM brainpoolp512t1_z_bn =
    EC_CURVE_BN(brainpoolp512t1_z);
static const BIGNUM brainpoolp512t1_order_bn =
    EC_CURVE_BN(brainpoolp512t1_order);
static const BIGNUM brainpoolp512t1_cofactor_bn =
    EC_CURVE_BN(brainpoolp512t1_cofactor);

static const BN_MONT_CTX brainpoolp512t1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(brainpoolp512t1_p),
	.RR = EC_CURVE_BN(brainpoolp512t1_rr),
	.N = EC_CURVE_BN(brainpoolp512t1_p),
	.n0 = EC_CURVE_N0(0x839b32207d89efc5ULL),
};

static const EC_POINT brainpoolp512t1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_brainpoolP512t1,
	.X = (BIGNUM *)&brainpoolp512t1_x_bn,
	.Y = (BIGNUM *)&brainpoolp512t1_y_bn,
	.Z = (BIGNUM *)&brainpoolp512t1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP brainpoolp512t1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&brainpoolp512t1_generator,
	.order = (BIGNUM *)&brainpoolp512t1_order_bn,
	.cofactor = (BIGNUM *)&brainpoolp512t1_cofactor_bn,
	.nid = NID_brainpoolP512t1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&brainpoolp512t1_p_bn,
	.a = (BIGNUM *)&brainpoolp512t1_a_bn,
	.b = (BIGNUM *)&brainpoolp512t1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&brainpoolp512t1_mont,
	.static_params = 1,
};

/* ANSSI curve FRP256v1 */

static const BIGNUM frp256v1_p_bn =
    EC_CURVE_BN(frp256v1_p);
static const BIGNUM frp256v1_a_bn =
    EC_CURVE_BN(frp256v1_a);
static const BIGNUM frp256v1_b_bn =
    EC_CURVE_BN(frp256v1_b);
static const BIGNUM frp256v1_x_bn =
    EC_CURVE_BN(frp256v1_x);
static const BIGNUM frp256v1_y_bn =
    EC_CURVE_BN(frp256v1_y);
static const BIGNUM frp256v1_z_bn =
    EC_CURVE_BN(frp256v1_z);
static const BIGNUM frp256v1_order_bn =
    EC_CURVE_BN(frp256v1_order);
static const BIGNUM frp256v1_cofactor_bn =
    EC_CURVE_BN(frp256v1_cofactor);

static const BN_MONT_CTX frp256v1_mont = {
	.ri = BN_BITS2 * EC_CURVE_WORDS(frp256v1_p),
	.RR = EC_CURVE_BN(frp256v1_rr),
	.N = EC_CURVE_BN(frp256v1_p),
	.n0 = EC_CURVE_N0(0xc797483a164e1155ULL),
};

static const EC_POINT frp256v1_generator = {
	.meth = &ec_GFp_mont_method,
	.nid = NID_FRP256v1,
	.X = (BIGNUM *)&frp256v1_x_bn,
	.Y = (BIGNUM *)&frp256v1_y_bn,
	.Z = (BIGNUM *)&frp256v1_z_bn,
	.Z_is_one = 1,
};

static const EC_GROUP frp256v1_group = {
	.meth = &ec_GFp_mont_method,
	.generator = (EC_POINT *)&frp256v1_generator,
	.order = (BIGNUM *)&frp256v1_order_bn,
	.cofactor = (BIGNUM *)&frp256v1_cofactor_bn,
	.nid = NID_FRP256v1,
	.asn1_flag = OPENSSL_EC_NAMED_CURVE,
	.asn1_form = POINT_CONVERSION_UNCOMPRESSED,
	.p = (BIGNUM *)&frp256v1_p_bn,
	.a = (BIGNUM *)&frp256v1_a_bn,
	.b = (BIGNUM *)&frp256v1_b_bn,
	.a_is_minus3 = 1,
	.mont_ctx = (BN_MONT_CTX *)&frp256v1_mont,
	.static_params = 1,
};

#endif /* HEADER_EC_CURVE_TABLES_H */
