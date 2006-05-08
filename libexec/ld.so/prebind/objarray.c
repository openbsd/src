/* $OpenBSD: objarray.c,v 1.5 2006/05/08 20:34:36 deraadt Exp $ */
/*
 * Copyright (c) 2006 Dale Rahn <drahn@dalerahn.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslimits.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <nlist.h>
#include <elf_abi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include "resolve.h"
#include "link.h"
#include "sod.h"
#include "machine/reloc.h"
#include "prebind.h"
#include "prebind_struct.h"

struct objarray_list {
	struct elf_object *obj;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	u_int32_t id0;
	u_int32_t id1;
	u_int32_t *idxtolib;
	void *oprebind_data;
	int	numlibs;

	TAILQ_HEAD(, objlist) inst_list;
} *objarray;

int objarray_cnt;
int objarray_sz;

int write_txtbusy_file(char *name);
void copy_oldsymcache(int objidx);
void elf_load_existing_prebind(struct elf_object *object, int fd);

void
elf_add_object_curbin_list(struct elf_object *object)
{
	struct objlist *ol;
	ol = malloc(sizeof (struct objlist));
	ol->object = object;
	TAILQ_INSERT_TAIL(&(curbin->curbin_list), ol, list);
	if ( load_object == NULL)
		load_object = object;
	ol->load_prog = load_object;

#if 0
	printf("adding object %s %d with prog %s\n", object->load_name,
	    object->dyn.null, load_object->load_name);
#endif
	TAILQ_INSERT_TAIL(&(objarray[object->dyn.null].inst_list), ol, inst_list);
}
void
elf_init_objarray(void)
{
	objarray_sz = 512;
	objarray = malloc(sizeof (objarray[0]) * objarray_sz);
}

void
elf_sum_reloc()
{
	int i, numobjs;
	int err = 0;
	struct objlist *ol;
	struct proglist *pl;

	for (i = 0; i < objarray_cnt; i++) {
#if 0
		printf("%3d: %d obj %s\n", i, objarray[i].obj->dyn.null,
		    objarray[i].obj->load_name);
#endif
		if (TAILQ_EMPTY(&objarray[i].inst_list)) {
			printf("skipping  %s\n", objarray[i].obj->load_name);
			continue;
		}
		if (objarray[i].oprebind_data != NULL) {
			copy_oldsymcache(i);
			continue;
		}
		objarray[i].symcache = calloc(sizeof(struct symcache_noflag),
		    objarray[i].obj->nchains);
		objarray[i].pltsymcache = calloc(sizeof(struct symcache_noflag),
		    objarray[i].obj->nchains);

		if (objarray[i].symcache == NULL ||
		    objarray[i].pltsymcache == NULL) {
			printf("out of memory allocating symcache\n");
			exit (20);
		}

		TAILQ_FOREACH(ol, &(objarray[i].inst_list),
		    inst_list) {

#if 0
			printf("\tprog %d %s\n", ol->load_prog->dyn.null,
			    ol->load_prog->load_name);
			printf("cache: %p %p %s\n",
			     objarray[i].symcache, ol->cache,
			     ol->object->load_name );
#endif

			elf_copy_syms(objarray[i].symcache,
			    ol->cache,
			    ol->object,
			    ol->load_prog,
			    ol->object->nchains);

			elf_copy_syms(objarray[i].pltsymcache,
			    ol->pltcache,
			    ol->object,
			    ol->load_prog,
			    ol->object->nchains);
		}
	}
	TAILQ_FOREACH(ol, &library_list, list) {
#if 0
		printf("processing lib %s\n", ol->object->load_name);
#endif
		err += elf_prep_lib_prebind(ol->object);
	}
	TAILQ_FOREACH(pl, &prog_list, list) {
		numobjs = 0;
		TAILQ_FOREACH(ol, &(pl->curbin_list), list) {
			numobjs++;
		}
		pl->nobj = numobjs;
		pl->libmap = calloc(numobjs, sizeof (u_int32_t *));
		pl->fixup = calloc(2 * numobjs, sizeof (struct fixup *));
		pl->fixupcnt = calloc(2 * numobjs, sizeof (int));

		numobjs = 0;
		TAILQ_FOREACH(ol, &(pl->curbin_list), list) {
			elf_calc_fixups(pl, ol, numobjs);
			numobjs++;
		}
	}
	TAILQ_FOREACH(pl, &prog_list, list) {
		err += elf_prep_bin_prebind(pl);
#if 0
		printf("processing binary %s\n",
		    TAILQ_FIRST(&pl->curbin_list)->object->load_name);
#endif
	}
	if (err != 0)
		printf("failures %d\n", err);
}

int
elf_prep_lib_prebind(struct elf_object *object)
{
	int numlibs = 0;
	int ret = 0;
	int i;
	int ref_obj;
	int *libmap;
	int *idxtolib;
	struct nameidx *nameidx;
	char *nametab;
	int nametablen;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	struct symcachetab *symcachetab;
	int symcache_cnt = 0;
	struct symcachetab *pltsymcachetab;
	int pltsymcache_cnt = 0;

	symcache = objarray[object->dyn.null].symcache;
	pltsymcache = objarray[object->dyn.null].pltsymcache;
	libmap = calloc(objarray_cnt, sizeof (int));
	idxtolib = calloc(objarray_cnt, sizeof (int));
	objarray[object->dyn.null].idxtolib = idxtolib;

	for (i = 0; i < objarray_cnt; i++)
		libmap[i] = -1;

	nametablen = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (symcache[i].sym == NULL)
			continue;
		ref_obj = symcache[i].obj->dyn.null;
		symcache_cnt++;
		if (libmap[ref_obj] != -1)
			continue;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
		nametablen += strlen(symcache[i].obj->load_name) + 1;
		numlibs++;
	}
	symcachetab = calloc(symcache_cnt , sizeof(struct symcachetab));

	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (symcache[i].sym == NULL)
			continue;
		symcachetab[symcache_cnt].idx = i;
		symcachetab[symcache_cnt].obj_idx =
		    libmap[symcache[i].obj->dyn.null];
		symcachetab[symcache_cnt].sym_idx =
		    symcache[i].sym - symcache[i].obj->dyn.symtab;
		symcache_cnt++;
	}
	for (i = 0; i < object->nchains; i++) {
		if  (pltsymcache[i].sym == NULL)
			continue;
		ref_obj = pltsymcache[i].obj->dyn.null;
		pltsymcache_cnt++;
		if (libmap[ref_obj] != -1)
			continue;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
		nametablen += strlen(pltsymcache[i].obj->load_name) + 1;
		numlibs++;
	}
	pltsymcachetab = calloc(pltsymcache_cnt , sizeof(struct symcachetab));

	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (pltsymcache[i].sym == NULL)
			continue;
		pltsymcachetab[pltsymcache_cnt].idx = i;
		pltsymcachetab[pltsymcache_cnt].obj_idx =
		    libmap[pltsymcache[i].obj->dyn.null];
		pltsymcachetab[pltsymcache_cnt].sym_idx =
		    pltsymcache[i].sym - pltsymcache[i].obj->dyn.symtab;
		pltsymcache_cnt++;
	}

	objarray[object->dyn.null].numlibs = numlibs;

	nameidx = calloc(numlibs, sizeof (struct nameidx));
	nametab = malloc(nametablen);

	nametablen = 0;
	for (i = 0; i < numlibs; i++) {
		nameidx[i].name = nametablen;
		nameidx[i].id0 = objarray[idxtolib[i]].id0;
		nameidx[i].id1 = objarray[idxtolib[i]].id1;
		nametablen += strlen(objarray[idxtolib[i]].obj->load_name) + 1;
		strlcpy (&nametab[nameidx[i].name],
		    objarray[idxtolib[i]].obj->load_name,
		    nametablen - nameidx[i].name);
	}
#if 0
	for (i = 0; i < numlibs; i++) {
		printf("\tlib %s\n", &nametab[nameidx[i].name]);
	}
#endif

	/* skip writing lib if using old prebind data */
	if (objarray[object->dyn.null].oprebind_data == NULL)
		ret = elf_write_lib(object, nameidx, nametab, nametablen,
		    numlibs, 0, NULL, NULL, NULL, NULL, symcachetab,
		    symcache_cnt, pltsymcachetab, pltsymcache_cnt);

	free (nameidx);
	free (nametab);
	free (libmap);
	free(pltsymcachetab);
	free(symcachetab);

	return ret;
}

