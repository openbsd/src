/*
 * Copyright (c) 2008-2014 Alexandre Ratchov <alex@caoua.org>
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

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "afile.h"
#include "utils.h"

typedef struct {
	unsigned char ld[4];
} le32_t;

typedef struct {
	unsigned char lw[2];
} le16_t;

typedef struct {
	unsigned char bd[4];
} be32_t;

typedef struct {
	unsigned char bw[2];
} be16_t;

struct wav_riff {
	char id[4];
	le32_t size;
	char type[4];
};

struct wav_chunk {
	char id[4];
	le32_t size;
};

struct wav_fmt {
#define WAV_FMT_PCM	1
#define WAV_FMT_FLOAT	3
#define WAV_FMT_ALAW	6
#define WAV_FMT_ULAW	7
#define WAV_FMT_EXT	0xfffe
	le16_t fmt;
	le16_t nch;
	le32_t rate;
	le32_t byterate;
	le16_t blkalign;
	le16_t bits;
#define WAV_FMT_SIZE		 16
#define WAV_FMT_EXT_SIZE	(16 + 24)
	le16_t extsize;
	le16_t valbits;
	le32_t chanmask;
	le16_t extfmt;
	char guid[14];
};

struct wav_hdr {
	struct wav_riff riff;		/* 00..11 */
	struct wav_chunk fmt_hdr;	/* 12..20 */
	struct wav_fmt fmt;
	struct wav_chunk data_hdr;
};

struct aiff_form {
	char id[4];
	be32_t size;
	char type[4];
};

struct aiff_chunk {
	char id[4];
	be32_t size;
};

struct aiff_comm {
	struct aiff_commbase {
		be16_t nch;
		be32_t nfr;
		be16_t bits;
		/* rate in 80-bit floating point */
		be16_t rate_ex;
		be32_t rate_hi;
		be32_t rate_lo;
	} base;
	char comp_id[4];
	/* followed by stuff we don't care about */
};

struct aiff_data {
	be32_t offs;
	be32_t blksz;
};

struct aiff_hdr {
	struct aiff_form form;
	struct aiff_chunk comm_hdr;
	struct aiff_commbase comm;
	struct aiff_chunk data_hdr;
	struct aiff_data data;
};

struct au_hdr {
	char id[4];
	be32_t offs;
	be32_t size;
#define AU_FMT_PCM8	2
#define AU_FMT_PCM16	3
#define AU_FMT_PCM24	4
#define AU_FMT_PCM32	5
#define AU_FMT_FLOAT	6
#define AU_FMT_ALAW	0x1b
#define AU_FMT_ULAW	1
	be32_t fmt;
	be32_t rate;
	be32_t nch;
	char desc[8];
	/* followed by optional desc[] continuation */
};

const char wav_id_riff[4] = {'R', 'I', 'F', 'F'};
const char wav_id_wave[4] = {'W', 'A', 'V', 'E'};
const char wav_id_data[4] = {'d', 'a', 't', 'a'};
const char wav_id_fmt[4] = {'f', 'm', 't', ' '};
const char wav_guid[14] = {
	0x00, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x80, 0x00,
	0x00, 0xAA, 0x00, 0x38,
	0x9B, 0x71
};

const char aiff_id_form[4] = {'F', 'O', 'R', 'M'};
const char aiff_id_aiff[4] = {'A', 'I', 'F', 'F'};
const char aiff_id_aifc[4] = {'A', 'I', 'F', 'C'};
const char aiff_id_data[4] = {'S', 'S', 'N', 'D'};
const char aiff_id_comm[4] = {'C', 'O', 'M', 'M'};
const char aiff_id_none[4] = {'N', 'O', 'N', 'E'};
const char aiff_id_fl32[4] = {'f', 'l', '3', '2'};
const char aiff_id_ulaw[4] = {'u', 'l', 'a', 'w'};
const char aiff_id_alaw[4] = {'a', 'l', 'a', 'w'};

const char au_id[4] = {'.', 's', 'n', 'd'};

static inline unsigned int
le16_get(le16_t *p)
{
	return p->lw[0] | p->lw[1] << 8;
}

static inline void
le16_set(le16_t *p, unsigned int v)
{
	p->lw[0] = v;
	p->lw[1] = v >> 8;
}

