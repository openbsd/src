/*	$OpenBSD: dsp.c,v 1.18 2022/12/26 19:16:00 jmc Exp $	*/
/*
 * Copyright (c) 2008-2012 Alexandre Ratchov <alex@caoua.org>
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
#include <string.h>
#include "dsp.h"
#include "utils.h"

const int aparams_ctltovol[128] = {
	    0,
	  256,	  266,	  276,	  287,	  299,	  310,	  323,	  335,
	  348,	  362,	  376,	  391,	  406,	  422,	  439,	  456,
	  474,	  493,	  512,	  532,	  553,	  575,	  597,	  621,
	  645,	  670,	  697,	  724,	  753,	  782,	  813,	  845,
	  878,	  912,	  948,	  985,	 1024,	 1064,	 1106,	 1149,
	 1195,	 1241,	 1290,	 1341,	 1393,	 1448,	 1505,	 1564,
	 1625,	 1689,	 1756,	 1825,	 1896,	 1971,	 2048,	 2128,
	 2212,	 2299,	 2389,	 2483,	 2580,	 2682,	 2787,	 2896,
	 3010,	 3128,	 3251,	 3379,	 3511,	 3649,	 3792,	 3941,
	 4096,	 4257,	 4424,	 4598,	 4778,	 4966,	 5161,	 5363,
	 5574,	 5793,	 6020,	 6256,	 6502,	 6757,	 7023,	 7298,
	 7585,	 7883,	 8192,	 8514,	 8848,	 9195,	 9556,	 9931,
	10321,	10726,	11148,	11585,	12040,	12513,	13004,	13515,
	14045,	14596,	15170,	15765,	16384,	17027,	17696,	18390,
	19112,	19863,	20643,	21453,	22295,	23170,	24080,	25025,
	26008,	27029,	28090,	29193,	30339,	31530,	32768
};

const short dec_ulawmap[256] = {
	-32124, -31100, -30076, -29052, -28028, -27004, -25980, -24956,
	-23932, -22908, -21884, -20860, -19836, -18812, -17788, -16764,
	-15996, -15484, -14972, -14460, -13948, -13436, -12924, -12412,
	-11900, -11388, -10876, -10364,  -9852,  -9340,  -8828,  -8316,
	 -7932,  -7676,  -7420,  -7164,  -6908,  -6652,  -6396,  -6140,
	 -5884,  -5628,  -5372,  -5116,  -4860,  -4604,  -4348,  -4092,
	 -3900,  -3772,  -3644,  -3516,  -3388,  -3260,  -3132,  -3004,
	 -2876,  -2748,  -2620,  -2492,  -2364,  -2236,  -2108,  -1980,
	 -1884,  -1820,  -1756,  -1692,  -1628,  -1564,  -1500,  -1436,
	 -1372,  -1308,  -1244,  -1180,  -1116,  -1052,   -988,   -924,
	  -876,   -844,   -812,   -780,   -748,   -716,   -684,   -652,
	  -620,   -588,   -556,   -524,   -492,   -460,   -428,   -396,
	  -372,   -356,   -340,   -324,   -308,   -292,   -276,   -260,
	  -244,   -228,   -212,   -196,   -180,   -164,   -148,   -132,
	  -120,   -112,   -104,    -96,    -88,    -80,    -72,    -64,
	   -56,    -48,    -40,    -32,    -24,    -16,     -8,      0,
	 32124,  31100,  30076,  29052,  28028,  27004,  25980,  24956,
	 23932,  22908,  21884,  20860,  19836,  18812,  17788,  16764,
	 15996,  15484,  14972,  14460,  13948,  13436,  12924,  12412,
	 11900,  11388,  10876,  10364,   9852,   9340,   8828,   8316,
	  7932,   7676,   7420,   7164,   6908,   6652,   6396,   6140,
	  5884,   5628,   5372,   5116,   4860,   4604,   4348,   4092,
	  3900,   3772,   3644,   3516,   3388,   3260,   3132,   3004,
	  2876,   2748,   2620,   2492,   2364,   2236,   2108,   1980,
	  1884,   1820,   1756,   1692,   1628,   1564,   1500,   1436,
	  1372,   1308,   1244,   1180,   1116,   1052,    988,    924,
	   876,    844,    812,    780,    748,    716,    684,    652,
	   620,    588,    556,    524,    492,    460,    428,    396,
	   372,    356,    340,    324,    308,    292,    276,    260,
	   244,    228,    212,    196,    180,    164,    148,    132,
	   120,    112,    104,     96,     88,     80,     72,     64,
	    56,     48,     40,     32,     24,     16,      8,      0
};

const short dec_alawmap[256] = {
	 -5504,  -5248,  -6016,  -5760,  -4480,  -4224,  -4992,  -4736,
	 -7552,  -7296,  -8064,  -7808,  -6528,  -6272,  -7040,  -6784,
	 -2752,  -2624,  -3008,  -2880,  -2240,  -2112,  -2496,  -2368,
	 -3776,  -3648,  -4032,  -3904,  -3264,  -3136,  -3520,  -3392,
	-22016, -20992, -24064, -23040, -17920, -16896, -19968, -18944,
	-30208, -29184, -32256, -31232, -26112, -25088, -28160, -27136,
	-11008, -10496, -12032, -11520,  -8960,  -8448,  -9984,  -9472,
	-15104, -14592, -16128, -15616, -13056, -12544, -14080, -13568,
	  -344,   -328,   -376,   -360,   -280,   -264,   -312,   -296,
	  -472,   -456,   -504,   -488,   -408,   -392,   -440,   -424,
	   -88,    -72,   -120,   -104,    -24,     -8,    -56,    -40,
	  -216,   -200,   -248,   -232,   -152,   -136,   -184,   -168,
	 -1376,  -1312,  -1504,  -1440,  -1120,  -1056,  -1248,  -1184,
	 -1888,  -1824,  -2016,  -1952,  -1632,  -1568,  -1760,  -1696,
	  -688,   -656,   -752,   -720,   -560,   -528,   -624,   -592,
	  -944,   -912,  -1008,   -976,   -816,   -784,   -880,   -848,
	  5504,   5248,   6016,   5760,   4480,   4224,   4992,   4736,
	  7552,   7296,   8064,   7808,   6528,   6272,   7040,   6784,
	  2752,   2624,   3008,   2880,   2240,   2112,   2496,   2368,
	  3776,   3648,   4032,   3904,   3264,   3136,   3520,   3392,
	 22016,  20992,  24064,  23040,  17920,  16896,  19968,  18944,
	 30208,  29184,  32256,  31232,  26112,  25088,  28160,  27136,
	 11008,  10496,  12032,  11520,   8960,   8448,   9984,   9472,
	 15104,  14592,  16128,  15616,  13056,  12544,  14080,  13568,
	   344,    328,    376,    360,    280,    264,    312,    296,
	   472,    456,    504,    488,    408,    392,    440,    424,
	    88,     72,    120,    104,     24,      8,     56,     40,
	   216,    200,    248,    232,    152,    136,    184,    168,
	  1376,   1312,   1504,   1440,   1120,   1056,   1248,   1184,
	  1888,   1824,   2016,   1952,   1632,   1568,   1760,   1696,
	   688,    656,    752,    720,    560,    528,    624,    592,
	   944,    912,   1008,    976,    816,    784,    880,    848
};

const int resamp_filt[RESAMP_LENGTH / RESAMP_STEP + 1] = {
	      0,       0,       3,       9,      22,      42,      73,     116,
	    174,     248,     341,     454,     589,     749,     934,    1148,
	   1392,    1666,    1974,    2316,    2693,    3107,    3560,    4051,
	   4582,    5154,    5766,    6420,    7116,    7853,    8632,    9451,
	  10311,   11210,   12148,   13123,   14133,   15178,   16253,   17359,
	  18491,   19647,   20824,   22018,   23226,   24443,   25665,   26888,
	  28106,   29315,   30509,   31681,   32826,   33938,   35009,   36033,
	  37001,   37908,   38744,   39502,   40174,   40750,   41223,   41582,
	  41819,   41925,   41890,   41704,   41358,   40842,   40147,   39261,
	  38176,   36881,   35366,   33623,   31641,   29411,   26923,   24169,
	  21140,   17827,   14222,   10317,    6105,    1580,   -3267,   -8440,
	 -13944,  -19785,  -25967,  -32492,  -39364,  -46584,  -54153,  -62072,
	 -70339,  -78953,  -87911,  -97209, -106843, -116806, -127092, -137692,
	-148596, -159795, -171276, -183025, -195029, -207271, -219735, -232401,
	-245249, -258259, -271407, -284670, -298021, -311434, -324880, -338329,
	-351750, -365111, -378378, -391515, -404485, -417252, -429775, -442015,
	-453930, -465477, -476613, -487294, -497472, -507102, -516137, -524527,
	-532225, -539181, -545344, -550664, -555090, -558571, -561055, -562490,
	-562826, -562010, -559990, -556717, -552139, -546205, -538866, -530074,
	-519779, -507936, -494496, -479416, -462652, -444160, -423901, -401835,
	-377923, -352132, -324425, -294772, -263143, -229509, -193847, -156134,
	-116348,  -74474,  -30494,   15601,   63822,  114174,  166661,  221283,
	 278037,  336916,  397911,  461009,  526194,  593446,  662741,  734054,
	 807354,  882608,  959779, 1038826, 1119706, 1202370, 1286768, 1372846,
	1460546, 1549808, 1640566, 1732753, 1826299, 1921130, 2017169, 2114336,
	2212550, 2311723, 2411770, 2512598, 2614116, 2716228, 2818836, 2921841,
	3025142, 3128636, 3232218, 3335782, 3439219, 3542423, 3645282, 3747687,
	3849526, 3950687, 4051059, 4150530, 4248987, 4346320, 4442415, 4537163,
	4630453, 4722177, 4812225, 4900493, 4986873, 5071263, 5153561, 5233668,
	5311485, 5386917, 5459872, 5530259, 5597992, 5662986, 5725160, 5784436,
	5840739, 5893999, 5944148, 5991122, 6034862, 6075313, 6112422, 6146142,
	6176430, 6203247, 6226559, 6246335, 6262551, 6275185, 6284220, 6289647,
	6291456, 6289647, 6284220, 6275185, 6262551, 6246335, 6226559, 6203247,
	6176430, 6146142, 6112422, 6075313, 6034862, 5991122, 5944148, 5893999,
	5840739, 5784436, 5725160, 5662986, 5597992, 5530259, 5459872, 5386917,
	5311485, 5233668, 5153561, 5071263, 4986873, 4900493, 4812225, 4722177,
	4630453, 4537163, 4442415, 4346320, 4248987, 4150530, 4051059, 3950687,
	3849526, 3747687, 3645282, 3542423, 3439219, 3335782, 3232218, 3128636,
	3025142, 2921841, 2818836, 2716228, 2614116, 2512598, 2411770, 2311723,
	2212550, 2114336, 2017169, 1921130, 1826299, 1732753, 1640566, 1549808,
	1460546, 1372846, 1286768, 1202370, 1119706, 1038826,  959779,  882608,
	 807354,  734054,  662741,  593446,  526194,  461009,  397911,  336916,
	 278037,  221283,  166661,  114174,   63822,   15601,  -30494,  -74474,
	-116348, -156134, -193847, -229509, -263143, -294772, -324425, -352132,
	-377923, -401835, -423901, -444160, -462652, -479416, -494496, -507936,
	-519779, -530074, -538866, -546205, -552139, -556717, -559990, -562010,
	-562826, -562490, -561055, -558571, -555090, -550664, -545344, -539181,
	-532225, -524527, -516137, -507102, -497472, -487294, -476613, -465477,
	-453930, -442015, -429775, -417252, -404485, -391515, -378378, -365111,
	-351750, -338329, -324880, -311434, -298021, -284670, -271407, -258259,
	-245249, -232401, -219735, -207271, -195029, -183025, -171276, -159795,
	-148596, -137692, -127092, -116806, -106843,  -97209,  -87911,  -78953,
	 -70339,  -62072,  -54153,  -46584,  -39364,  -32492,  -25967,  -19785,
	 -13944,   -8440,   -3267,    1580,    6105,   10317,   14222,   17827,
	  21140,   24169,   26923,   29411,   31641,   33623,   35366,   36881,
	  38176,   39261,   40147,   40842,   41358,   41704,   41890,   41925,
	  41819,   41582,   41223,   40750,   40174,   39502,   38744,   37908,
	  37001,   36033,   35009,   33938,   32826,   31681,   30509,   29315,
	  28106,   26888,   25665,   24443,   23226,   22018,   20824,   19647,
	  18491,   17359,   16253,   15178,   14133,   13123,   12148,   11210,
	  10311,    9451,    8632,    7853,    7116,    6420,    5766,    5154,
	   4582,    4051,    3560,    3107,    2693,    2316,    1974,    1666,
	   1392,    1148,     934,     749,     589,     454,     341,     248,
	    174,     116,      73,      42,      22,       9,       3,       0,
	      0
};


/*
 * Generate a string corresponding to the encoding in par,
 * return the length of the resulting string.
 */
