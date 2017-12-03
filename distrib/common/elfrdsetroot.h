/* $OpenBSD: elfrdsetroot.h,v 1.3 2017/12/03 19:32:19 tb Exp $ */

struct elf_fn {
	void (*locate_image)(int, struct elfhdr *,  char *, long *, long *,
	    off_t *, size_t *);
	int (*find_rd_root_image)(char *, int, Elf_Phdr *, int, long *, long *,
	    off_t *, size_t *);

};

extern int debug;
extern struct elf_fn ELF32_fn;
extern struct elf_fn ELF64_fn;