int
elf_prep_bin_prebind(struct proglist *pl)
{
	int ret;
	int numlibs = 0;
	int i, j;
	int ref_obj;
	int *libmap;
	int *idxtolib;
	struct nameidx *nameidx;
	char *nametab;
	int nametablen;
	struct symcache_noflag *symcache;
	struct symcache_noflag *pltsymcache;
	struct symcachetab *symcachetab;
	int symcache_cnt;
	struct symcachetab *pltsymcachetab;
	int pltsymcache_cnt;
	struct elf_object *object;
	struct objlist *ol;

	object = TAILQ_FIRST(&(pl->curbin_list))->object;
	symcache = objarray[object->dyn.null].symcache;
	pltsymcache = objarray[object->dyn.null].pltsymcache;
	libmap = calloc(objarray_cnt, sizeof (int));
	idxtolib = calloc(pl->nobj, sizeof (int));

	for (i = 0; i < objarray_cnt; i++)
		libmap[i] = -1;

	for (i = 0; i < pl->nobj; i++)
		idxtolib[i] = -1;

	nametablen = 0;
	TAILQ_FOREACH(ol, &(pl->curbin_list), list) {
		ref_obj = ol->object->dyn.null;
		nametablen += strlen(ol->object->load_name) + 1;
		libmap[ref_obj] = numlibs;
		idxtolib[numlibs] = ref_obj;
#if 0
	printf("obj :%d, idx %d %s\n", numlibs, ref_obj, ol->object->load_name);
#endif
		numlibs++;
	}

	/* do got */
	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (symcache[i].sym != NULL)
			symcache_cnt++;
	}

	symcachetab = calloc(symcache_cnt , sizeof(struct symcachetab));

	symcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (symcache[i].sym == NULL)
			continue;
		symcachetab[symcache_cnt].idx = i;
		symcachetab[symcache_cnt].obj_idx =
		    libmap[symcache[i].obj->dyn.null];
		symcachetab[symcache_cnt].sym_idx =
		    symcache[i].sym - symcache[i].obj->dyn.symtab;
		symcache_cnt++;
	}

	/* now do plt */
	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (pltsymcache[i].sym != NULL)
			pltsymcache_cnt++;
	}
	pltsymcachetab = calloc(pltsymcache_cnt , sizeof(struct symcachetab));

	pltsymcache_cnt = 0;
	for (i = 0; i < object->nchains; i++) {
		if  (pltsymcache[i].sym == NULL)
			continue;
		pltsymcachetab[pltsymcache_cnt].idx = i;
		pltsymcachetab[pltsymcache_cnt].obj_idx =
		    libmap[pltsymcache[i].obj->dyn.null];
		pltsymcachetab[pltsymcache_cnt].sym_idx =
		    pltsymcache[i].sym - pltsymcache[i].obj->dyn.symtab;
		pltsymcache_cnt++;
	}

	nameidx = calloc(numlibs, sizeof (struct nameidx));
	nametab = malloc(nametablen);

	if (nameidx == NULL || nametab == NULL)
		perror("buffers");

	nametablen = 0;
	for (i = 0; i < numlibs; i++) {
		nameidx[i].name = nametablen;
		nameidx[i].id0 = objarray[idxtolib[i]].id0;
		nameidx[i].id1 = objarray[idxtolib[i]].id1;
		nametablen += strlen(objarray[idxtolib[i]].obj->load_name) + 1;

		strlcpy (&nametab[nameidx[i].name],
		    objarray[idxtolib[i]].obj->load_name,
		    nametablen - nameidx[i].name);
	}