int
aparams_enctostr(struct aparams *par, char *ostr)
{
	char *p = ostr;

	*p++ = par->sig ? 's' : 'u';
	if (par->bits > 9)
		*p++ = '0' + par->bits / 10;
	*p++ = '0' + par->bits % 10;
	if (par->bps > 1) {
		*p++ = par->le ? 'l' : 'b';
		*p++ = 'e';
		if (par->bps != APARAMS_BPS(par->bits) ||
		    par->bits < par->bps * 8) {
			*p++ = par->bps + '0';
			if (par->bits < par->bps * 8) {
				*p++ = par->msb ? 'm' : 'l';
				*p++ = 's';
				*p++ = 'b';
			}
		}
	}
	*p++ = '\0';
	return p - ostr - 1;
}

/*
 * Parse an encoding string, examples: s8, u8, s16, s16le, s24be ...
 * set *istr to the char following the encoding. Return the number
 * of bytes consumed.
 */
int
aparams_strtoenc(struct aparams *par, char *istr)
{
	char *p = istr;
	int i, sig, bits, le, bps, msb;

#define IS_SEP(c)			\
	(((c) < 'a' || (c) > 'z') &&	\
	 ((c) < 'A' || (c) > 'Z') &&	\
	 ((c) < '0' || (c) > '9'))

	/*
	 * get signedness
	 */
	if (*p == 's') {
		sig = 1;
	} else if (*p == 'u') {
		sig = 0;
	} else
		return 0;
	p++;

	/*
	 * get number of bits per sample
	 */
	bits = 0;
	for (i = 0; i < 2; i++) {
		if (*p < '0' || *p > '9')
			break;
		bits = (bits * 10) + *p - '0';
		p++;
	}
	if (bits < BITS_MIN || bits > BITS_MAX)
		return 0;
	bps = APARAMS_BPS(bits);
	msb = 1;
	le = ADATA_LE;

	/*
	 * get (optional) endianness
	 */
	if (p[0] == 'l' && p[1] == 'e') {
		le = 1;
		p += 2;
	} else if (p[0] == 'b' && p[1] == 'e') {
		le = 0;
		p += 2;
	} else if (IS_SEP(*p)) {
		goto done;
	} else
		return 0;

	/*
	 * get (optional) number of bytes
	 */
	if (*p >= '0' && *p <= '9') {
		bps = *p - '0';
		if (bps < (bits + 7) / 8 ||
		    bps > (BITS_MAX + 7) / 8)
			return 0;
		p++;

		/*
		 * get (optional) alignment
		 */
		if (p[0] == 'm' && p[1] == 's' && p[2] == 'b') {
			msb = 1;
			p += 3;
		} else if (p[0] == 'l' && p[1] == 's' && p[2] == 'b') {
			msb = 0;
			p += 3;
		} else if (IS_SEP(*p)) {
			goto done;
		} else
			return 0;
	} else if (!IS_SEP(*p))
		return 0;

done:
	par->msb = msb;
	par->sig = sig;
	par->bits = bits;
	par->bps = bps;
	par->le = le;
	return p - istr;
}

