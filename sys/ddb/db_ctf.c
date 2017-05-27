/*	$OpenBSD: db_ctf.c,v 1.6 2017/05/27 15:05:16 mpi Exp $	*/

/*
 * Copyright (c) 2016 Jasper Lievisse Adriaanse <jasper@openbsd.org>
 * Copyright (c) 2016 Martin Pieuchot <mpi@openbsd.org>
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
#include <sys/stdint.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>

#include <machine/db_machdep.h>

#include <ddb/db_extern.h>
#include <ddb/db_sym.h>
#include <ddb/db_elf.h>
#include <ddb/db_output.h>

#include <sys/exec_elf.h>
#include <sys/ctf.h>
#include <sys/malloc.h>
#include <lib/libz/zlib.h>

extern db_symtab_t		db_symtab;

struct ddb_ctf {
	struct ctf_header 	*cth;
	const char		*rawctf;	/* raw .SUNW_ctf section */
        size_t			 rawctflen;	/* raw .SUNW_ctf section size */
	const char 		*data;		/* decompressed CTF data */
	size_t			 dlen;		/* decompressed CTF data size */
	char			*strtab;	/* ELF string table */
	uint32_t 		 ctf_found;
};

struct ddb_ctf db_ctf;

static const char	*db_ctf_lookup_name(uint32_t);
static const char	*db_ctf_idx2sym(size_t *, uint8_t);
static const char	*db_elf_find_ctf(db_symtab_t *, size_t *);
static char		*db_ctf_decompress(const char *, size_t, off_t);

/*
 * Entrypoint to verify CTF presence, initialize the header, decompress
 * the data, etc.
 */
void
db_ctf_init(void)
{
	db_symtab_t *stab = &db_symtab;
	size_t rawctflen;

	/* Assume nothing was correct found until proven otherwise. */
	db_ctf.ctf_found = 0;

	if (stab->private == NULL)
		return;

	db_ctf.strtab = db_elf_find_strtab(stab);
	if (db_ctf.strtab == NULL)
		return;

	db_ctf.rawctf = db_elf_find_ctf(stab, &rawctflen);
	if (db_ctf.rawctf == NULL)
		return;

	db_ctf.rawctflen = rawctflen;
	db_ctf.cth = (struct ctf_header *)db_ctf.rawctf;
	db_ctf.dlen = db_ctf.cth->cth_stroff + db_ctf.cth->cth_strlen;

	if ((db_ctf.cth->cth_flags & CTF_F_COMPRESS) == 0) {
		printf("unsupported non-compressed CTF section\n");
		return;
	}

	/* Now decompress the section, take into account to skip the header */
	db_ctf.data = db_ctf_decompress(db_ctf.rawctf + sizeof(*db_ctf.cth),
	    db_ctf.rawctflen - sizeof(*db_ctf.cth), db_ctf.dlen);
	if (db_ctf.data == NULL)
		return;

	/* We made it this far, everything seems fine. */
	db_ctf.ctf_found = 1;
}

/*
 * Internal helper function - return a pointer to the CTF section
 */
static const char *
db_elf_find_ctf(db_symtab_t *stab, size_t *size)
{
	Elf_Ehdr *elf = STAB_TO_EHDR(stab);
	Elf_Shdr *shp = STAB_TO_SHDR(stab, elf);
	char *shstrtab;
	int i;

	shstrtab = (char *)elf + shp[elf->e_shstrndx].sh_offset;

	for (i = 0; i < elf->e_shnum; i++) {
		if ((shp[i].sh_flags & SHF_ALLOC) != 0 &&
		    strcmp(ELF_CTF, shstrtab+shp[i].sh_name) == 0) {
			*size = shp[i].sh_size;
			return ((char *)elf + shp[i].sh_offset);
		}
	}

	return (NULL);
}