#if 0
	for (i = 0; i < numlibs; i++) {
		printf("\tlib %s\n", &nametab[nameidx[i].name]);
	}
#endif
	pl->libmapcnt = calloc(numlibs, sizeof(u_int32_t));

	/* have to do both got and plt fixups */
	for(i = 0; i < numlibs; i++) {
		for (j = 0; j < pl->fixupcnt[2*i]; j++) {
			pl->fixup[2*i][j].targobj_idx =
			    libmap[pl->fixup[2*i][j].targobj_idx];
		}
		for (j = 0; j < pl->fixupcnt[2*i+1]; j++) {
			pl->fixup[2*i+1][j].targobj_idx =
			    libmap[pl->fixup[2*i+1][j].targobj_idx];
		}

		pl->libmapcnt[i] = objarray[idxtolib[i]].numlibs;
		for (j = 0; j < objarray[idxtolib[i]].numlibs; j++)
			pl->libmap[i][j] = libmap[pl->libmap[i][j]];
	}

	ret = elf_write_lib(object, nameidx, nametab, nametablen, numlibs,
	    numlibs, pl->fixup, pl->fixupcnt,
	    pl->libmap, pl->libmapcnt,
	    symcachetab, symcache_cnt,
	    pltsymcachetab, pltsymcache_cnt);

	free(symcachetab);
	free(pltsymcachetab);
	free(idxtolib);
	free(nameidx);
	free(nametab);
	free(libmap);

	return ret;
}

