/* adapted from Eric Youngdale's readelf program */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf_abi.h>

char *xstrdup(char *);

char *readsoname(FILE *infile)
{
  Elf32_Ehdr *epnt;
  Elf32_Phdr *ppnt;
  int i;
  char *header;
  unsigned int dynamic_addr = 0;
  unsigned int dynamic_size = 0;
  int strtab_val = 0;
  int soname_val = 0;
  int loadaddr = -1;
  int loadbase = 0;
  Elf32_Dyn *dpnt;
  struct stat st;
  char *res = NULL;

  if (fstat(fileno(infile), &st))
    return NULL;
  header = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fileno(infile), 0);
  if (header == (caddr_t)-1)
    return NULL;

  epnt = (Elf32_Ehdr *)header;
  if ((int)(epnt+1) > (int)(header + st.st_size))
    goto skip;

  ppnt = (Elf32_Phdr *)&header[epnt->e_phoff];
  if ((int)ppnt < (int)header ||
      (int)(ppnt+epnt->e_phnum) > (int)(header + st.st_size))
    goto skip;

  for(i = 0; i < epnt->e_phnum; i++)
  {
    if (loadaddr == -1 && ppnt->p_vaddr != 0) 
      loadaddr = (ppnt->p_vaddr & 0xfffff000) -
	(ppnt->p_offset & 0xfffff000);
    if(ppnt->p_type == 2)
    {
      dynamic_addr = ppnt->p_offset;
      dynamic_size = ppnt->p_filesz;
    };
    ppnt++;
  };
    
  dpnt = (Elf32_Dyn *) &header[dynamic_addr];
  dynamic_size = dynamic_size / sizeof(Elf32_Dyn);
  if ((int)dpnt < (int)header ||
      (int)(dpnt+dynamic_size) > (int)(header + st.st_size))
    goto skip;
  
  while(dpnt->d_tag != DT_NULL)
  {
    if (dpnt->d_tag == DT_STRTAB)
      strtab_val = dpnt->d_un.d_val;
    if (dpnt->d_tag == DT_SONAME)
      soname_val = dpnt->d_un.d_val;
#define DT_MIPS_BASE_ADDRESS    0x70000006	/* XXX */
    if (dpnt->d_tag == DT_MIPS_BASE_ADDRESS)
      loadbase = dpnt->d_un.d_val;
    dpnt++;
  };

  if (!strtab_val || !soname_val)
    goto skip;
  if (soname_val + strtab_val - loadbase < 0 ||
      soname_val + strtab_val - loadbase >= st.st_size)
    goto skip;

  res = xstrdup((char *) (header - loadbase + strtab_val + soname_val));

 skip:
  munmap(header, st.st_size);

  return res;
}

