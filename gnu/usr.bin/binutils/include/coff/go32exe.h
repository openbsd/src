/* COFF information for PC running go32.  */

#define STUBSIZE 2048

struct external_filehdr_go32_exe {
	char stub[STUBSIZE];	/* the stub to load the image	*/
	/* the standard COFF header */
	char f_magic[2];	/* magic number			*/
	char f_nscns[2];	/* number of sections		*/
	char f_timdat[4];	/* time & date stamp		*/
	char f_symptr[4];	/* file pointer to symtab	*/
	char f_nsyms[4];	/* number of symtab entries	*/
	char f_opthdr[2];	/* sizeof(optional hdr)		*/
	char f_flags[2];	/* flags			*/
};

#undef FILHDR
#define	FILHDR	struct external_filehdr_go32_exe
#undef FILHSZ
#define	FILHSZ	STUBSIZE+20