int64_t prebind_blocks;
int
elf_write_lib(struct elf_object *object, struct nameidx *nameidx,
    char *nametab, int nametablen, int numlibs,
    int nfixup, struct fixup **fixup, int *fixupcnt,
    u_int32_t **libmap, int *libmapcnt,
    struct symcachetab *symcachetab, int symcache_cnt,
    struct symcachetab *pltsymcachetab, int pltsymcache_cnt)
{
	off_t base_offset;
	struct prebind_footer footer;
	struct stat ifstat;
	int fd;
	u_int32_t next_start;
	u_int32_t *fixuptab = NULL;
	u_int32_t *maptab = NULL;
	u_int32_t footer_offset;
	int i;
	size_t len;

	/* open the file */
	fd = open(object->load_name, O_RDWR);
	if (fd == -1) {
		if (errno == ETXTBSY)
			fd = write_txtbusy_file(object->load_name);
		if (fd == -1) {
			perror(object->load_name);
			return 1;
		}
	}
	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	len = read(fd, &footer, sizeof(struct prebind_footer));

	if (footer.bind_id[0] == BIND_ID0 &&
	    footer.bind_id[1] == BIND_ID1 &&
	    footer.bind_id[2] == BIND_ID2 &&
	    footer.bind_id[3] == BIND_ID3) {

		ftruncate(fd, footer.orig_size);
		elf_clear_prog_load(fd, object);
	}

	if (fstat(fd, &ifstat) == -1) {
		perror(object->load_name);
		exit(10);
	}
	bzero(&footer, sizeof(struct prebind_footer));

	base_offset = ifstat.st_size;
	prebind_blocks -= ifstat.st_blocks; /* subtract old size */

	/* verify dev/inode - do we care about last modified? */

	/* pieces to store on lib
	 *
	 * offset to footer
	 * nameidx		- numlibs * sizeof nameidx
	 * symcache		- symcache_cnt * sizeof (symcache_idx)
	 * pltsymcache		- pltsymcache_cnt * sizeof (symcache_idx)
	 * fixup(N/A for lib)	- nfixup * sizeof (symcache_idx)
	 * nametab		- nametablen
	 * footer	 (not aligned)
	 */

	footer.orig_size = base_offset;
	base_offset = ELF_ROUND(base_offset, sizeof(u_int64_t));
	footer.prebind_base = base_offset;
	footer.nameidx_idx = sizeof(u_int32_t);
	footer.symcache_idx = footer.nameidx_idx +
	    numlibs * sizeof (struct nameidx);
	footer.pltsymcache_idx = footer.symcache_idx +
	    symcache_cnt  * sizeof (struct nameidx);
	footer.symcache_cnt = symcache_cnt;
	footer.pltsymcache_cnt = pltsymcache_cnt;
	footer.fixup_cnt = 0;
	footer.numlibs = numlibs;
	next_start = footer.pltsymcache_idx +
	    (pltsymcache_cnt * sizeof (struct symcachetab));
	if (nfixup != 0) {
		footer.fixup_cnt = nfixup;
		footer.fixup_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		footer.fixupcnt_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		fixuptab = calloc( 2*nfixup, sizeof(u_int32_t));
		for ( i = 0; i < 2*nfixup; i++) {
			fixuptab[i] = next_start;
			next_start += fixupcnt[i] * sizeof(struct fixup);
		}
		footer.libmap_idx = next_start;
		next_start += 2*nfixup * sizeof(u_int32_t);
		maptab = calloc( 2*nfixup, sizeof(u_int32_t));
		maptab[0] = next_start;
		for (i = 1; i < nfixup; i++) {
			maptab[i] = next_start;
			next_start += libmapcnt[i] * sizeof(u_int32_t);
		}


	}
	footer.nametab_idx = next_start;
	next_start += nametablen;
	next_start = ELF_ROUND(next_start, sizeof(u_int64_t));
	footer_offset = next_start;
	if (verbose > 1) {
		printf("footer_offset %d\n", footer_offset);
	}
	footer.prebind_size = next_start + sizeof(struct prebind_footer);

	footer.prebind_version = PREBIND_VERSION;
	footer.id0 = objarray[object->dyn.null].id0;
	footer.id1 = objarray[object->dyn.null].id1;
	footer.bind_id[0] = BIND_ID0;
	footer.bind_id[1] = BIND_ID1;
	footer.bind_id[2] = BIND_ID2;
	footer.bind_id[3] = BIND_ID3;

	lseek(fd, footer.prebind_base, SEEK_SET);
	write(fd, &footer_offset, sizeof(u_int32_t));