void
db_dump_ctf_header(void)
{
	if (!db_ctf.ctf_found)
		return;

	db_printf("CTF header found at %p (%ld)\n", db_ctf.rawctf,
		  db_ctf.rawctflen);
	db_printf("cth_magic: 0x%04x\n", db_ctf.cth->cth_magic);
	db_printf("cth_verion: %d\n", db_ctf.cth->cth_version);
	db_printf("cth_flags: 0x%02x", db_ctf.cth->cth_flags);
	if (db_ctf.cth->cth_flags & CTF_F_COMPRESS) {
		db_printf(" (compressed)");
	}
	db_printf("\n");
	db_printf("cth_parlabel: %s\n",
		  db_ctf_lookup_name(db_ctf.cth->cth_parlabel));
	db_printf("cth_parname: %s\n",
		  db_ctf_lookup_name(db_ctf.cth->cth_parname));
	db_printf("cth_lbloff: %d\n", db_ctf.cth->cth_lbloff);
	db_printf("cth_objtoff: %d\n", db_ctf.cth->cth_objtoff);
	db_printf("cth_funcoff: %d\n", db_ctf.cth->cth_funcoff);
	db_printf("cth_typeoff: %d\n", db_ctf.cth->cth_typeoff);
	db_printf("cth_stroff: %d\n", db_ctf.cth->cth_stroff);
	db_printf("cth_strlen: %d\n", db_ctf.cth->cth_strlen);
}

/*
 * Convert an index to a symbol name while ensuring the type is matched.
 * It must be noted this only works if the CTF table has the same order
 * as the symbol table.
 */
static const char *
db_ctf_idx2sym(size_t *idx, uint8_t type)
{
	Elf_Sym *symp, *symtab_start, *symtab_end;
	size_t i = *idx + 1;

	symtab_start = STAB_TO_SYMSTART(&db_symtab);
	symtab_end = STAB_TO_SYMEND(&db_symtab);

	for (symp = &symtab_start[i]; symp < symtab_end; i++, symp++) {
		if (ELF_ST_TYPE(symp->st_info) != type)
			continue;

		*idx = i;
		return db_ctf.strtab + symp->st_name;
	}

	return NULL;
}

/*
 * For a given function name, return the number of arguments.
 */
int
db_ctf_func_numargs(const char *funcname)
{
	uint16_t		*fstart, *fend;
	const char		*fname;
	uint16_t		*fsp, kind, vlen;
	size_t			 i, idx = 0;

	if (!db_ctf.ctf_found)
		return 0;

	fstart = (uint16_t *)(db_ctf.data + db_ctf.cth->cth_funcoff);
	fend = (uint16_t *)(db_ctf.data + db_ctf.cth->cth_typeoff);

	fsp = fstart;
	while (fsp < fend) {
		fname = db_ctf_idx2sym(&idx, STT_FUNC);
		if (fname == NULL)
			break;

		kind = CTF_INFO_KIND(*fsp);
		vlen = CTF_INFO_VLEN(*fsp);
		fsp++;

		if (kind == CTF_K_UNKNOWN && vlen == 0)
			continue;

		/* Skip return type */
		fsp++;

		/* Skip argument types */
		for (i = 0; i < vlen; i++)
			fsp++;

		if (strcmp(funcname, fname) == 0)
			return vlen;
	}

	return 0;
}

static const char *
db_ctf_lookup_name(uint32_t offset)
{
	const char		*name;

	if (CTF_NAME_STID(offset) != CTF_STRTAB_0)
		return "external";

	if (CTF_NAME_OFFSET(offset) >= db_ctf.cth->cth_strlen)
		return "exceeds strlab";

	if (db_ctf.cth->cth_stroff + CTF_NAME_OFFSET(offset) >= db_ctf.dlen)
		return "invalid";

	name = db_ctf.data + db_ctf.cth->cth_stroff + CTF_NAME_OFFSET(offset);
	if (*name == '\0')
		return "(anon)";

	return name;
}

static char *
db_ctf_decompress(const char *buf, size_t size, off_t len)
{
	z_stream		 stream;
	char			*data;
	int			 error;

	data = malloc(len, M_TEMP, M_WAITOK|M_ZERO|M_CANFAIL);
	if (data == NULL)
		return NULL;

	memset(&stream, 0, sizeof(stream));
	stream.next_in = (void *)buf;
	stream.avail_in = size;
	stream.next_out = data;
	stream.avail_out = len;

	if ((error = inflateInit(&stream)) != Z_OK) {
		db_printf("zlib inflateInit failed: %s", zError(error));
		goto exit;
	}

	if ((error = inflate(&stream, Z_FINISH)) != Z_STREAM_END) {
		db_printf("zlib inflate failed: %s", zError(error));
		inflateEnd(&stream);
		goto exit;
	}

	if ((error = inflateEnd(&stream)) != Z_OK) {
		db_printf("zlib inflateEnd failed: %s", zError(error));
		goto exit;
	}

	if (stream.total_out != len) {
		db_printf("decompression failed: %llu != %llu",
		    stream.total_out, len);
		goto exit;
	}

	return data;

exit:
	free(data, M_DEVBUF, sizeof(*data));
	return NULL;
}