/*
 * Initialise parameters structure with the defaults natively supported
 * by the machine.
 */
void
aparams_init(struct aparams *par)
{
	par->bps = sizeof(adata_t);
	par->bits = ADATA_BITS;
	par->le = ADATA_LE;
	par->sig = 1;
	par->msb = 0;
}

/*
 * log the given format/channels/encoding
 */
void
aparams_log(struct aparams *par)
{
	char enc[ENCMAX];

	aparams_enctostr(par, enc);
	log_puts(enc);
}

/*
 * return true if encoding corresponds to what we store in adata_t
 */
int
aparams_native(struct aparams *par)
{
	return par->sig &&
	    par->bps == sizeof(adata_t) &&
	    par->bits == ADATA_BITS &&
	    (par->bps == 1 || par->le == ADATA_LE) &&
	    (par->bits == par->bps * 8 || !par->msb);
}

/*
 * Return the number of input and output frame that would be consumed
 * by resamp_do(p, *icnt, *ocnt).
 */
void
resamp_getcnt(struct resamp *p, int *icnt, int *ocnt)
{
	long long idiff, odiff;
	int cdiff;

	cdiff = p->oblksz - p->diff;
	idiff = (long long)*icnt * p->oblksz;
	odiff = (long long)*ocnt * p->iblksz;
	if (odiff - idiff >= cdiff)
		*ocnt = (idiff + cdiff + p->iblksz - 1) / p->iblksz;
	else
		*icnt = (odiff + p->diff) / p->oblksz;
}