static inline unsigned int
le32_get(le32_t *p)
{
	return p->ld[0] |
	       p->ld[1] << 8 |
	       p->ld[2] << 16 |
	       p->ld[3] << 24;
}

static inline void
le32_set(le32_t *p, unsigned int v)
{
	p->ld[0] = v;
	p->ld[1] = v >> 8;
	p->ld[2] = v >> 16;
	p->ld[3] = v >> 24;
}

static inline unsigned int
be16_get(be16_t *p)
{
	return p->bw[1] | p->bw[0] << 8;
}

static inline void
be16_set(be16_t *p, unsigned int v)
{
	p->bw[1] = v;
	p->bw[0] = v >> 8;
}

static inline unsigned int
be32_get(be32_t *p)
{
	return p->bd[3] |
	       p->bd[2] << 8 |
	       p->bd[1] << 16 |
	       p->bd[0] << 24;
}

static inline void
be32_set(be32_t *p, unsigned int v)
{
	p->bd[3] = v;
	p->bd[2] = v >> 8;
	p->bd[1] = v >> 16;
	p->bd[0] = v >> 24;
}

static int
afile_readhdr(struct afile *f, void *addr, size_t size)
{
	if (lseek(f->fd, 0, SEEK_SET) == -1) {
		log_puts(f->path);
		log_puts(": failed to seek to beginning of file\n");
		return 0;
	}
	if (read(f->fd, addr, size) != size) {
		log_puts(f->path);
		log_puts(": failed to read header\n");
		return 0;
	}
	return 1;
}

static int
afile_writehdr(struct afile *f, void *addr, size_t size)
{
	if (lseek(f->fd, 0, SEEK_SET) == -1) {
		log_puts(f->path);
		log_puts(": failed to seek back to header\n");
		return 0;
	}
	if (write(f->fd, addr, size) != size) {
		log_puts(f->path);
		log_puts(": failed to write header\n");
		return 0;
	}
	f->curpos = f->startpos;
	return 1;
}

static int
afile_checkpar(struct afile *f)
{
	if (f->nch == 0 || f->nch > NCHAN_MAX) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(f->nch);
		log_puts(": unsupported number of channels\n");
		return 0;
	}
	if (f->rate < RATE_MIN || f->rate > RATE_MAX) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(f->rate);
		log_puts(": unsupported rate\n");
		return 0;
	}
	if (f->par.bits < BITS_MIN || f->par.bits > BITS_MAX) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(f->par.bits);
		log_puts(": unsupported bits per sample\n");
		return 0;
	}
	if (f->par.bits > f->par.bps * 8) {
		log_puts(f->path);
		log_puts(": bits larger than bytes-per-sample\n");
		return 0;
	}
	if (f->fmt == AFILE_FMT_FLOAT && f->par.bits != 32) {
		log_puts(f->path);
		log_puts(": only 32-bit floating points are supported\n");
		return 0;
	}
	return 1;
}

static int
afile_wav_readfmt(struct afile *f, unsigned int csize)
{
	struct wav_fmt fmt;
	unsigned int wenc;

	if (csize < WAV_FMT_SIZE) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(csize);
		log_puts(": bogus format chunk size\n");
		return 0;
	}
	if (csize > WAV_FMT_EXT_SIZE)
		csize = WAV_FMT_EXT_SIZE;
	if (read(f->fd, &fmt, csize) != csize) {
		log_puts(f->path);
		log_puts(": failed to read format chunk\n");
		return 0;
	}
	wenc = le16_get(&fmt.fmt);
	f->par.bits = le16_get(&fmt.bits);
	if (wenc == WAV_FMT_EXT) {
		if (csize != WAV_FMT_EXT_SIZE) {
			log_puts(f->path);
			log_puts(": missing extended format chunk\n");
			return 0;
		}
		if (memcmp(fmt.guid, wav_guid, sizeof(wav_guid)) != 0) {
			log_puts(f->path);
			log_puts(": unknown format (GUID)\n");
			return 0;
		}
		f->par.bps = (f->par.bits + 7) / 8;
		f->par.bits = le16_get(&fmt.valbits);
		wenc = le16_get(&fmt.extfmt);
	} else
		f->par.bps = (f->par.bits + 7) / 8;
	f->nch = le16_get(&fmt.nch);
	f->rate = le32_get(&fmt.rate);
	f->par.le = 1;
	f->par.msb = 1;
	switch (wenc) {
	case WAV_FMT_PCM:
		f->fmt = AFILE_FMT_PCM;
		f->par.sig = (f->par.bits <= 8) ? 0 : 1;
		break;
	case WAV_FMT_ALAW:
		f->fmt = AFILE_FMT_ALAW;
		f->par.bits = 8;
		f->par.bps = 1;
		break;
	case WAV_FMT_ULAW:
		f->fmt = AFILE_FMT_ULAW;
		f->par.bits = 8;
		f->par.bps = 1;
		break;
	case WAV_FMT_FLOAT:
		f->fmt = AFILE_FMT_FLOAT;
		break;
	default:
		log_putu(wenc);
		log_puts(": unsupported encoding\n");
		return 0;
	}
	return afile_checkpar(f);
}

