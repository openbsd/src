/*	$OpenBSD: db_ctf.c,v 1.1 2016/09/16 19:13:17 jasper Exp $	*/

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
#include <ddb/db_ctf.h>

#include <sys/exec_elf.h>
#include <sys/malloc.h>
#include <lib/libz/zlib.h>

extern db_symtab_t		db_symtab;

struct ddb_ctf {
	struct ctf_header 	*cth;
	const char 		*data;
	off_t			 dlen;
	unsigned int 		 ctf_found;
	unsigned int 		 nsyms;
        size_t			 ctftab_size;
	const char		*ctftab;
};

struct ddb_ctf db_ctf;

static const char	*db_ctf_lookup_name(unsigned int);
static const char	*db_ctf_idx2sym(size_t *, unsigned char);
static const char	*db_elf_find_ctftab(db_symtab_t *, size_t *);
static char		*db_ctf_decompress(const char *, size_t, off_t);
static int		 db_ctf_print_functions();
static int		 db_ctf_nsyms(void);

#define	ELF_CTF ".SUNW_ctf"

/*
 * Entrypoint to verify CTF presence, initialize the header, decompress
 * the data, etc.
 */
void
db_ctf_init(void)
{
	db_symtab_t *stab = &db_symtab;
	const char *ctftab;
	size_t ctftab_size;
	int nsyms;

	/* Assume nothing was correct found until proven otherwise. */
	db_ctf.ctf_found = 0;

	ctftab = db_elf_find_ctftab(stab, &ctftab_size);
	if (ctftab == NULL) {
		return;
	}

	db_ctf.ctftab = ctftab;
	db_ctf.ctftab_size = ctftab_size;
	db_ctf.cth = (struct ctf_header *)ctftab;
	db_ctf.dlen = db_ctf.cth->cth_stroff + db_ctf.cth->cth_strlen;

	/* Now decompress the section, take into account to skip the header */
	if (db_ctf.cth->cth_flags & CTF_F_COMPRESS) {
		if ((db_ctf.data =
		     db_ctf_decompress(db_ctf.ctftab + sizeof(*db_ctf.cth),
				      db_ctf.ctftab_size - sizeof(*db_ctf.cth),
				       db_ctf.dlen)) == NULL)
			return;
	} else {
		db_printf("Unsupported non-compressed CTF section encountered\n");
		return;
	}

	/* Lookup the total number of kernel symbols. */
	if ((nsyms = db_ctf_nsyms()) == 0)
		return;
	else
		db_ctf.nsyms = nsyms;

	/* We made it this far, everything seems fine. */
	db_ctf.ctf_found = 1;
}

/*
 * Internal helper function - return a pointer to the CTF table
 * for the current symbol table (and update the size).
 */
static const char *
db_elf_find_ctftab(db_symtab_t *stab, size_t *size)
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

	db_printf("CTF header found at %p (%ld)\n", db_ctf.ctftab,
		  db_ctf.ctftab_size);
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

#if 1
	db_ctf_print_functions();
#endif
	return;
}

/*
 * We need a way to get the number of symbols, so (ab)use db_elf_sym_forall()
 * to give us the count.
 */
struct db_ctf_forall_arg {
	int cnt;
	db_sym_t sym;
};

static void db_ctf_forall(db_sym_t, char *, char *, int, void *);

static void
db_ctf_forall(db_sym_t sym, char *name, char *suff, int pre, void *varg)
{
	struct db_ctf_forall_arg *arg = varg;

	if (arg->cnt-- == 0)
		arg->sym = sym;
}

static int
db_ctf_nsyms(void)
{
	int nsyms;
	struct db_ctf_forall_arg dfa;

	dfa.cnt = 0;
	db_elf_sym_forall(db_ctf_forall, &dfa);
	nsyms = -dfa.cnt;

	/* The caller must make sure to handle zero symbols. */
	return nsyms;
}

/*
 * Convert an index to a symbol name while ensuring the type is matched.
 * It must be noted this only works if the CTF table has the same order
 * as the symbol table.
 */
static const char *
db_ctf_idx2sym(size_t *idx, unsigned char type)
{
	db_symtab_t *stab = &db_symtab;
	Elf_Sym *symp, *symtab_start;
	const Elf_Sym *st;
	char *strtab;
	size_t i;

	if (stab->private == NULL)
		return (NULL);

	strtab = db_elf_find_strtab(stab);
	if (strtab == NULL)
		return (NULL);

	symtab_start = STAB_TO_SYMSTART(stab);
	symp = symtab_start;

	for (i = *idx + 1; i < db_ctf.nsyms; i++) {
		st = &symp[i];

		if (ELF_ST_TYPE(st->st_info) != type)
			continue;

		*idx = i;
		return strtab + st->st_name;
	}

	return (NULL);
}

/*
 * For a given function name, return the number of arguments.
 */
int
db_ctf_func_numargs(const char *funcname)
{
	const char		*s;
	unsigned short		*fsp, kind, vlen;
	size_t			 idx = 0;
	int			 nargs;

	if (!db_ctf.ctf_found)
		return (0);

	fsp = (unsigned short *)(db_ctf.data + db_ctf.cth->cth_funcoff);
	while (fsp < (unsigned short *)(db_ctf.data + db_ctf.cth->cth_typeoff)) {
		kind = CTF_INFO_KIND(*fsp);
		vlen = CTF_INFO_VLEN(*fsp);
		s = db_ctf_idx2sym(&idx, STT_FUNC);
		fsp++;

		if (kind == CTF_K_UNKNOWN && vlen == 0)
			continue;

		nargs = 0;
		if (s != NULL) {
			/*
			 * We have to keep increasing fsp++ while walking the
			 * table even if we discard the value at that location.
			 * This is required to keep a moving index.
			 *
			 * First increment for the return type, then for each
			 * parameter type.
			 */
			fsp++;

			while (vlen-- > 0) {
				nargs++;
				fsp++;
			}

			if (strncmp(funcname, s, strlen(funcname)) == 0) {
				return (nargs);
			}
		}
	}

	return (0);
}

static int
db_ctf_print_functions(void)
{
	unsigned short		*fsp, kind, vlen;
	size_t			 idx = 0, i = 0;
	const char		*s;
	int			 l;

	if (!db_ctf.ctf_found)
		return 1;

	fsp = (unsigned short *)(db_ctf.data + db_ctf.cth->cth_funcoff);
	while (fsp < (unsigned short *)(db_ctf.data + db_ctf.cth->cth_typeoff)) {
		kind = CTF_INFO_KIND(*fsp);
		vlen = CTF_INFO_VLEN(*fsp);
		fsp++;

		if (kind == CTF_K_UNKNOWN && vlen == 0)
			continue;

		l = db_printf("  [%zu] FUNC ", i++);
		if ((s = db_ctf_idx2sym(&idx, STT_FUNC)) != NULL)
			db_printf("(%s)", s);
		db_printf(" returns: %u args: (", *fsp++);
		while (vlen-- > 0)
			db_printf("%u%s", *fsp++, (vlen > 0) ? ", " : "");
		db_printf(") idx: %zu\n", idx);
	}
	db_printf("\n");
	return 0;
}

static const char *
db_ctf_lookup_name(unsigned int offset)
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

	/* XXX: drop malloc(9) usage */
	data = malloc(len, M_TEMP, M_WAITOK|M_ZERO|M_CANFAIL);
	if (data == NULL) {
		return (NULL);
	}

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

	return (data);

exit:
	free(data, M_DEVBUF, sizeof(*data));
	return (NULL);
}