/*
 * Resample the given number of frames. The number of output frames
 * must match the corresponding number of input frames. Either always
 * use icnt and ocnt such that:
 *
 *	 icnt * oblksz = ocnt * iblksz
 *
 * or use resamp_getcnt() to calculate the proper numbers.
 */
void
resamp_do(struct resamp *p, adata_t *in, adata_t *out, int icnt, int ocnt)
{
	unsigned int nch;
	adata_t *idata;
	unsigned int oblksz;
	unsigned int ifr;
	int s, ds, diff;
	adata_t *odata;
	unsigned int iblksz;
	unsigned int ofr;
	unsigned int c;
	int64_t f[NCHAN_MAX];
	adata_t *ctxbuf, *ctx;
	unsigned int ctx_start;
	int q, qi, qf, n;

	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = in;
	odata = out;
	diff = p->diff;
	iblksz = p->iblksz;
	oblksz = p->oblksz;
	ctxbuf = p->ctx;
	ctx_start = p->ctx_start;
	nch = p->nch;
	ifr = icnt;
	ofr = ocnt;

	/*
	 * Start conversion.
	 */
#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("resamp: copying ");
		log_puti(ifr);
		log_puts(" -> ");
		log_putu(ofr);
		log_puts(" frames, diff = ");
		log_puti(diff);
		log_puts("\n");
	}