static int
afile_wav_readhdr(struct afile *f)
{
	struct wav_riff riff;
	struct wav_chunk chunk;
	unsigned int csize, rsize, pos = 0;
	int fmt_done = 0;

	if (!afile_readhdr(f, &riff, sizeof(struct wav_riff)))
		return 0;
	if (memcmp(&riff.id, &wav_id_riff, 4) != 0 ||
	    memcmp(&riff.type, &wav_id_wave, 4) != 0) {
		log_puts(f->path);
		log_puts(": not a .wav file\n");
		return 0;
	}
	rsize = le32_get(&riff.size);
	for (;;) {
		if (pos + sizeof(struct wav_chunk) > rsize) {
			log_puts(f->path);
			log_puts(": missing data chunk\n");
			return 0;
		}
		if (read(f->fd, &chunk, sizeof(chunk)) != sizeof(chunk)) {
			log_puts(f->path);
			log_puts(": failed to read chunk header\n");
			return 0;
		}
		csize = le32_get(&chunk.size);
		if (memcmp(chunk.id, wav_id_fmt, 4) == 0) {
			if (!afile_wav_readfmt(f, csize))
				return 0;
			fmt_done = 1;
		} else if (memcmp(chunk.id, wav_id_data, 4) == 0) {
			f->startpos = pos + sizeof(riff) + sizeof(chunk);
			f->endpos = f->startpos + csize;
			break;
		} else {
#ifdef DEBUG
			if (log_level >= 2) {
				log_puts(f->path);
				log_puts(": skipped unknown chunk\n");
			}
#endif
		}

		/*
		 * next chunk
		 */
		pos += sizeof(struct wav_chunk) + csize;
		if (lseek(f->fd, sizeof(riff) + pos, SEEK_SET) == -1) {
			log_puts(f->path);
			log_puts(": failed to seek to chunk\n");
			return 0;
		}
	}
	if (!fmt_done) {
		log_puts(f->path);
		log_puts(": missing format chunk\n");
		return 0;
	}
	return 1;
}

/*
 * Write header and seek to start position
 */
static int
afile_wav_writehdr(struct afile *f)
{
	struct wav_hdr hdr;

	memset(&hdr, 0, sizeof(struct wav_hdr));
	memcpy(hdr.riff.id, wav_id_riff, 4);
	memcpy(hdr.riff.type, wav_id_wave, 4);
	le32_set(&hdr.riff.size, f->endpos - sizeof(hdr.riff));
	memcpy(hdr.fmt_hdr.id, wav_id_fmt, 4);
	le32_set(&hdr.fmt_hdr.size, sizeof(hdr.fmt));
	le16_set(&hdr.fmt.fmt, WAV_FMT_EXT);
	le16_set(&hdr.fmt.nch, f->nch);
	le32_set(&hdr.fmt.rate, f->rate);
	le32_set(&hdr.fmt.byterate, f->rate * f->par.bps * f->nch);
	le16_set(&hdr.fmt.blkalign, f->par.bps * f->nch);
	le16_set(&hdr.fmt.bits, f->par.bits);
	le16_set(&hdr.fmt.extsize,
	    WAV_FMT_EXT_SIZE - WAV_FMT_SIZE - sizeof(hdr.fmt.extsize));
	le16_set(&hdr.fmt.valbits, f->par.bits);
	le16_set(&hdr.fmt.extfmt, 1);
	memcpy(&hdr.fmt.guid, wav_guid, sizeof(hdr.fmt.guid));
	memcpy(hdr.data_hdr.id, wav_id_data, 4);
	le32_set(&hdr.data_hdr.size, f->endpos - f->startpos);
	return afile_writehdr(f, &hdr, sizeof(struct wav_hdr));
}

