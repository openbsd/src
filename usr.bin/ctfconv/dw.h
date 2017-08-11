/*
 * Copyright (c) 2016 Martin Pieuchot
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

#ifndef _DW_H_
#define _DW_H_

struct dwbuf {
	const char		*buf;
	size_t			 len;
};

struct dwattr {
	SIMPLEQ_ENTRY(dwattr)	 dat_next;
	uint64_t		 dat_attr;
	uint64_t		 dat_form;
};

struct dwaval {
	SIMPLEQ_ENTRY(dwaval)	 dav_next;
	struct dwattr		*dav_dat;	/* corresponding attribute */
	union {
		struct dwbuf	 _buf;
		struct {
			const char	*_str;
			union {
				uint64_t	 _u64;
				int64_t		 _s64;
				uint32_t	 _u32;
				uint16_t	 _u16;
				uint8_t		 _u8;
			} _T;
		} _V;
	} AV;
#define dav_buf	AV._buf
#define dav_str	AV._V._str
#define dav_u64	AV._V._T._u64
#define dav_s64	AV._V._T._s64
#define dav_u32	AV._V._T._u32
#define dav_u16	AV._V._T._u16
#define dav_u8	AV._V._T._u8
};

SIMPLEQ_HEAD(dwaval_queue, dwaval);

struct dwdie {
	SIMPLEQ_ENTRY(dwdie)	 die_next;
	struct dwabbrev		*die_dab;
	size_t			 die_offset;
	uint8_t			 die_lvl;
	struct dwaval_queue	 die_avals;
};

SIMPLEQ_HEAD(dwdie_queue, dwdie);

struct dwabbrev {
	SIMPLEQ_ENTRY(dwabbrev)	 dab_next;
	uint64_t		 dab_code;
	uint64_t		 dab_tag;
	uint8_t			 dab_children;
	SIMPLEQ_HEAD(, dwattr)	 dab_attrs;
};

SIMPLEQ_HEAD(dwabbrev_queue, dwabbrev);

struct dwcu {
	uint64_t		 dcu_length;
	uint64_t		 dcu_abbroff;
	uint16_t		 dcu_version;
	uint8_t			 dcu_psize;
	size_t			 dcu_offset;	/* offset in the segment */
	struct dwabbrev_queue	 dcu_abbrevs;
	struct dwdie_queue	 dcu_dies;
};

const char	*dw_tag2name(uint64_t);
const char	*dw_at2name(uint64_t);
const char	*dw_form2name(uint64_t);
const char	*dw_op2name(uint8_t);

int	 dw_loc_parse(struct dwbuf *, uint8_t *, uint64_t *, uint64_t *);

int	 dw_ab_parse(struct dwbuf *, struct dwabbrev_queue *);
int	 dw_cu_parse(struct dwbuf *, struct dwbuf *, size_t, struct dwcu **);

void	 dw_dabq_purge(struct dwabbrev_queue *);
void	 dw_dcu_free(struct dwcu *);


#endif /* _DW_H_ */
