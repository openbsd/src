#include <dev/ic/z8530reg.h>
#include <machine/prom.h>
#include <machine/sic.h>
#include <machine/asi.h>
#include <machine/asm.h>

#define NOFAULT_CONTINUE	0x01
#define NOFAULT_QUIET		0x02

extern int nofault;

#define CALL_ROM_COMMAND (**(void (**)(void))ROM_COMMAND)()
#define GET_ROM_COMMAREA (*(struct prom_command_area **)ROM_COMM_AREA)

void
_exit (int val)
{
  char *command = "reset intr";
 
  GET_ROM_COMMAREA->command_ptr = command;
  CALL_ROM_COMMAND;
}

void
sic_init (void)
{
  unsigned char irc_status;
  unsigned char id;
  unsigned char v;

  set_ipl (0x0f);

  printf ("1");  
  /* Disable the receiver.  */
  irc_status = lduba (ASI_IRXC, 0);
  while (irc_status & SIC_IRC_E)
    {
      stba (ASI_IRXC, 0, irc_status & ~SIC_IRC_E);
      irc_status = lduba (ASI_IRXC, 0);
      printf ("2");  
    }

  /* Enable all interruptions.  */
  sta (ASI_IPR, 0, 0);
  
  printf ("3");  

  /* Set device id.  */
  id = lda (ASI_BID, 0) & 0x0f;
  stba (ASI_DIR, 0, id);

  printf ("4");  
  /* Ack int.  */
  lda (ASI_ACK_IPV, 0);

  printf ("5");  
  /* Enable the receiver.  */
  stba (ASI_IRXC, 0, irc_status | SIC_IRC_E);

  printf ("6");  
  set_ipl (0);
  *(unsigned long *)0x17030000 = 0; /* id | 0x40;  */

  printf ("7");  
  /* Enable interruptions from the system board.  */
  v = *(volatile char *)0x17031000; 
  putchar ('8');
  printf ("9\n");  
}

void
sendint (int boardid, int level)
{
  printf ("sendint level %d: ", level);
  if (lduba (ASI_ITXC, 0) & SIC_ITXC_E)
    {
      printf ("busy\n");
      return; 
    }
  else
    printf ("not busy...\n");
  sta (ASI_IXR, 0, ((level & 0xff) << 8) | SIC_IXR_DIR | (boardid & 0x0f));
  stba (ASI_ITXC, 0, SIC_ITXC_E);
}

#define NUMREGS 80 

/* Number of bytes of registers.  */
#define NUMREGBYTES (NUMREGS * 4)
enum regnames
{
  G0, G1, G2, G3, G4, G5, G6, G7,
  O0, O1, O2, O3, O4, O5, SP, O7,
  L0, L1, L2, L3, L4, L5, L6, L7,
  I0, I1, I2, I3, I4, I5, FP, I7,
  
  F0, F1, F2, F3, F4, F5, F6, F7,
  F8, F9, F10, F11, F12, F13, F14, F15,
  F16, F17, F18, F19, F20, F21, F22, F23,
  F24, F25, F26, F27, F28, F29, F30, F31,
  Y, PSR, WIM, TBR, PC, NPC, FPSR, CPSR,
  MMCR, FCR, FVAR, PDBA, FPAR, FTOR, FES, FTSR}; /* MMCR at 72.  */


extern int in_trap_handler;
unsigned int hz_ticks;

void
handle_exception (unsigned long *registers)
{
  int trap = (registers[TBR] >> 4) & 0xff;

  if (trap != 9 && trap != 28 && !(nofault & NOFAULT_QUIET))
    {
      printf ("Trap %d: (in: %d)\n"
	      "psr = 0x%x, tbr = 0x%x, pc = 0x%x, npc = 0x%x\n",
	      trap, in_trap_handler,
	      registers[PSR], registers[TBR], registers[PC], registers[NPC]);
      printf ("sp = %x fp = %x\n",
	      registers[SP], registers[FP]);
      printf ("MMCR = %x, FCR = %x, FVAR = %x, PDBA = %x\n",
	      registers[MMCR], registers[FCR],
	      registers[FVAR], registers[PDBA]);
      printf ("FPAR = %x, FTOR = %x, FES = %x, FTSR = %x\n",
	      registers[FPAR], registers[FTOR],
	      registers[FES], registers[FTSR]);
      printf ("DIR: %x, IPR: %x, IRC: %x, IXR: %x, IXC: %x\n",
	      lduba (ASI_DIR, 0) & SIC_DIR_MASK,
	      lda (ASI_IPR, 0) & SIC_IPR_MASK,
	      lduba (ASI_IRXC, 0) & 0x3,
	      lda (ASI_IXR, 0) & 0xffff,
	      lduba (ASI_ITXC, 0) & 0x3);
    }
  
  if (trap == 9 && (nofault & NOFAULT_CONTINUE))
    {
      nofault = 0;
      registers[PC] = registers[NPC];
      registers[NPC] += 4;
    }
  else if (trap == 28)
    {
      int ipv;

      ipv = lda (ASI_ACK_IPV, 0) & 0xffff;

#if 1
      if ((ipv & SIC_IPV_IVL) >> 8 != 141)
	printf ("interrupt: vector: %d, info: %x, IRXC: %x\n",
		(ipv & SIC_IPV_IVL) >> 8,
		ipv & 0xff,
		lduba (ASI_IRXC, 0) & 0x3);
#endif
      stba (ASI_IRXC, 0, SIC_IRC_E);

      switch ((ipv & SIC_IPV_IVL) >> 8)
	{
	case 137:
	  zs_intr ();
	  break;
	case 135:
	  le_intr ();
	  break;
	case 141:
	  hz_ticks++;
	  break;
	default:
	  printf ("No handler for %d\n", ipv >> 8);
	}
    }
  else
    _exit (1);
}
