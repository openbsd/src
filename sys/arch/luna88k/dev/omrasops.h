/*
 * Copyright (c) 2013 Kenji Aoyama
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
 * Base addresses of LUNA's frame buffer
 * XXX: We consider only 1bpp for now
 */
#define OMFB_FB_WADDR   0xB1080008      /* common plane */
#define OMFB_FB_RADDR   0xB10C0008      /* plane #0 */

/*
 * Helper macros
 */
#define W(p) (*(u_int32_t *)(p))
#define R(p) (*(u_int32_t *)((u_int8_t *)(p) + 0x40000))

/*
 * Replacement Rules (rops) (derived from hp300)
 */
#define RR_CLEAR        0x0
#define RR_COPY         0x3
