
#include <stdio.h>
#include <a.out.h>

#define		SYMTAB_SPACE  0x8000
int db_symtabsize = SYMTAB_SPACE;
char db_symtab[SYMTAB_SPACE] = { 0,0,0,0,1 };
/*
 * The actual format of the above is:
 *	int symtab_length = NSYMS;
 *	struct nlist[NSYMS];
 *	int strtab_length;
 *	char strtab[];
 */

/* Print out our symbol table. */
main()
{
	struct nlist *nl;
	int symtab_len, strtab_len;
	char *strtab;
	char *p;
	int *ip;
	int st, sc, x;

	/* symbol table */
	ip = (int*) db_symtab;
	symtab_len = *ip++;
	if (symtab_len < 4) {
		printf("no symbol table\n");
		exit(1);
	}
	nl = (struct nlist *) ip;

	/* string table pointer and length */
	ip = (int*) ((char*)ip + symtab_len);
	strtab =  (char*)ip;
	strtab_len = *ip;

	if (strtab_len > (SYMTAB_SPACE - symtab_len))
		strtab_len = (SYMTAB_SPACE - symtab_len);

	/* print symbol table */
	while ((x=nl->n_un.n_strx) != 0) {
		if (x < 0 || x >= strtab_len) p = "?";
		else p = strtab + x;
		st = nl->n_type & 0x1F;
		sc = "uatdbxxxxcxxxxxxx"[st>1];
		if (st & 1) sc &= ~0x20;	/* to upper */
		printf("%08X %c %s\n", nl->n_value, sc, p);
		nl++;
		if ((char*)nl >= strtab) {
			printf("symbol table missing null terminator\n");
			break;
		}
	}
	exit(0);
}
