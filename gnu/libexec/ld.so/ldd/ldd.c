/*	$OpenBSD: ldd.c,v 1.1 1996/10/04 21:27:05 pefo Exp $ */

/*
 * Copyright (c) 1996 Per Fogelstrom
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed under OpenBSD by
 *	Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */


/* readsoname() adapted from Eric Youngdale's readelf program */


#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <elf_abi.h>

int readsoneeded(FILE *);

main(argc, argv)
	int argc;
	char **argv;
{
	FILE *f;

	int lflag = 1;
	int i, j;

	if(argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s [-l] file\n", argv[0]);
		exit(1);
	}

	for(j = 1; j < argc; j ++) {
		if(argv[j][0] == '-' && argv[j][1] == 'x')
			lflag=0;
		else
			i = j;
	}

	f = fopen(argv[i], "r");
	if(f == 0) {
		fprintf(stderr, "%s: can't find '%s'.\n", argv[0], argv[i]);
		exit(2);
	}
	if(!lflag)
		readsoneeded(f);
	fclose(f);

	if(lflag) {
		setenv("LD_TRACE_LOADED_OBJECTS", "1", 1);
		execl(argv[i], NULL);
	}
	exit(0);
}

int readsoneeded(FILE *infile)
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
    return -1L;
  header = mmap(0, st.st_size, PROT_READ, MAP_SHARED, fileno(infile), 0);
  if (header == (caddr_t)-1)
    return -1;

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
#define DT_MIPS_BASE_ADDRESS    0x70000006      /* XXX */
    if (dpnt->d_tag == DT_MIPS_BASE_ADDRESS)
      loadbase = dpnt->d_un.d_val;
    dpnt++;
  };

  if (!strtab_val)
    goto skip;

  dpnt = (Elf32_Dyn *) &header[dynamic_addr];
  while(dpnt->d_tag != DT_NULL)
  {
    if (dpnt->d_tag == DT_NEEDED) {
      soname_val = dpnt->d_un.d_val;
      if (soname_val != 0 && soname_val + strtab_val - loadbase >= 0 &&
        soname_val + strtab_val - loadbase < st.st_size)
        printf("%s\n", header - loadbase + soname_val + strtab_val);
    }
    dpnt++;
  };

 skip:
  munmap(header, st.st_size);

  return 0;
}