	lseek(fd, footer.prebind_base+footer.nameidx_idx, SEEK_SET);
	write(fd, nameidx, numlibs * sizeof (struct nameidx));

	lseek(fd, footer.prebind_base+footer.symcache_idx, SEEK_SET);
	write(fd, symcachetab, symcache_cnt * sizeof (struct symcachetab));

	lseek(fd, footer.prebind_base+footer.pltsymcache_idx, SEEK_SET);
	write(fd, pltsymcachetab, pltsymcache_cnt *
	    sizeof (struct symcachetab));

	if (verbose > 3)
		dump_symcachetab(symcachetab, symcache_cnt, object, 0);
	if (verbose > 3)
		dump_symcachetab(pltsymcachetab, pltsymcache_cnt, object, 0);

	if (nfixup != 0) {
		lseek(fd, footer.prebind_base+footer.fixup_idx, SEEK_SET);
		write(fd, fixuptab, 2*nfixup * sizeof(u_int32_t));
		lseek(fd, footer.prebind_base+footer.fixupcnt_idx, SEEK_SET);
		write(fd, fixupcnt, 2*nfixup * sizeof(u_int32_t));
		for (i = 0; i < 2*nfixup; i++) {
			lseek(fd, footer.prebind_base+fixuptab[i],
			    SEEK_SET);
			write(fd, fixup[i], fixupcnt[i] * sizeof(struct fixup));
		}

		lseek(fd, footer.prebind_base+footer.libmap_idx, SEEK_SET);
		write(fd, maptab, nfixup * sizeof(u_int32_t));
		for (i = 0; i < nfixup; i++) {
			lseek(fd, footer.prebind_base+maptab[i],
			    SEEK_SET);
			write(fd, libmap[i], libmapcnt[i] * sizeof(u_int32_t));
		}
	}
	lseek(fd, footer.prebind_base+footer.nametab_idx, SEEK_SET);
	write(fd, nametab, nametablen);
	lseek(fd, footer.prebind_base+footer_offset, SEEK_SET);
	write(fd, &footer, sizeof (struct prebind_footer));

	if (fstat(fd, &ifstat) == -1) {
		perror(object->load_name);
		exit(10);
	}
	prebind_blocks += ifstat.st_blocks; /* add new size */
	if (nfixup != 0) {
		elf_fixup_prog_load(fd, &footer, object);

		free(fixuptab);
		free(maptab);
	}

	if (verbose > 0)
		printf("%s: prebind info %d bytes old size %lld, growth %f\n",
		    object->load_name, footer.prebind_size, footer.orig_size,
		    (double)(footer.prebind_size) / footer.orig_size);

	if (verbose > 1)
		elf_dump_footer(&footer);

	close (fd);
	return 0;
}
void
elf_fixup_prog_load(int fd, struct prebind_footer *footer,
    struct elf_object *object)
{
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Phdr phdr_empty;
	int loadsection;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap for write\n", object->load_name);
		return;
	}

	ehdr = (Elf_Ehdr *) buf;
	phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

	for (loadsection = 0; loadsection < ehdr->e_phnum; loadsection++) {
		if (phdr[loadsection].p_type == PT_LOAD)
			break;
	}

	/* verify that extra slot is empty */
	bzero(&phdr_empty, sizeof(phdr_empty));
	if (bcmp(&phdr[ehdr->e_phnum], &phdr_empty, sizeof(phdr_empty)) != 0) {
		printf("extra slot not empty\n");
		goto done;
	}
	phdr[ehdr->e_phnum].p_type = PT_LOAD;
	phdr[ehdr->e_phnum].p_flags = PF_R | 0x08000000;
	phdr[ehdr->e_phnum].p_offset = footer->prebind_base;
	phdr[ehdr->e_phnum].p_vaddr = footer->prebind_base | 0x80000000;
	phdr[ehdr->e_phnum].p_paddr = footer->prebind_base | 0x40000000;
	phdr[ehdr->e_phnum].p_filesz = footer->prebind_size;
	phdr[ehdr->e_phnum].p_memsz = footer->prebind_size;
	phdr[ehdr->e_phnum].p_align = phdr[loadsection].p_align;
	ehdr->e_phnum++;

done:
	msync(buf, 8192, MS_SYNC);
	munmap(buf, 8192);
}

