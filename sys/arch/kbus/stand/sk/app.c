#define _KERNEL
#include <sys/param.h>
#include <machine/psl.h>

extern unsigned int hz_ticks;

#define SPL_BODY(new_spl)			\
{						\
   int oldspl;					\
   						\
   /* Get old spl.  */				\
   oldspl = lda (ASI_IPR, 0) & 0xff;		\
						\
   setpil15();					\
   sta (ASI_IPR, 0, new_spl & 0xff);		\
   setpil0();					\
						\
   return oldspl;				\
}

int
spl0 (void)
SPL_BODY (0)

int
spl1 (void)
SPL_BODY (140)

int
get_hz (void)
{
  int sec;
  int ticks;
  
  sec = getsecs () + 1;
  while (getsecs () < sec)
    ;
  ticks = hz_ticks;
  setpil0 ();
  while (getsecs () < sec + 8)
    ;
  setpil15 ();
  return (hz_ticks - ticks) / 8;
}

void
do_cmd (char *line)
{
  char *argv[40];
  int argc;
  int i, j;

  for (argc = 0, i = 0; ; argc++)
    {
      while (line[i] == ' ')
	line[i++] = 0;
      if (line[i] == 0)
	break;
      argv[argc] = line + i;
      while (line[i] && line[i] != ' ')
	i++;
    }
#if 0
  printf ("argc = %d, argv = ", argc);
  for (i = 0; i < argc; i++)
    printf ("%s ", argv[i]);
  printf ("\n");
#endif

  if (argc == 0)
    return;
  if (strcmp (argv[0], "quit") == 0)
    _exit (0);
  else if (strcmp (argv[0], "le") == 0)
    le_disp_status ();
  else if (strcmp (argv[0], "ipr?") == 0)
    printf ("ipr = %d, pil = %d\n",
	    lda (ASI_IPR, 0) & 0xff,
	    (getpsr() & PSR_PIL) >> 8);
  else if (strcmp (argv[0], "pil0") == 0)
    setpil0 ();
  else if (strcmp (argv[0], "pil15") == 0)
    setpil15 ();
  else if (strcmp (argv[0], "spl0") == 0)
    spl0 ();
  else if (strcmp (argv[0], "spl1") == 0)
    spl1 ();
  else if (strcmp (argv[0], "le_intr") == 0)
    le_intr ();
  else if (strcmp (argv[0], "zs_intr") == 0)
    zs_intr ();
  else if (strcmp (argv[0], "sic?") == 0)
    printf ("DIR: %x, IPR: %x, IRC: %x, IXR: %x, IXC: %x\n",
	    lduba (ASI_DIR, 0) & SIC_DIR_MASK,
	    lda (ASI_IPR, 0) & SIC_IPR_MASK,
	    lduba (ASI_IRXC, 0) & 0x3,
	    lda (ASI_IXR, 0) & 0xffff,
	    lduba (ASI_ITXC, 0) & 0x3);
  else if (strcmp (argv[0], "clk?") == 0)
    printf ("clk: %d\n", hz_ticks);
  else if (strcmp (argv[0], "init_kbd") == 0)
    init_kbd ();
  else if (strcmp (argv[0], "irxc0") == 0) 
    stba (ASI_IRXC, 0, 0);
  else if (strcmp (argv[0], "irxc1") == 0) 
    stba (ASI_IRXC, 0, 1);
  else if (strcmp (argv[0], "hz?") == 0)
    printf ("hz = %d\n", get_hz ());
  else
    printf ("Unknown command\n");
}