#endif
	for (;;) {
		if (diff >= oblksz) {
			if (ifr == 0)
				break;
			ctx_start = (ctx_start - 1) & (RESAMP_NCTX - 1);
			ctx = ctxbuf + ctx_start;
			for (c = nch; c > 0; c--) {
				*ctx = *idata++;
				ctx += RESAMP_NCTX;
			}
			diff -= oblksz;
			ifr--;
		} else {
			if (ofr == 0)
				break;

			for (c = 0; c < nch; c++)
				f[c] = 0;

			q = diff * p->filt_step;
			n = ctx_start;

			while (q < RESAMP_LENGTH) {
				qi = q >> RESAMP_STEP_BITS;
				qf = q & (RESAMP_STEP - 1);
				s = resamp_filt[qi];
				ds = resamp_filt[qi + 1] - s;
				s += (int64_t)qf * ds >> RESAMP_STEP_BITS;
				ctx = ctxbuf;
				for (c = 0; c < nch; c++) {
					f[c] += (int64_t)ctx[n] * s;
					ctx += RESAMP_NCTX;
				}
				q += p->filt_cutoff;
				n = (n + 1) & (RESAMP_NCTX - 1);
			}

			for (c = 0; c < nch; c++) {
				s = f[c] >> RESAMP_BITS;
				s = (int64_t)s * p->filt_cutoff >> RESAMP_BITS;
#if ADATA_BITS == 16
				/*
				 * In 16-bit mode, we've no room for filter
				 * overshoots, so we need to clip the signal
				 * to avoid 16-bit integers to wrap around.
				 * In 24-bit mode, samples may exceed the
				 * [-1:1] range. Later, cmap_add() will clip
				 * them, so no need to clip them here as well.
				 */
				if (s >= ADATA_UNIT)
					s = ADATA_UNIT - 1;
				else if (s < -ADATA_UNIT)
					s = -ADATA_UNIT;
#endif
				*odata++ = s;
			}

			diff += iblksz;
			ofr--;
		}
	}
	p->diff = diff;
	p->ctx_start = ctx_start;