static int
afile_aiff_readcomm(struct afile *f, unsigned int csize,
    int comp, unsigned int *nfr)
{
	struct aiff_comm comm;
	unsigned int csize_min;
	unsigned int e, m;

	csize_min = comp ?
	    sizeof(struct aiff_comm) : sizeof(struct aiff_commbase);
	if (csize < csize_min) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(csize);
		log_puts(": bogus comm chunk size\n");
		return 0;
	}
	if (read(f->fd, &comm, csize_min) != csize_min) {
		log_puts(f->path);
		log_puts(": failed to read comm chunk\n");
		return 0;
	}
	f->nch = be16_get(&comm.base.nch);
	e = be16_get(&comm.base.rate_ex);
	m = be32_get(&comm.base.rate_hi);
	if (e < 0x3fff || e > 0x3fff + 31) {
		log_puts(f->path);
		log_puts(": malformed sample rate\n");
		return 0;
	}
	f->rate = m >> (0x3fff + 31 - e);
	if (comp) {
		if (memcmp(comm.comp_id, aiff_id_none, 4) == 0) {
			f->fmt = AFILE_FMT_PCM;
			f->par.bits = be16_get(&comm.base.bits);
		} else if (memcmp(comm.comp_id, aiff_id_fl32, 4) == 0) {
			f->fmt = AFILE_FMT_FLOAT;
			f->par.bits = 32;
		} else if (memcmp(comm.comp_id, aiff_id_ulaw, 4) == 0) {
			f->fmt = AFILE_FMT_ULAW;
			f->par.bits = 8;
		} else if (memcmp(comm.comp_id, aiff_id_alaw, 4) == 0) {
			f->fmt = AFILE_FMT_ALAW;
			f->par.bits = 8;
		} else {
			log_puts(f->path);
			log_puts(": unsupported encoding\n");
			return 0;
		}
	} else {
		f->fmt = AFILE_FMT_PCM;
		f->par.bits = be16_get(&comm.base.bits);
	}
	f->par.le = 0;
	f->par.sig = 1;
	f->par.msb = 1;
	f->par.bps = (f->par.bits + 7) / 8;
	*nfr = be32_get(&comm.base.nfr);
	return afile_checkpar(f);
}

static int
afile_aiff_readdata(struct afile *f, unsigned int csize, unsigned int *roffs)
{
	struct aiff_data data;

	if (csize < sizeof(struct aiff_data)) {
		log_puts(f->path);
		log_puts(": ");
		log_putu(csize);
		log_puts(": bogus data chunk size\n");
		return 0;
	}
	csize = sizeof(struct aiff_data);
	if (read(f->fd, &data, csize) != csize) {
		log_puts(f->path);
		log_puts(": failed to read data chunk\n");
		return 0;
	}
	*roffs = csize + be32_get(&data.offs);
	return 1;
}

static int
afile_aiff_readhdr(struct afile *f)
{
	struct aiff_form form;
	struct aiff_chunk chunk;
	unsigned int csize, rsize, nfr = 0, pos = 0, offs;
	int comm_done = 0, comp;

	if (!afile_readhdr(f, &form, sizeof(struct aiff_form)))
		return 0;
	if (memcmp(&form.id, &aiff_id_form, 4) != 0) {
		log_puts(f->path);
		log_puts(": not an aiff file\n");
		return 0;
	}
	if (memcmp(&form.type, &aiff_id_aiff, 4) == 0) {
		comp = 0;
	} else if (memcmp(&form.type, &aiff_id_aifc, 4) == 0)
		comp = 1;
	else {
		log_puts(f->path);
		log_puts(": unsupported aiff file sub-type\n");
		return 0;
	}
	rsize = be32_get(&form.size);
	for (;;) {
		if (pos + sizeof(struct aiff_chunk) > rsize) {
			log_puts(f->path);
			log_puts(": missing data chunk\n");
			return 0;
		}
		if (read(f->fd, &chunk, sizeof(chunk)) != sizeof(chunk)) {
			log_puts(f->path);
			log_puts(": failed to read chunk header\n");
			return 0;
		}
		csize = be32_get(&chunk.size);
		if (memcmp(chunk.id, aiff_id_comm, 4) == 0) {
			if (!afile_aiff_readcomm(f, csize, comp, &nfr))
				return 0;
			comm_done = 1;
		} else if (memcmp(chunk.id, aiff_id_data, 4) == 0) {
			if (!afile_aiff_readdata(f, csize, &offs))
				return 0;
			f->startpos = sizeof(form) + pos +
			    sizeof(chunk) + offs;
			break;
		} else {
#ifdef DEBUG
			if (log_level >= 2) {
				log_puts(f->path);
				log_puts(": skipped unknown chunk\n");
			}
#endif
		}

		/*
		 * The aiff spec says "Each Chunk must contain an even
		 * number of bytes. For those Chunks whose total
		 * contents would yield an odd number of bytes, a zero
		 * pad byte must be added at the end of the Chunk. This
		 * pad byte is not included in ckDataSize, which
		 * indicates the size of the data in the Chunk."
		 */
		csize = (csize + 1) & ~1;
		pos += sizeof(struct aiff_chunk) + csize;

		if (lseek(f->fd, sizeof(form) + pos, SEEK_SET) == -1) {
			log_puts(f->path);
			log_puts(": failed to seek to chunk\n");
			return 0;
		}
	}
	if (!comm_done) {
		log_puts(f->path);
		log_puts(": missing comm chunk\n");
		return 0;
	}
	f->endpos = f->startpos + f->par.bps * f->nch * nfr;
	return 1;
}

