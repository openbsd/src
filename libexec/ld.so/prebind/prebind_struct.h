struct symcache_noflag {
	const elf_object_t *obj;
	const Elf_Sym	*sym;
};

struct objlist {
	TAILQ_ENTRY(objlist) list;
	TAILQ_ENTRY(objlist) inst_list;
	struct elf_object *load_prog;
	struct elf_object *object;
	struct symcache_noflag *cache;
	struct symcache_noflag *pltcache;
};

struct proglist {
	TAILQ_ENTRY(proglist) list;
	TAILQ_HEAD(, objlist) curbin_list;
	struct fixup **fixup;
	int *fixupcnt;
	int nobj;
	u_int32_t **libmap;
	u_int32_t *libmapcnt;
	char	*interp;
};
extern struct proglist *curbin;
extern struct elf_object *load_object;


typedef TAILQ_HEAD(, proglist) prog_list_ty;
typedef TAILQ_HEAD(, objlist) obj_list_ty;

extern obj_list_ty library_list;
extern prog_list_ty prog_list;

/* debug */
void elf_print_curbin_list(struct proglist *bin);
void elf_print_prog_list (prog_list_ty *prog_list);


/* objarray */
void elf_add_object_curbin_list(struct elf_object *object);

void elf_copy_syms(struct symcache_noflag *tcache,
    struct symcache_noflag *scache, struct elf_object *obj,
    struct elf_object *prog, int nsyms);
int elf_prep_lib_prebind(struct elf_object *object);
int elf_prep_bin_prebind(struct proglist *pl);
void elf_calc_fixups(struct proglist *pl, struct objlist *ol, int libidx);
int elf_write_lib(struct elf_object *object, struct nameidx *nameidx,
    char *nametab, int nametablen, int numlibs,
    int nfixup, struct fixup **fixup, int *fixupcnt,
    u_int32_t **libmap, int *libmapcnt,
    struct symcachetab *symcachetab, int symcache_cnt,
    struct symcachetab *pltsymcachetab, int pltsymcache_cnt);

void dump_symcachetab(struct symcachetab *symcachetab, int symcache_cnt, struct elf_object *object, int id);
void dump_info(struct elf_object *object);
void elf_clear_prog_load(int fd, struct elf_object *object);
void elf_fixup_prog_load(int fd, struct prebind_footer *footer,
    struct elf_object *object);
void elf_dump_footer(struct prebind_footer *footer);

extern int verbose;
void elf_load_existing_prebind(struct elf_object *object, int fd);