#ifdef DEBUG
	if (ifr != 0) {
		log_puts("resamp_do: ");
		log_puti(ifr);
		log_puts(": too many input frames\n");
		panic();
	}
	if (ofr != 0) {
		log_puts("resamp_do: ");
		log_puti(ofr);
		log_puts(": too many output frames\n");
		panic();
	}
#endif
}

static unsigned int
uint_gcd(unsigned int a, unsigned int b)
{
	unsigned int r;

	while (b > 0) {
		r = a % b;
		a = b;
		b = r;
	}
	return a;
}

/*
 * initialize resampler with ibufsz/obufsz factor and "nch" channels
 */
void
resamp_init(struct resamp *p, unsigned int iblksz,
    unsigned int oblksz, int nch)
{
	unsigned int g;

	/*
	 * reduce iblksz/oblksz fraction
	 */
	g = uint_gcd(iblksz, oblksz);
	iblksz /= g;
	oblksz /= g;

	/*
	 * ensure weird rates don't cause integer overflow
	 */
	while (iblksz > ADATA_UNIT || oblksz > ADATA_UNIT) {
		iblksz >>= 1;
		oblksz >>= 1;
	}

	p->iblksz = iblksz;
	p->oblksz = oblksz;
	p->diff = 0;
	p->nch = nch;
	p->ctx_start = 0;
	memset(p->ctx, 0, sizeof(p->ctx));
	if (p->iblksz < p->oblksz) {
		p->filt_cutoff = RESAMP_UNIT;
		p->filt_step = RESAMP_UNIT / p->oblksz;
	} else {
		p->filt_cutoff = (int64_t)RESAMP_UNIT * p->oblksz / p->iblksz;
		p->filt_step = RESAMP_UNIT / p->iblksz;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("resamp: ");
		log_putu(iblksz);
		log_puts("/");
		log_putu(oblksz);
		log_puts("\n");
	}
#endif
}

/*
 * encode "todo" frames from native to foreign encoding
 */
void
enc_do(struct conv *p, unsigned char *in, unsigned char *out, int todo)
{
	unsigned int f;
	adata_t *idata;
	unsigned int s;
	unsigned int oshift;
	unsigned int obias;
	unsigned int obps;
	unsigned int i;
	unsigned char *odata;
	int obnext;
	int osnext;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("enc: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = (adata_t *)in;
	odata = out;
	oshift = p->shift;
	obias = p->bias;
	obps = p->bps;
	obnext = p->bnext;
	osnext = p->snext;

	/*
	 * Start conversion.
	 */
	odata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		/* convert adata to u32 */
		s = (int)*idata++ + ADATA_UNIT;
		s <<= 32 - ADATA_BITS;
		/* convert u32 to uN */
		s >>= oshift;
		/* convert uN to sN */
		s -= obias;
		/* packetize sN */
		for (i = obps; i > 0; i--) {
			*odata = (unsigned char)s;
			s >>= 8;
			odata += obnext;
		}
		odata += osnext;
	}
}

/*
 * store "todo" frames of silence in foreign encoding
 */
void
enc_sil_do(struct conv *p, unsigned char *out, int todo)
{
	unsigned int f;
	unsigned int s;
	unsigned int oshift;
	int obias;
	unsigned int obps;
	unsigned int i;
	unsigned char *odata;
	int obnext;
	int osnext;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("enc: silence ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	odata = out;
	oshift = p->shift;
	obias = p->bias;
	obps = p->bps;
	obnext = p->bnext;
	osnext = p->snext;

	/*
	 * Start conversion.
	 */
	odata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		s = ((1U << 31) >> oshift) - obias;
		for (i = obps; i > 0; i--) {
			*odata = (unsigned char)s;
			s >>= 8;
			odata += obnext;
		}
		odata += osnext;
	}
}

/*
 * initialize encoder from native to foreign encoding
 */