/*
 * Write header and seek to start position
 */
static int
afile_aiff_writehdr(struct afile *f)
{
	struct aiff_hdr hdr;
	unsigned int bpf;
	unsigned int e, m;

	/* convert rate to 80-bit float (exponent and fraction part) */
	m = f->rate;
	e = 0x3fff + 31;
	while ((m & 0x80000000) == 0) {
		e--;
		m <<= 1;
	}

	/* bytes per frame */
	bpf = f->nch * f->par.bps;

	memset(&hdr, 0, sizeof(struct aiff_hdr));
	memcpy(hdr.form.id, aiff_id_form, 4);
	memcpy(hdr.form.type, aiff_id_aiff, 4);
	be32_set(&hdr.form.size, f->endpos - sizeof(hdr.form));

	memcpy(hdr.comm_hdr.id, aiff_id_comm, 4);
	be32_set(&hdr.comm_hdr.size, sizeof(hdr.comm));
	be16_set(&hdr.comm.nch, f->nch);
	be16_set(&hdr.comm.bits, f->par.bits);
	be16_set(&hdr.comm.rate_ex, e);
	be32_set(&hdr.comm.rate_hi, m);
	be32_set(&hdr.comm.rate_lo, 0);
	be32_set(&hdr.comm.nfr, (f->endpos - f->startpos) / bpf);

	memcpy(hdr.data_hdr.id, aiff_id_data, 4);
	be32_set(&hdr.data_hdr.size, f->endpos - f->startpos);
	be32_set(&hdr.data.offs, 0);
	be32_set(&hdr.data.blksz, 0);
	return afile_writehdr(f, &hdr, sizeof(struct aiff_hdr));
}

static int
afile_au_readhdr(struct afile *f)
{
	struct au_hdr hdr;
	unsigned int fmt;

	if (!afile_readhdr(f, &hdr, sizeof(struct au_hdr)))
		return 0;
	if (memcmp(&hdr.id, &au_id, 4) != 0) {
		log_puts(f->path);
		log_puts(": not a .au file\n");
		return 0;
	}
	f->startpos = be32_get(&hdr.offs);
	f->endpos = f->startpos + be32_get(&hdr.size);
	fmt = be32_get(&hdr.fmt);
	switch (fmt) {
	case AU_FMT_PCM8:
		f->fmt = AFILE_FMT_PCM;
		f->par.bits = 8;
		break;
	case AU_FMT_PCM16:
		f->fmt = AFILE_FMT_PCM;
		f->par.bits = 16;
		break;
	case AU_FMT_PCM24:
		f->fmt = AFILE_FMT_PCM;
		f->par.bits = 24;
		break;
	case AU_FMT_PCM32:
		f->fmt = AFILE_FMT_PCM;
		f->par.bits = 32;
		break;
	case AU_FMT_ULAW:
		f->fmt = AFILE_FMT_ULAW;
		f->par.bits = 8;
		break;
	case AU_FMT_ALAW:
		f->fmt = AFILE_FMT_ALAW;
		f->par.bits = 8;
		break;
	case AU_FMT_FLOAT:
		f->fmt = AFILE_FMT_FLOAT;
		f->par.bits = 32;
		break;
	default:
		log_puts(f->path);
		log_puts(": ");
		log_putu(fmt);
		log_puts(": unsupported encoding\n");
		return 0;
	}
	f->par.le = 0;
	f->par.sig = 1;
	f->par.bps = f->par.bits / 8;
	f->par.msb = 0;
	f->rate = be32_get(&hdr.rate);
	f->nch = be32_get(&hdr.nch);
	if (lseek(f->fd, f->startpos, SEEK_SET) == -1) {
		log_puts(f->path);
		log_puts(": ");
		log_puts("failed to seek to data chunk\n");
		return 0;
	}
	return afile_checkpar(f);
}