void
elf_clear_prog_load(int fd, struct elf_object *object)
{
	void *buf;
	Elf_Ehdr *ehdr;
	Elf_Phdr *phdr;
	Elf_Phdr phdr_empty;
	int loadsection;

	buf = mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_FILE | MAP_SHARED,
	    fd, 0);
	if (buf == MAP_FAILED) {
		printf("%s: cannot mmap for write\n", object->load_name);
		return;
	}

	ehdr = (Elf_Ehdr *) buf;
	phdr = (Elf_Phdr *)((char *)buf + ehdr->e_phoff);

	if(ehdr->e_type != ET_EXEC) {
		goto done;
	}

	loadsection = ehdr->e_phnum - 1;
	if ((phdr[loadsection].p_type != PT_LOAD) ||
	    ((phdr[loadsection].p_flags & 0x08000000) == 0)) {
		/* doesn't look like ours */
		printf("mapped, %s id doesn't match %lx %d %d\n",
		    object->load_name,
		    (long)(phdr[loadsection].p_vaddr),
		    phdr[loadsection].p_flags, loadsection);
		goto done;
	}

	/* verify that extra slot is empty */
	bzero(&phdr[loadsection], sizeof(phdr_empty));

	ehdr->e_phnum--;

done:
	msync(buf, 8192, MS_SYNC);
	munmap(buf, 8192);
}
void
elf_calc_fixups(struct proglist *pl, struct objlist *ol, int libidx)
{
	int i;
	int numfixups;
	int objidx;
	int prog;
	struct symcache_noflag *symcache;
	struct elf_object *prog_obj;
	struct elf_object *interp;

	objidx = ol->object->dyn.null,

	prog_obj = TAILQ_FIRST(&(pl->curbin_list))->object;
	interp = prog_obj->load_object;
	prog = prog_obj->dyn.null;
	if (verbose > 3)
		printf("fixup GOT %s\n", ol->object->load_name);
	symcache = objarray[objidx].symcache;

	numfixups = 0;
	for (i = 0; i < ol->object->nchains; i++) {
		/*
		 * this assumes if the same object is found, the same
		 * symbol will be found as well
		 */
		if (ol->cache[i].obj != symcache[i].obj &&
		    symcache[i].obj != interp) {
			numfixups++;
		}
	}
	pl->fixup[2*libidx] = calloc(numfixups, sizeof (struct fixup));

	numfixups = 0;
	for (i = 0; i < ol->object->nchains; i++) {
		/*
		 * this assumes if the same object is found, the same
		 * symbol will be found as well
		 */
		if (ol->cache[i].obj != NULL &&
		    ol->cache[i].obj != symcache[i].obj) {
			struct fixup *f = &(pl->fixup[2*libidx][numfixups]);
			f->sym = i;
			f->targobj_idx = ol->cache[i].obj->dyn.null;
			f->sym_idx = ol->cache[i].sym -
			    ol->cache[i].obj->dyn.symtab;
			if (verbose > 3) {
				printf("obj %d idx %d targobj %d, sym idx %d\n",
				    i,
				    f->sym, f->targobj_idx, f->sym_idx);
			}

			numfixups++;
		}
	}
	pl->fixupcnt[2*libidx] = numfixups;
#if 0
	printf("prog %s obj %s had %d got fixups\n", prog_obj->load_name,
	    ol->object->load_name, numfixups);
#endif

	if (verbose > 3)
		printf("fixup PLT %s\n", ol->object->load_name);
	/* now PLT */

	symcache = objarray[objidx].pltsymcache;

	numfixups = 0;
	for (i = 0; i < ol->object->nchains; i++) {
		/*
		 * this assumes if the same object is found, the same
		 * symbol will be found as well
		 */
		if (ol->pltcache[i].obj != symcache[i].obj) {
			numfixups++;
		}
	}
	pl->fixup[2*libidx+1] = calloc(numfixups, sizeof (struct fixup));

	numfixups = 0;
	for (i = 0; i < ol->object->nchains; i++) {
		/*
		 * this assumes if the same object is found, the same
		 * symbol will be found as well
		 */
		if (ol->pltcache[i].obj != symcache[i].obj) {
			struct fixup *f = &(pl->fixup[2*libidx+1][numfixups]);
			f->sym = i;
			f->targobj_idx = ol->pltcache[i].obj->dyn.null;
			f->sym_idx = ol->pltcache[i].sym -
			    ol->pltcache[i].obj->dyn.symtab;
			if (verbose > 3) {
				printf("obj %d idx %d targobj %d, sym idx %d\n",
				    i,
				    f->sym, f->targobj_idx, f->sym_idx);
			}

			numfixups++;
		}
	}
	pl->fixupcnt[2*libidx+1] = numfixups;

	pl->libmap[libidx] = calloc( objarray[objidx].numlibs,
	    sizeof(u_int32_t));

	for (i = 0; i < objarray[objidx].numlibs; i++) {
		pl->libmap[libidx][i] = objarray[objidx].idxtolib[i];
	}
#if 0
	printf("prog %s obj %s had %d plt fixups\n", prog_obj->load_name,
	    ol->object->load_name, numfixups);
#endif

}