void
enc_init(struct conv *p, struct aparams *par, int nch)
{
	p->nch = nch;
	p->bps = par->bps;
	if (par->msb) {
		p->shift = 32 - par->bps * 8;
	} else {
		p->shift = 32 - par->bits;
	}
	if (par->sig) {
		p->bias = (1U << 31) >> p->shift;
	} else {
		p->bias = 0;
	}
	if (!par->le) {
		p->bfirst = par->bps - 1;
		p->bnext = -1;
		p->snext = 2 * par->bps;
	} else {
		p->bfirst = 0;
		p->bnext = 1;
		p->snext = 0;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("enc: ");
		aparams_log(par);
		log_puts(", ");
		log_puti(p->nch);
		log_puts(" channels\n");
	}
#endif
}

/*
 * decode "todo" frames from foreign to native encoding
 */
void
dec_do(struct conv *p, unsigned char *in, unsigned char *out, int todo)
{
	unsigned int f;
	unsigned int ibps;
	unsigned int i;
	unsigned int s = 0xdeadbeef;
	unsigned char *idata;
	int ibnext;
	int isnext;
	unsigned int ibias;
	unsigned int ishift;
	adata_t *odata;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("dec: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = in;
	odata = (adata_t *)out;
	ibps = p->bps;
	ibnext = p->bnext;
	ibias = p->bias;
	ishift = p->shift;
	isnext = p->snext;

	/*
	 * Start conversion.
	 */
	idata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		for (i = ibps; i > 0; i--) {
			s <<= 8;
			s |= *idata;
			idata += ibnext;
		}
		idata += isnext;
		s += ibias;
		s <<= ishift;
		s >>= 32 - ADATA_BITS;
		*odata++ = s - ADATA_UNIT;
	}
}

/*
 * convert a 32-bit float to adata_t, clipping to -1:1, boundaries
 * excluded
 */
static inline int
f32_to_adata(unsigned int x)
{
	unsigned int s, e, m, y;

	s = (x >> 31);
	e = (x >> 23) & 0xff;
	m = (x << 8) | 0x80000000;

	/*
	 * f32 exponent is (e - 127) and the point is after the 31-th
	 * bit, thus the shift is:
	 *
	 * 31 - (BITS - 1) - (e - 127)
	 *
	 * to ensure output is in the 0..(2^BITS)-1 range, the minimum
	 * shift is 31 - (BITS - 1) + 1, and maximum shift is 31
	 */
	if (e < 127 - (ADATA_BITS - 1))
		y = 0;
	else if (e >= 127)
		y = ADATA_UNIT - 1;
	else
		y = m >> (127 + (32 - ADATA_BITS) - e);
	return (y ^ -s) + s;
}

/*
 * convert samples from little endian ieee 754 floats to adata_t
 */
void
dec_do_float(struct conv *p, unsigned char *in, unsigned char *out, int todo)
{
	unsigned int f;
	unsigned int i;
	unsigned int s = 0xdeadbeef;
	unsigned char *idata;
	int ibnext;
	int isnext;
	adata_t *odata;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("dec_float: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	/*
	 * Partially copy structures into local variables, to avoid
	 * unnecessary indirections; this also allows the compiler to
	 * order local variables more "cache-friendly".
	 */
	idata = in;
	odata = (adata_t *)out;
	ibnext = p->bnext;
	isnext = p->snext;

	/*
	 * Start conversion.
	 */
	idata += p->bfirst;
	for (f = todo * p->nch; f > 0; f--) {
		for (i = 4; i > 0; i--) {
			s <<= 8;
			s |= *idata;
			idata += ibnext;
		}
		idata += isnext;
		*odata++ = f32_to_adata(s);
	}
}

/*
 * convert samples from ulaw/alaw to adata_t
 */
void
dec_do_ulaw(struct conv *p, unsigned char *in,
    unsigned char *out, int todo, int is_alaw)
{
	unsigned int f;
	unsigned char *idata;
	adata_t *odata;
	const short *map;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("dec_ulaw: copying ");
		log_putu(todo);
		log_puts(" frames\n");
	}
#endif
	map = is_alaw ? dec_alawmap : dec_ulawmap;
	idata = in;
	odata = (adata_t *)out;
	for (f = todo * p->nch; f > 0; f--)
		*odata++ = map[*idata++] << (ADATA_BITS - 16);
}

/*
 * initialize decoder from foreign to native encoding
 */
