/*
 * Copyright (c) 2001 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef ARLA_STATS_H
#define ARLA_STATS_H 1


/* Struct with collected statistics */
struct collect_stat{
    int64_t starttime;
};

void
collectstats_init (void);

void
collectstats_start (struct collect_stat *p);

void
collectstats_stop (struct collect_stat *p,
		   FCacheEntry *entry,
		   ConnCacheEntry *conn,
		   long partition,
		   int measure_type, int measure_items);

int
collectstats_hostpart(uint32_t *host, uint32_t *part, int *n);

int
collectstats_getentry(uint32_t host, uint32_t part, uint32_t type,
		      uint32_t items_slot, uint32_t *count,
		      int64_t *items_total, int64_t *total_time);

void
stats_set_prefetch(size_t sz);

size_t
stats_prefetch(ConnCacheEntry *conn, uint32_t part);

size_t
stats_fetch_round(ConnCacheEntry *conn, uint32_t part, size_t size);

#endif /* ARLA_STATS_H */