void
elf_add_object(struct elf_object *object, int objtype)
{
	struct objarray_list *newarray;
	struct objlist *ol;
	ol = malloc(sizeof (struct objlist));
	ol->object = object;
	if (objtype != OBJTYPE_EXE)
		TAILQ_INSERT_TAIL(&library_list, ol, list);
	if (objarray_cnt+1 >= objarray_sz) {
		objarray_sz += 512;
		newarray = realloc(objarray, sizeof (objarray[0]) *
		    objarray_sz);
		if (newarray != NULL)
			objarray = newarray;
		else  {
			perror("objarray");
			exit(20);
		}
	}
	object->dyn.null = objarray_cnt; /* Major abuse, I know */
	TAILQ_INIT(&(objarray[objarray_cnt].inst_list));
	objarray[objarray_cnt].obj = object;
	objarray[objarray_cnt].id0 = arc4random(); /* XXX FIX */
	objarray[objarray_cnt].id1 = arc4random();
	objarray[objarray_cnt].oprebind_data = NULL;
	objarray_cnt++;

	elf_add_object_curbin_list(object);
}

void
elf_free_curbin_list(elf_object_t *object)
{
	struct objlist *ol;
	int i;

	while (!TAILQ_EMPTY(&(curbin->curbin_list))) {
		ol = TAILQ_FIRST(&(curbin->curbin_list));
		TAILQ_REMOVE(&(objarray[ol->object->dyn.null].inst_list), ol, inst_list);
		TAILQ_REMOVE(&(curbin->curbin_list), ol, list);
		free(ol);
	}

	printf("trying to remove %s\n", object->load_name);
	for (i = objarray_cnt; i != 0;) {
		i--;
		printf("obj %s\n", objarray[i].obj->load_name);
		if (objarray[i].obj == object) {
			printf("found obj at %d max obj %d\n", i, objarray_cnt);
			TAILQ_FOREACH(ol, &(curbin->curbin_list), list) {
			}
			/* XXX - delete references */
			objarray_cnt = i;
			break;
		}
	}
}

void
elf_print_objarray(void)
{
	int i, j;
	struct objlist *ol;

	printf("loaded objs # %d\n", objarray_cnt);
	for (i = 0; i < objarray_cnt; i++) {
		printf("%3d: %d obj %s\n", i, (int)objarray[i].obj->dyn.null,
		    objarray[i].obj->load_name);
		TAILQ_FOREACH(ol, &(objarray[i].inst_list),
		    inst_list) {
			printf("\tprog %s\n", ol->load_prog->load_name);
			printf("got cache:\n");
			for (j = 0; j < ol->object->nchains; j++) {
				if (ol->cache[j].obj != NULL) {
					printf("symidx %d: obj %d sym %ld %s\n",
					    j, (int)ol->cache[j].obj->dyn.null,
					    ol->cache[j].sym -
					    ol->cache[j].obj->dyn.symtab,
					    ol->cache[j].sym->st_name +
					    ol->cache[j].obj->dyn.strtab
					    );
				}
			}
			printf("plt cache:\n");
			for (j = 0; j < ol->object->nchains; j++) {
				if (ol->pltcache[j].obj != NULL) {
					printf("symidx %d: obj %d sym %ld %s\n",
					    j, (int)ol->pltcache[j].obj->dyn.null,
					    ol->pltcache[j].sym -
					    ol->pltcache[j].obj->dyn.symtab,
					    ol->pltcache[j].sym->st_name +
					    ol->pltcache[j].obj->dyn.strtab
					    );
				}
			}
		}
	}
}