/*
 * Write header and seek to start position
 */
static int
afile_au_writehdr(struct afile *f)
{
	struct au_hdr hdr;
	unsigned int fmt;

	memset(&hdr, 0, sizeof(struct au_hdr));
	memcpy(hdr.id, au_id, 4);
	be32_set(&hdr.offs, f->startpos);
	be32_set(&hdr.size, f->endpos - f->startpos);
	switch (f->par.bits) {
	case 8:
		fmt = AU_FMT_PCM8;
		break;
	case 16:
		fmt = AU_FMT_PCM16;
		break;
	case 24:
		fmt = AU_FMT_PCM24;
		break;
	case 32:
		fmt = AU_FMT_PCM32;
		break;
#ifdef DEBUG
	default:
		log_puts(f->path);
		log_puts(": wrong precision\n");
		panic();
		return 0;
#endif
	}
	be32_set(&hdr.fmt, fmt);
	be32_set(&hdr.rate, f->rate);
	be32_set(&hdr.nch, f->nch);
	return afile_writehdr(f, &hdr, sizeof(struct au_hdr));
}

size_t
afile_read(struct afile *f, void *data, size_t count)
{
	off_t maxread;
	ssize_t n;

	if (f->endpos >= 0) {
		maxread = f->endpos - f->curpos;
		if (maxread == 0) {
#ifdef DEBUG
			if (log_level >= 3) {
				log_puts(f->path);
				log_puts(": end reached\n");
			}
#endif
			return 0;
		}
		if (count > maxread)
			count = maxread;
	}
	n = read(f->fd, data, count);
	if (n == -1) {
		log_puts(f->path);
		log_puts(": couldn't read\n");
		return 0;
	}
	f->curpos += n;
	return n;
}

size_t
afile_write(struct afile *f, void *data, size_t count)
{
	off_t maxwrite;
	int n;

	if (f->maxpos >= 0) {
		maxwrite = f->maxpos - f->curpos;
		if (maxwrite == 0) {
#ifdef DEBUG
			if (log_level >= 3) {
				log_puts(f->path);
				log_puts(": max file size reached\n");
			}
#endif
			return 0;
		}
		if (count > maxwrite)
			count = maxwrite;
	}
	n = write(f->fd, data, count);
	if (n == -1) {
		log_puts(f->path);
		log_puts(": couldn't write\n");
		return 0;
	}
	f->curpos += n;
	if (f->endpos < f->curpos)
		f->endpos = f->curpos;
	return n;
}

int
afile_seek(struct afile *f, off_t pos)
{
	pos += f->startpos;
	if (f->endpos >= 0 && pos > f->endpos && !f->par.sig) {
		log_puts(f->path);
		log_puts(": attempt to seek outside file boundaries\n");
		return 0;
	}

	/*
	 * seek only if needed to avoid errors with pipes & sockets
	 */
	if (pos != f->curpos) {
		if (lseek(f->fd, pos, SEEK_SET) == -1) {
			log_puts(f->path);
			log_puts(": couldn't seek\n");
			return 0;
		}
		f->curpos = pos;
	}
	return 1;
}

void
afile_close(struct afile *f)
{
	if (f->flags & AFILE_FWRITE) {
		if (f->hdr == AFILE_HDR_WAV)
			afile_wav_writehdr(f);
		else if (f->hdr == AFILE_HDR_AIFF)
			afile_aiff_writehdr(f);
		else if (f->hdr == AFILE_HDR_AU)
			afile_au_writehdr(f);
	}
	close(f->fd);
}

