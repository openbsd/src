/*
 * (c) Thomas Pornin 1998, 1999, 2000
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. The name of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef UCPP__HASH__
#define UCPP__HASH__

struct hash_item;

struct HT {
	struct hash_item **lists;
	int nb_lists;
	int (*cmpdata)(void *, void *);
	int (*hash)(void *);
	void (*deldata)(void *);
};

int hash_string(char *);
struct HT *newHT(int, int (*)(void *, void *), int (*)(void *),
	void (*)(void *));
void *putHT(struct HT *, void *);
void *forceputHT(struct HT *, void *);
void *getHT(struct HT *, void *);
int delHT(struct HT *, void *);
void killHT(struct HT *);
void saveHT(struct HT *, void **);
void restoreHT(struct HT *, void **);
void tweakHT(struct HT *, void **, void *);
void scanHT(struct HT *, void (*)(void *));
int hash_struct(void *);
int cmp_struct(void *, void *);

#endif