void
dec_init(struct conv *p, struct aparams *par, int nch)
{
	p->bps = par->bps;
	p->nch = nch;
	if (par->msb) {
		p->shift = 32 - par->bps * 8;
	} else {
		p->shift = 32 - par->bits;
	}
	if (par->sig) {
		p->bias = (1U << 31) >> p->shift;
	} else {
		p->bias = 0;
	}
	if (par->le) {
		p->bfirst = par->bps - 1;
		p->bnext = -1;
		p->snext = 2 * par->bps;
	} else {
		p->bfirst = 0;
		p->bnext = 1;
		p->snext = 0;
	}
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("dec: ");
		aparams_log(par);
		log_puts(", ");
		log_puti(p->nch);
		log_puts(" channels\n");
	}
#endif
}

/*
 * mix "todo" input frames on the output with the given volume
 */
void
cmap_add(struct cmap *p, void *in, void *out, int vol, int todo)
{
	adata_t *idata, *odata;
	int i, j, nch, istart, inext, onext, ostart, y, v;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("cmap: adding ");
		log_puti(todo);
		log_puts(" frames\n");
	}
#endif
	idata = in;
	odata = out;
	ostart = p->ostart;
	onext = p->onext;
	istart = p->istart;
	inext = p->inext;
	nch = p->nch;
	v = vol;

	/*
	 * map/mix input on the output
	 */
	for (i = todo; i > 0; i--) {
		odata += ostart;
		idata += istart;
		for (j = nch; j > 0; j--) {
			y = *odata + ADATA_MUL(*idata, v);
			if (y >= ADATA_UNIT)
				y = ADATA_UNIT - 1;
			else if (y < -ADATA_UNIT)
				y = -ADATA_UNIT;
			*odata = y;
			idata++;
			odata++;
		}
		odata += onext;
		idata += inext;
	}
}

/*
 * overwrite output with "todo" input frames with the given volume
 */
void
cmap_copy(struct cmap *p, void *in, void *out, int vol, int todo)
{
	adata_t *idata, *odata;
	int i, j, nch, istart, inext, onext, ostart, v;

#ifdef DEBUG
	if (log_level >= 4) {
		log_puts("cmap: copying ");
		log_puti(todo);
		log_puts(" frames\n");
	}
#endif
	idata = in;
	odata = out;
	ostart = p->ostart;
	onext = p->onext;
	istart = p->istart;
	inext = p->inext;
	nch = p->nch;
	v = vol;

	/*
	 * copy to the output buffer
	 */
	for (i = todo; i > 0; i--) {
		idata += istart;
		odata += ostart;
		for (j = nch; j > 0; j--) {
			*odata = ADATA_MUL(*idata, v);
			odata++;
			idata++;
		}
		odata += onext;
		idata += inext;
	}
}

/*
 * initialize channel mapper, to map a subset of input channel range
 * into a subset of the output channel range
 */
void
cmap_init(struct cmap *p,
    int imin, int imax, int isubmin, int isubmax,
    int omin, int omax, int osubmin, int osubmax)
{
	int cmin, cmax;

	cmin = -NCHAN_MAX;
	if (osubmin > cmin)
		cmin = osubmin;
	if (omin > cmin)
		cmin = omin;
	if (isubmin > cmin)
		cmin = isubmin;
	if (imin > cmin)
		cmin = imin;

	cmax = NCHAN_MAX;
	if (osubmax < cmax)
		cmax = osubmax;
	if (omax < cmax)
		cmax = omax;
	if (isubmax < cmax)
		cmax = isubmax;
	if (imax < cmax)
		cmax = imax;

	p->ostart = cmin - omin;
	p->onext = omax - cmax;
	p->istart = cmin - imin;
	p->inext = imax - cmax;
	p->nch = cmax - cmin + 1;
#ifdef DEBUG
	if (log_level >= 3) {
		log_puts("cmap: nch = ");
		log_puti(p->nch);
		log_puts(", ostart = ");
		log_puti(p->ostart);
		log_puts(", onext = ");
		log_puti(p->onext);
		log_puts(", istart = ");
		log_puti(p->istart);
		log_puts(", inext = ");
		log_puti(p->inext);
		log_puts("\n");
	}
#endif
}