int
afile_open(struct afile *f, char *path, int hdr, int flags,
    struct aparams *par, int rate, int nch)
{
	char *ext;
	static union {
		struct wav_hdr wav;
		struct aiff_hdr aiff;
		struct au_hdr au;
	} dummy;

	f->par = *par;
	f->rate = rate;
	f->nch = nch;
	f->flags = flags;
	f->hdr = hdr;
	if (hdr == AFILE_HDR_AUTO) {
		f->hdr = AFILE_HDR_RAW;
		ext = strrchr(path, '.');
		if (ext != NULL) {
			ext++;
			if (strcasecmp(ext, "aif") == 0 ||
			    strcasecmp(ext, "aiff") == 0 ||
			    strcasecmp(ext, "aifc") == 0)
				f->hdr = AFILE_HDR_AIFF;
			else if (strcasecmp(ext, "au") == 0 ||
			    strcasecmp(ext, "snd") == 0)
				f->hdr = AFILE_HDR_AU;
			else if (strcasecmp(ext, "wav") == 0)
				f->hdr = AFILE_HDR_WAV;
		}
	}
	if (f->flags == AFILE_FREAD) {
		if (strcmp(path, "-") == 0) {
			f->path = "stdin";
			f->fd = STDIN_FILENO;
		} else {
			f->path = path;
			f->fd = open(f->path, O_RDONLY);
			if (f->fd == -1) {
				log_puts(f->path);
				log_puts(": failed to open for reading\n");
				return 0;
			}
		}
		if (f->hdr == AFILE_HDR_WAV) {
			if (!afile_wav_readhdr(f))
				goto bad_close;
		} else if (f->hdr == AFILE_HDR_AIFF) {
			if (!afile_aiff_readhdr(f))
				goto bad_close;
		} else if (f->hdr == AFILE_HDR_AU) {
			if (!afile_au_readhdr(f))
				goto bad_close;
		} else {
			f->startpos = 0;
			f->endpos = -1; /* read until EOF */
			f->fmt = AFILE_FMT_PCM;
		}
		f->curpos = f->startpos;
	} else if (flags == AFILE_FWRITE) {
		if (strcmp(path, "-") == 0) {
			f->path = "stdout";
			f->fd = STDOUT_FILENO;
		} else {
			f->path = path;
			f->fd = open(f->path,
			    O_WRONLY | O_TRUNC | O_CREAT, 0666);
			if (f->fd == -1) {
				log_puts(f->path);
				log_puts(": failed to create file\n");
				return 0;
			}
		}
		if (f->hdr == AFILE_HDR_WAV) {
			f->par.bps = (f->par.bits + 7) >> 3;
			if (f->par.bits > 8) {
				f->par.le = 1;
				f->par.sig = 1;
			} else
				f->par.sig = 0;
			if (f->par.bits & 7)
				f->par.msb = 1;
			f->endpos = f->startpos = sizeof(struct wav_hdr);
			f->maxpos = 0x7fffffff;
			if (!afile_writehdr(f, &dummy, sizeof(struct wav_hdr)))
				goto bad_close;
		} else if (f->hdr == AFILE_HDR_AIFF) {
			f->par.bps = (f->par.bits + 7) >> 3;
			if (f->par.bps > 1)
				f->par.le = 0;
			f->par.sig = 1;
			if (f->par.bits & 7)
				f->par.msb = 1;
			f->endpos = f->startpos = sizeof(struct aiff_hdr);
			f->maxpos = 0x7fffffff;
			if (!afile_writehdr(f, &dummy,
				sizeof(struct aiff_hdr)))
				goto bad_close;
		} else if (f->hdr == AFILE_HDR_AU) {
			f->par.bits = (f->par.bits + 7) & ~7;
			f->par.bps = f->par.bits / 8;
			f->par.le = 0;
			f->par.sig = 1;
			f->par.msb = 1;
			f->endpos = f->startpos = sizeof(struct au_hdr);
			f->maxpos = 0x7fffffff;
			if (!afile_writehdr(f, &dummy, sizeof(struct au_hdr)))
				goto bad_close;
		} else {
			f->endpos = f->startpos = 0;
			f->maxpos = -1;
		}
		f->curpos = f->startpos;
	} else {
#ifdef DEBUG
		log_puts("afile_open: wrong flags\n");
		panic();
#endif
	}
	return 1;
bad_close:
	close(f->fd);
	return 0;
}