int
write_txtbusy_file(char *name)
{
	char *prebind_name;
	int fd;
	int oldfd;
	int err;
	struct stat sb;
	void *buf;
	size_t len, wlen;

	err = lstat(name, &sb);	/* get mode of old file (preserve mode) */
	if (err != 0)
		return -1; /* stat shouldn't fail but if it does */

	/* pick a better filename (pulling apart string?) */
	err = asprintf(&prebind_name, "%s%s", name, ".prebXXXXXXXXXX");
	if (err == -1) {
		/* fail */
		exit (10);	/* bail on memory failure */
	}
	mkstemp(prebind_name);

	/* allocate a 256k buffer to copy the file */
#define BUFSZ (256 * 1024)
	buf = malloc(BUFSZ);

	fd = open(prebind_name, O_RDWR|O_CREAT|O_TRUNC, sb.st_mode);
	oldfd = open(name, O_RDONLY);
	while ((len = read(oldfd,  buf, BUFSZ)) > 0) {
		wlen = write(fd, buf, len);
		if (wlen != len) {
			/* write failed */
				close(fd);
				close(oldfd);
				unlink(prebind_name);
				free(buf);
				return -1;
		}
	}

	/* this mode is used above, but is modified by umask */
	chmod (prebind_name, sb.st_mode);
	close(oldfd);
	unlink(name);
	rename (prebind_name, name);
	free (buf);

	return fd;
}

void
elf_load_existing_prebind(struct elf_object *object, int fd)
{
	struct prebind_footer footer;
	void *prebind_data;

	lseek(fd, -((off_t)sizeof(struct prebind_footer)), SEEK_END);
	read(fd, &footer, sizeof(struct prebind_footer));

	if (footer.bind_id[0] != BIND_ID0 ||
	    footer.bind_id[1] != BIND_ID1 ||
	    footer.bind_id[2] != BIND_ID2 ||
	    footer.bind_id[3] != BIND_ID3) {
		return;
	}

        prebind_data = mmap(0, footer.prebind_size, PROT_READ,
		    MAP_FILE, fd, footer.prebind_base);
	objarray[object->dyn.null].oprebind_data = prebind_data;
	objarray[object->dyn.null].id0 = footer.id0;
	objarray[object->dyn.null].id1 = footer.id1;
}
void
copy_oldsymcache(int objidx)
{
	void *prebind_map;
	struct prebind_footer *footer;
	struct elf_object *object;
	struct elf_object *tobj;
	struct symcache_noflag *tcache;
	struct symcachetab *symcache;
	int i, j;
	int found;
	char *c;
	u_int32_t offset;
	u_int32_t *poffset;
	struct nameidx *nameidx;
	char *nametab;
	int *idxtolib;


	object = objarray[objidx].obj;

	prebind_map = objarray[object->dyn.null].oprebind_data;

	objarray[objidx].symcache =
	    calloc(sizeof(struct symcache_noflag), object->nchains);
	objarray[objidx].pltsymcache =
	    calloc(sizeof(struct symcache_noflag), object->nchains);

	if (objarray[objidx].symcache == NULL ||
	    objarray[objidx].pltsymcache == NULL) {
		printf("out of memory allocating symcache\n");
		exit (20);
	}

	poffset = (u_int32_t *)prebind_map;
	c = prebind_map;
	offset = *poffset;
	c += offset;
	footer = (void *)c;

	nameidx = prebind_map + footer->nameidx_idx;
	nametab = prebind_map + footer->nametab_idx;

	idxtolib = calloc(footer->numlibs, sizeof(int));
	found = 0;
	for (i = 0; i < footer->numlibs; i++) {
		found = 0;
		for (j = 0; j < objarray_cnt; j++) {
			if (objarray[j].id0 == nameidx[i].id0 &&
			    objarray[j].id1 == nameidx[i].id1) {
				found = 1;
				idxtolib[i] = j;
				if (strcmp(objarray[j].obj->load_name,
				    &nametab[nameidx[i].name]) != 0) {
					printf("warning filename mismatch"
					    " [%s] [%s]\n",
					    objarray[j].obj->load_name,
					    &nametab[nameidx[i].name]);
				}
			}
		}
		if (found == 0)
			break;
	}
	if (found == 0)
		goto done;

	/* build idxtolibs */

	tcache = objarray[objidx].symcache;
	symcache = prebind_map + footer->symcache_idx;

	for (i = 0; i < footer->symcache_cnt; i++) {
		tobj =  objarray[idxtolib[symcache[i].obj_idx]].obj;

		tcache[symcache[i].idx].obj = tobj;
		tcache[symcache[i].idx].sym = tobj->dyn.symtab +
		    symcache[i].sym_idx;
	}

	tcache = objarray[objidx].pltsymcache;
	symcache = prebind_map + footer->pltsymcache_idx;
	for (i = 0; i < footer->pltsymcache_cnt; i++) {
		tobj =  objarray[idxtolib[symcache[i].obj_idx]].obj;

		tcache[symcache[i].idx].obj = tobj;
		tcache[symcache[i].idx].sym = tobj->dyn.symtab +
		    symcache[i].sym_idx;
	}
done:
	free (idxtolib);
	/* munmap(prebind_map, size);*/
}
