/* Generated automatically by the program `genattrtab'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"
#include "insn-config.h"
#include "recog.h"
#include "regs.h"
#include "real.h"
#include "output.h"
#include "insn-attr.h"

#define operands recog_operand

int
insn_current_length (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 0;

    }
}

int
insn_variable_length_p (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 0;

    }
}

int
insn_default_length (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 110:
    case 108:
    case 106:
    case 96:
      insn_extract (insn);
      if (symbolic_memory_operand (operands[1], VOIDmode))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 1)
        {
	  if (symbolic_memory_operand (operands[1], VOIDmode))
	    {
	      return 2;
	    }
	  else
	    {
	      return 1;
	    }
        }
      else if (which_alternative != 0)
        {
	  if (symbolic_memory_operand (operands[0], VOIDmode))
	    {
	      return 2;
	    }
	  else
	    {
	      return 1;
	    }
        }
      else
        {
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 2;
	    }
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 3) || (which_alternative == 2))
        {
	  if (symbolic_memory_operand (operands[1], VOIDmode))
	    {
	      return 2;
	    }
	  else
	    {
	      return 1;
	    }
        }
      else if ((which_alternative != 0) && (which_alternative != 1))
        {
	  if (symbolic_memory_operand (operands[0], VOIDmode))
	    {
	      return 2;
	    }
	  else
	    {
	      return 1;
	    }
        }
      else if (which_alternative == 1)
        {
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 2;
	    }
        }
      else
        {
	  return 1;
        }

    case 89:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 2;
        }
      else if ((which_alternative == 1) || ((which_alternative == 2) || (which_alternative == 3)))
        {
	  return 3;
        }
      else if (which_alternative == 4)
        {
	  return 2;
        }
      else if (which_alternative == 5)
        {
	  return 3;
        }
      else
        {
	  return 3;
        }

    case 85:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return 1;
        }
      else if (which_alternative == 2)
        {
	  return 2;
        }
      else if (which_alternative == 3)
        {
	  return 3;
        }
      else
        {
	  return 3;
        }

    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return 1;
        }
      else if ((which_alternative == 2) || (which_alternative == 3))
        {
	  return 2;
        }
      else if ((which_alternative == 4) || ((which_alternative == 5) || (which_alternative == 6)))
        {
	  return 3;
        }
      else
        {
	  return 3;
        }

    case 79:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return 4;
        }
      else if ((which_alternative == 2) || ((which_alternative == 3) || (which_alternative == 4)))
        {
	  return 5;
        }
      else
        {
	  return 5;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 2;
	    }
        }
      else if (which_alternative == 1)
        {
	  return 1;
        }
      else if (which_alternative == 2)
        {
	  if (symbolic_memory_operand (operands[1], VOIDmode))
	    {
	      return 2;
	    }
	  else
	    {
	      return 1;
	    }
        }
      else
        {
	  return 1;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  if ((which_alternative == 0) || (which_alternative == 2))
	    {
	      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	        {
		  return 1;
	        }
	      else
	        {
		  return 2;
	        }
	    }
	  else
	    {
	      return 1;
	    }
        }
      else if (which_alternative == 2)
        {
	  return 1;
        }
      else
        {
	  if ((which_alternative == 3) || (which_alternative == 4))
	    {
	      if (symbolic_memory_operand (operands[1], VOIDmode))
	        {
		  return 2;
	        }
	      else
	        {
		  return 1;
	        }
	    }
	  else
	    {
	      if (symbolic_memory_operand (operands[0], VOIDmode))
	        {
		  return 2;
	        }
	      else
	        {
		  return 1;
	        }
	    }
        }

    case 270:
    case 269:
    case 268:
    case 261:
    case 260:
    case 259:
    case 258:
    case 257:
    case 256:
    case 255:
    case 254:
    case 253:
    case 252:
    case 251:
    case 250:
    case 249:
    case 248:
    case 247:
    case 227:
    case 208:
    case 207:
    case 203:
    case 168:
    case 166:
    case 164:
    case 162:
    case 158:
    case 155:
    case 152:
    case 149:
    case 146:
    case 133:
    case 132:
    case 131:
    case 129:
    case 127:
    case 125:
    case 111:
    case 41:
    case 40:
    case 38:
      insn_extract (insn);
      if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
        {
	  return 1;
        }
      else
        {
	  return 3;
        }

    case 177:
    case 175:
    case 172:
    case 170:
    case 104:
    case 102:
    case 43:
    case 42:
    case 39:
    case 37:
    case 34:
    case 33:
      insn_extract (insn);
      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
        {
	  return 1;
        }
      else
        {
	  return 2;
        }

    case 232:
      return 8;

    case 267:
    case 266:
    case 265:
    case 264:
    case 263:
    case 262:
    case 226:
    case 225:
    case 223:
    case 231:
    case 211:
    case 206:
    case 174:
    case 169:
    case 161:
    case 157:
    case 154:
    case 151:
    case 148:
    case 145:
    case 139:
    case 138:
    case 136:
    case 135:
    case 128:
    case 124:
    case 94:
    case 76:
    case 72:
    case 68:
    case 61:
    case 30:
    case 29:
    case 28:
    case 27:
    case 26:
    case 25:
    case 24:
    case 23:
      return 2;

    case 210:
    case 205:
    case 60:
      return 4;

    case 202:
    case 143:
    case 142:
    case 81:
    case 77:
      return 5;

    case 196:
    case 193:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 1;
        }
      else
        {
	  return 2;
        }

    case 195:
    case 192:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 1;
        }
      else
        {
	  return 4;
        }

    case 141:
    case 140:
      return 6;

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 2;
        }
      else if (which_alternative == 1)
        {
	  return 1;
        }
      else
        {
	  return 1;
        }

    case 87:
    case 82:
    case 44:
      return 3;

    case 80:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 4;
        }
      else if (which_alternative == 1)
        {
	  return 5;
        }
      else
        {
	  return 5;
        }

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 1;

    }
}

int
result_ready_cost (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 200:
    case 199:
    case 198:
      return 63 /* 0x3f */;

    case 191:
    case 190:
    case 189:
      return 37 /* 0x25 */;

    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
      return 7;

    case 197:
    case 196:
    case 195:
    case 194:
    case 193:
    case 192:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
      return 5;

    case 100:
    case 98:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      return 2;

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 1)
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 5;
        }
      else if ((which_alternative == 3) || (which_alternative == 2))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 89:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 4)
        {
	  return 5;
        }
      else if ((which_alternative == 2) || (which_alternative == 5))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 85:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative != 0) && ((which_alternative != 2) && (which_alternative != 3)))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 2)
        {
	  return 5;
        }
      else if ((which_alternative == 7) || ((which_alternative == 1) || (which_alternative == 6)))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 80:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative != 0) && (which_alternative != 1))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 79:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return 5;
        }
      else if ((which_alternative != 1) && ((which_alternative != 2) && (which_alternative != 3)))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 90:
    case 82:
    case 77:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 2)
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 1)
        {
	  return 5;
        }
      else if ((which_alternative == 3) || (which_alternative == 4))
        {
	  return 2;
        }
      else
        {
	  return 1;
        }

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 1;

    }
}

int
fp_mds_unit_ready_cost (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 191:
    case 190:
    case 189:
      return 37 /* 0x25 */;

    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
      return 7;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 63 /* 0x3f */;

    }
}

unsigned int
fp_mds_unit_blockage_range (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 191:
    case 190:
    case 189:
      return 2424869 /* 0x250025 */;

    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
      return 458759 /* 0x70007 */;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 4128831 /* 0x3f003f */;

    }
}

int
fp_alu_unit_ready_cost (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 5;

    }
}

int
memory_unit_ready_cost (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 2;

    }
}

int
function_units_used (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 200:
    case 199:
    case 198:
    case 191:
    case 190:
    case 189:
    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
      return 2;

    case 197:
    case 196:
    case 195:
    case 194:
    case 193:
    case 192:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
      return 1;

    case 100:
    case 98:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      return 0;

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 1)
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 2) || (which_alternative == 3))
        {
	  return 0;
        }
      else if (which_alternative == 0)
        {
	  return 1;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 1) || (which_alternative == 0))
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 89:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 5) || (which_alternative == 2))
        {
	  return 0;
        }
      else if (which_alternative == 4)
        {
	  return 1;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 85:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative != 0) && ((which_alternative != 2) && (which_alternative != 3)))
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 1) || ((which_alternative == 6) || (which_alternative == 7)))
        {
	  return 0;
        }
      else if (which_alternative == 2)
        {
	  return 1;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 80:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative != 0) && (which_alternative != 1))
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 79:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && (which_alternative != 3))))
        {
	  return 0;
        }
      else if (which_alternative == 0)
        {
	  return 1;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 82:
    case 77:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 2)
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 2)
        {
	  return 0;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 3) || (which_alternative == 4))
        {
	  return 0;
        }
      else if (which_alternative == 1)
        {
	  return 1;
        }
      else
        {
	  return -1 /* 0xffffffff */;
        }

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return -1 /* 0xffffffff */;

    }
}

int
num_delay_slots (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 228:
    case 221:
    case 218:
    case 216:
    case 215:
    case 214:
    case 212:
    case 56:
    case 55:
      return 1;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return 0;

    }
}

enum attr_in_annul_branch_delay
get_attr_in_annul_branch_delay (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 196:
    case 195:
    case 193:
    case 192:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      insn_extract (insn);
      if (! (symbolic_memory_operand (operands[1], VOIDmode)))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 85:
    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 270:
    case 269:
    case 268:
    case 261:
    case 260:
    case 259:
    case 258:
    case 257:
    case 256:
    case 255:
    case 254:
    case 253:
    case 252:
    case 251:
    case 250:
    case 249:
    case 248:
    case 247:
    case 227:
    case 208:
    case 207:
    case 203:
    case 168:
    case 166:
    case 164:
    case 162:
    case 158:
    case 155:
    case 152:
    case 149:
    case 146:
    case 133:
    case 132:
    case 131:
    case 129:
    case 127:
    case 125:
    case 111:
    case 41:
    case 40:
    case 38:
      insn_extract (insn);
      if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 177:
    case 175:
    case 172:
    case 170:
    case 104:
    case 102:
    case 43:
    case 42:
    case 39:
    case 37:
    case 34:
    case 33:
      insn_extract (insn);
      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
        {
	  return IN_ANNUL_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_ANNUL_BRANCH_DELAY_FALSE;
        }

    case 230:
    case 200:
    case 199:
    case 198:
    case 197:
    case 194:
    case 191:
    case 190:
    case 189:
    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 176:
    case 171:
    case 167:
    case 165:
    case 163:
    case 130:
    case 126:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
    case 103:
    case 101:
    case 100:
    case 98:
    case 75:
    case 71:
    case 67:
    case 66:
    case 65:
    case 64:
    case 63:
    case 62:
    case 36:
    case 35:
    case 32:
    case 31:
    case 22:
    case 21:
    case 20:
    case 19:
    case 18:
    case 17:
    case 16:
      return IN_ANNUL_BRANCH_DELAY_TRUE;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return IN_ANNUL_BRANCH_DELAY_FALSE;

    }
}

enum attr_in_uncond_branch_delay
get_attr_in_uncond_branch_delay (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 196:
    case 195:
    case 193:
    case 192:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      insn_extract (insn);
      if (! (symbolic_memory_operand (operands[1], VOIDmode)))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 85:
    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 270:
    case 269:
    case 268:
    case 261:
    case 260:
    case 259:
    case 258:
    case 257:
    case 256:
    case 255:
    case 254:
    case 253:
    case 252:
    case 251:
    case 250:
    case 249:
    case 248:
    case 247:
    case 227:
    case 208:
    case 207:
    case 203:
    case 168:
    case 166:
    case 164:
    case 162:
    case 158:
    case 155:
    case 152:
    case 149:
    case 146:
    case 133:
    case 132:
    case 131:
    case 129:
    case 127:
    case 125:
    case 111:
    case 41:
    case 40:
    case 38:
      insn_extract (insn);
      if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 177:
    case 175:
    case 172:
    case 170:
    case 104:
    case 102:
    case 43:
    case 42:
    case 39:
    case 37:
    case 34:
    case 33:
      insn_extract (insn);
      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
        {
	  return IN_UNCOND_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_UNCOND_BRANCH_DELAY_FALSE;
        }

    case 230:
    case 200:
    case 199:
    case 198:
    case 197:
    case 194:
    case 191:
    case 190:
    case 189:
    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 176:
    case 171:
    case 167:
    case 165:
    case 163:
    case 130:
    case 126:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
    case 103:
    case 101:
    case 100:
    case 98:
    case 75:
    case 71:
    case 67:
    case 66:
    case 65:
    case 64:
    case 63:
    case 62:
    case 36:
    case 35:
    case 32:
    case 31:
    case 22:
    case 21:
    case 20:
    case 19:
    case 18:
    case 17:
    case 16:
      return IN_UNCOND_BRANCH_DELAY_TRUE;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return IN_UNCOND_BRANCH_DELAY_FALSE;

    }
}

enum attr_in_branch_delay
get_attr_in_branch_delay (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 196:
    case 195:
    case 193:
    case 192:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      insn_extract (insn);
      if (! (symbolic_memory_operand (operands[1], VOIDmode)))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 85:
    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 270:
    case 269:
    case 268:
    case 261:
    case 260:
    case 259:
    case 258:
    case 257:
    case 256:
    case 255:
    case 254:
    case 253:
    case 252:
    case 251:
    case 250:
    case 249:
    case 248:
    case 247:
    case 227:
    case 208:
    case 207:
    case 203:
    case 168:
    case 166:
    case 164:
    case 162:
    case 158:
    case 155:
    case 152:
    case 149:
    case 146:
    case 133:
    case 132:
    case 131:
    case 129:
    case 127:
    case 125:
    case 111:
    case 41:
    case 40:
    case 38:
      insn_extract (insn);
      if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 177:
    case 175:
    case 172:
    case 170:
    case 104:
    case 102:
    case 43:
    case 42:
    case 39:
    case 37:
    case 34:
    case 33:
      insn_extract (insn);
      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
        {
	  return IN_BRANCH_DELAY_TRUE;
        }
      else
        {
	  return IN_BRANCH_DELAY_FALSE;
        }

    case 230:
    case 200:
    case 199:
    case 198:
    case 197:
    case 194:
    case 191:
    case 190:
    case 189:
    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 176:
    case 171:
    case 167:
    case 165:
    case 163:
    case 130:
    case 126:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
    case 103:
    case 101:
    case 100:
    case 98:
    case 75:
    case 71:
    case 67:
    case 66:
    case 65:
    case 64:
    case 63:
    case 62:
    case 36:
    case 35:
    case 32:
    case 31:
    case 22:
    case 21:
    case 20:
    case 19:
    case 18:
    case 17:
    case 16:
      return IN_BRANCH_DELAY_TRUE;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return IN_BRANCH_DELAY_FALSE;

    }
}

enum attr_in_call_delay
get_attr_in_call_delay (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 196:
    case 195:
    case 193:
    case 192:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 110:
    case 108:
    case 106:
    case 96:
      insn_extract (insn);
      if (! (symbolic_memory_operand (operands[1], VOIDmode)))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative != 0)
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 85:
    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 270:
    case 269:
    case 268:
    case 261:
    case 260:
    case 259:
    case 258:
    case 257:
    case 256:
    case 255:
    case 254:
    case 253:
    case 252:
    case 251:
    case 250:
    case 249:
    case 248:
    case 247:
    case 227:
    case 208:
    case 207:
    case 203:
    case 168:
    case 166:
    case 164:
    case 162:
    case 158:
    case 155:
    case 152:
    case 149:
    case 146:
    case 133:
    case 132:
    case 131:
    case 129:
    case 127:
    case 125:
    case 111:
    case 41:
    case 40:
    case 38:
      insn_extract (insn);
      if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 177:
    case 175:
    case 172:
    case 170:
    case 104:
    case 102:
    case 43:
    case 42:
    case 39:
    case 37:
    case 34:
    case 33:
      insn_extract (insn);
      if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
        {
	  return IN_CALL_DELAY_TRUE;
        }
      else
        {
	  return IN_CALL_DELAY_FALSE;
        }

    case 230:
    case 200:
    case 199:
    case 198:
    case 197:
    case 194:
    case 191:
    case 190:
    case 189:
    case 188:
    case 187:
    case 186:
    case 185:
    case 184:
    case 183:
    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 176:
    case 171:
    case 167:
    case 165:
    case 163:
    case 130:
    case 126:
    case 123:
    case 122:
    case 121:
    case 120:
    case 119:
    case 118:
    case 117:
    case 116:
    case 115:
    case 114:
    case 113:
    case 112:
    case 103:
    case 101:
    case 100:
    case 98:
    case 75:
    case 71:
    case 67:
    case 66:
    case 65:
    case 64:
    case 63:
    case 62:
    case 36:
    case 35:
    case 32:
    case 31:
    case 22:
    case 21:
    case 20:
    case 19:
    case 18:
    case 17:
    case 16:
      return IN_CALL_DELAY_TRUE;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return IN_CALL_DELAY_FALSE;

    }
}

enum attr_type
get_attr_type (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case 74:
    case 70:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if ((which_alternative == 0) || (which_alternative == 1))
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_LOAD;
        }
      else
        {
	  return TYPE_STORE;
        }

    case 59:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_FP;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_MOVE;
        }
      else if ((which_alternative == 3) || (which_alternative == 4))
        {
	  return TYPE_LOAD;
        }
      else if (which_alternative == 5)
        {
	  return TYPE_STORE;
        }
      else
        {
	  return TYPE_STORE;
        }

    case 79:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_FP;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_FPSTORE;
        }
      else if (which_alternative == 3)
        {
	  return TYPE_STORE;
        }
      else if (which_alternative == 4)
        {
	  return TYPE_FPLOAD;
        }
      else
        {
	  return TYPE_LOAD;
        }

    case 80:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_STORE;
        }
      else
        {
	  return TYPE_LOAD;
        }

    case 84:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_FPSTORE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_FPLOAD;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_FP;
        }
      else if (which_alternative == 3)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 4)
        {
	  return TYPE_FPSTORE;
        }
      else if (which_alternative == 5)
        {
	  return TYPE_STORE;
        }
      else if (which_alternative == 6)
        {
	  return TYPE_FPLOAD;
        }
      else
        {
	  return TYPE_LOAD;
        }

    case 85:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_STORE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_LOAD;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 3)
        {
	  return TYPE_STORE;
        }
      else
        {
	  return TYPE_LOAD;
        }

    case 89:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_STORE;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_LOAD;
        }
      else if (which_alternative == 3)
        {
	  return TYPE_MULTI;
        }
      else if (which_alternative == 4)
        {
	  return TYPE_FP;
        }
      else if (which_alternative == 5)
        {
	  return TYPE_FPLOAD;
        }
      else
        {
	  return TYPE_FPSTORE;
        }

    case 77:
    case 82:
    case 90:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_LOAD;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_FPLOAD;
        }
      else
        {
	  return TYPE_STORE;
        }

    case 92:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_FP;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 2)
        {
	  return TYPE_FPLOAD;
        }
      else if (which_alternative == 3)
        {
	  return TYPE_LOAD;
        }
      else if (which_alternative == 4)
        {
	  return TYPE_FPSTORE;
        }
      else
        {
	  return TYPE_STORE;
        }

    case 93:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_MOVE;
        }
      else if (which_alternative == 1)
        {
	  return TYPE_LOAD;
        }
      else
        {
	  return TYPE_STORE;
        }

    case 98:
    case 100:
      insn_extract (insn);
      if (! constrain_operands (INSN_CODE (insn), reload_completed))
        fatal_insn_not_found (insn);
      if (which_alternative == 0)
        {
	  return TYPE_UNARY;
        }
      else
        {
	  return TYPE_LOAD;
        }

    case 31:
    case 32:
    case 35:
    case 36:
    case 230:
    case 231:
      return TYPE_MISC;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    case 44:
    case 60:
    case 202:
    case 205:
    case 206:
    case 210:
    case 211:
    case 223:
    case 225:
    case 226:
    case 232:
    case 262:
    case 263:
    case 264:
    case 265:
    case 266:
    case 267:
      return TYPE_MULTI;

    case 198:
    case 199:
    case 200:
      return TYPE_FPSQRT;

    case 189:
    case 190:
    case 191:
      return TYPE_FPDIV;

    case 184:
    case 185:
    case 186:
    case 187:
    case 188:
      return TYPE_FPMUL;

    case 17:
    case 18:
    case 19:
    case 20:
    case 21:
    case 22:
      return TYPE_FPCMP;

    case 112:
    case 113:
    case 114:
    case 115:
    case 116:
    case 117:
    case 118:
    case 119:
    case 120:
    case 121:
    case 122:
    case 123:
    case 178:
    case 179:
    case 180:
    case 181:
    case 182:
    case 183:
    case 192:
    case 193:
    case 194:
    case 195:
    case 196:
    case 197:
      return TYPE_FP;

    case 219:
      return TYPE_CALL_NO_DELAY_SLOT;

    case 218:
    case 221:
      return TYPE_CALL;

    case 55:
    case 56:
      return TYPE_BRANCH;

    case 212:
    case 214:
    case 215:
    case 216:
    case 228:
      return TYPE_UNCOND_BRANCH;

    case 68:
    case 72:
    case 76:
    case 81:
    case 87:
    case 94:
      return TYPE_STORE;

    case 96:
    case 106:
    case 108:
    case 110:
      return TYPE_LOAD;

    case 16:
    case 101:
    case 103:
    case 126:
    case 130:
    case 163:
    case 165:
    case 167:
    case 171:
    case 176:
      return TYPE_COMPARE;

    case 23:
    case 24:
    case 25:
    case 26:
    case 33:
    case 34:
    case 37:
    case 39:
    case 42:
    case 43:
    case 102:
    case 104:
    case 169:
    case 170:
    case 172:
    case 174:
    case 175:
    case 177:
      return TYPE_UNARY;

    case 61:
    case 62:
    case 63:
    case 64:
      return TYPE_MOVE;

    default:
      return TYPE_BINARY;

    }
}

enum attr_use_clobbered
get_attr_use_clobbered (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      return USE_CLOBBERED_FALSE;

    }
}

int
eligible_for_delay (delay_insn, slot, candidate_insn, flags)
     rtx delay_insn;
     int slot;
     rtx candidate_insn;
     int flags;
{
  rtx insn;

  if (slot >= 1)
    abort ();

  insn = delay_insn;
  switch (recog_memoized (insn))
    {
    case 221:
    case 218:
      slot += 1 * 1;
      break;
      break;

    case 228:
    case 216:
    case 215:
    case 214:
    case 212:
      slot += 3 * 1;
      break;
      break;

    case 56:
    case 55:
      slot += 2 * 1;
      break;
      break;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      slot += 0 * 1;
      break;
      break;

    }

  if (slot < 1)
    abort ();

  insn = candidate_insn;
  switch (slot)
    {
    case 3:
      switch (recog_memoized (insn))
	{
        case 196:
        case 195:
        case 193:
        case 192:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative == 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 110:
        case 108:
        case 106:
        case 96:
	  insn_extract (insn);
	  if (! (symbolic_memory_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 93:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 92:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 90:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative != 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 85:
        case 84:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((which_alternative == 0) || (which_alternative == 1))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 74:
        case 70:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 59:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 270:
        case 269:
        case 268:
        case 261:
        case 260:
        case 259:
        case 258:
        case 257:
        case 256:
        case 255:
        case 254:
        case 253:
        case 252:
        case 251:
        case 250:
        case 249:
        case 248:
        case 247:
        case 227:
        case 208:
        case 207:
        case 203:
        case 168:
        case 166:
        case 164:
        case 162:
        case 158:
        case 155:
        case 152:
        case 149:
        case 146:
        case 133:
        case 132:
        case 131:
        case 129:
        case 127:
        case 125:
        case 111:
        case 41:
        case 40:
        case 38:
	  insn_extract (insn);
	  if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 177:
        case 175:
        case 172:
        case 170:
        case 104:
        case 102:
        case 43:
        case 42:
        case 39:
        case 37:
        case 34:
        case 33:
	  insn_extract (insn);
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 230:
        case 200:
        case 199:
        case 198:
        case 197:
        case 194:
        case 191:
        case 190:
        case 189:
        case 188:
        case 187:
        case 186:
        case 185:
        case 184:
        case 183:
        case 182:
        case 181:
        case 180:
        case 179:
        case 178:
        case 176:
        case 171:
        case 167:
        case 165:
        case 163:
        case 130:
        case 126:
        case 123:
        case 122:
        case 121:
        case 120:
        case 119:
        case 118:
        case 117:
        case 116:
        case 115:
        case 114:
        case 113:
        case 112:
        case 103:
        case 101:
        case 100:
        case 98:
        case 75:
        case 71:
        case 67:
        case 66:
        case 65:
        case 64:
        case 63:
        case 62:
        case 36:
        case 35:
        case 32:
        case 31:
        case 22:
        case 21:
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
	  return 1;

        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    case 2:
      switch (recog_memoized (insn))
	{
        case 196:
        case 195:
        case 193:
        case 192:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative == 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 110:
        case 108:
        case 106:
        case 96:
	  insn_extract (insn);
	  if (! (symbolic_memory_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 93:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 92:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 90:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative != 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 85:
        case 84:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((which_alternative == 0) || (which_alternative == 1))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 74:
        case 70:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 59:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 270:
        case 269:
        case 268:
        case 261:
        case 260:
        case 259:
        case 258:
        case 257:
        case 256:
        case 255:
        case 254:
        case 253:
        case 252:
        case 251:
        case 250:
        case 249:
        case 248:
        case 247:
        case 227:
        case 208:
        case 207:
        case 203:
        case 168:
        case 166:
        case 164:
        case 162:
        case 158:
        case 155:
        case 152:
        case 149:
        case 146:
        case 133:
        case 132:
        case 131:
        case 129:
        case 127:
        case 125:
        case 111:
        case 41:
        case 40:
        case 38:
	  insn_extract (insn);
	  if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 177:
        case 175:
        case 172:
        case 170:
        case 104:
        case 102:
        case 43:
        case 42:
        case 39:
        case 37:
        case 34:
        case 33:
	  insn_extract (insn);
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 230:
        case 200:
        case 199:
        case 198:
        case 197:
        case 194:
        case 191:
        case 190:
        case 189:
        case 188:
        case 187:
        case 186:
        case 185:
        case 184:
        case 183:
        case 182:
        case 181:
        case 180:
        case 179:
        case 178:
        case 176:
        case 171:
        case 167:
        case 165:
        case 163:
        case 130:
        case 126:
        case 123:
        case 122:
        case 121:
        case 120:
        case 119:
        case 118:
        case 117:
        case 116:
        case 115:
        case 114:
        case 113:
        case 112:
        case 103:
        case 101:
        case 100:
        case 98:
        case 75:
        case 71:
        case 67:
        case 66:
        case 65:
        case 64:
        case 63:
        case 62:
        case 36:
        case 35:
        case 32:
        case 31:
        case 22:
        case 21:
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
	  return 1;

        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    case 1:
      switch (recog_memoized (insn))
	{
        case 196:
        case 195:
        case 193:
        case 192:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative == 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 110:
        case 108:
        case 106:
        case 96:
	  insn_extract (insn);
	  if (! (symbolic_memory_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 93:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 92:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative != 0) && ((which_alternative != 1) && ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && (((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode))))) || ((which_alternative == 2) || (which_alternative == 3))))))) || (((which_alternative == 0) || (which_alternative == 1)) && ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 90:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative != 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 85:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((which_alternative == 1) || (which_alternative == 0))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 84:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((which_alternative == 0) || (which_alternative == 1))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 74:
        case 70:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative != 0) && ((which_alternative != 1) && ((! (symbolic_memory_operand (operands[1], VOIDmode))) || (which_alternative != 2)))) || (((which_alternative == 0) || (which_alternative == 1)) && (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 59:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((((which_alternative == 3) || (which_alternative == 4)) && ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))))) || ((((which_alternative == 5) || (which_alternative == 6)) && ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || (((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 4)) && ((which_alternative != 5) && (which_alternative != 6))) && (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))))))))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 270:
        case 269:
        case 268:
        case 261:
        case 260:
        case 259:
        case 258:
        case 257:
        case 256:
        case 255:
        case 254:
        case 253:
        case 252:
        case 251:
        case 250:
        case 249:
        case 248:
        case 247:
        case 227:
        case 208:
        case 207:
        case 203:
        case 168:
        case 166:
        case 164:
        case 162:
        case 158:
        case 155:
        case 152:
        case 149:
        case 146:
        case 133:
        case 132:
        case 131:
        case 129:
        case 127:
        case 125:
        case 111:
        case 41:
        case 40:
        case 38:
	  insn_extract (insn);
	  if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 177:
        case 175:
        case 172:
        case 170:
        case 104:
        case 102:
        case 43:
        case 42:
        case 39:
        case 37:
        case 34:
        case 33:
	  insn_extract (insn);
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 230:
        case 200:
        case 199:
        case 198:
        case 197:
        case 194:
        case 191:
        case 190:
        case 189:
        case 188:
        case 187:
        case 186:
        case 185:
        case 184:
        case 183:
        case 182:
        case 181:
        case 180:
        case 179:
        case 178:
        case 176:
        case 171:
        case 167:
        case 165:
        case 163:
        case 130:
        case 126:
        case 123:
        case 122:
        case 121:
        case 120:
        case 119:
        case 118:
        case 117:
        case 116:
        case 115:
        case 114:
        case 113:
        case 112:
        case 103:
        case 101:
        case 100:
        case 98:
        case 75:
        case 71:
        case 67:
        case 66:
        case 65:
        case 64:
        case 63:
        case 62:
        case 36:
        case 35:
        case 32:
        case 31:
        case 22:
        case 21:
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
	  return 1;

        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    default:
      abort ();
    }
}

int
eligible_for_annul_false (delay_insn, slot, candidate_insn, flags)
     rtx delay_insn;
     int slot;
     rtx candidate_insn;
     int flags;
{
  rtx insn;

  if (slot >= 1)
    abort ();

  insn = delay_insn;
  switch (recog_memoized (insn))
    {
    case 221:
    case 218:
      slot += 1 * 1;
      break;
      break;

    case 228:
    case 216:
    case 215:
    case 214:
    case 212:
      slot += 3 * 1;
      break;
      break;

    case 56:
    case 55:
      slot += 2 * 1;
      break;
      break;

    case -1:
      if (GET_CODE (PATTERN (insn)) != ASM_INPUT
          && asm_noperands (PATTERN (insn)) < 0)
        fatal_insn_not_found (insn);
    default:
      slot += 0 * 1;
      break;
      break;

    }

  if (slot < 1)
    abort ();

  insn = candidate_insn;
  switch (slot)
    {
    case 3:
      switch (recog_memoized (insn))
	{
        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    case 2:
      switch (recog_memoized (insn))
	{
        case 196:
        case 195:
        case 193:
        case 192:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative == 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 110:
        case 108:
        case 106:
        case 96:
	  insn_extract (insn);
	  if (! (symbolic_memory_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 93:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 1) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 92:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((((which_alternative == 3) || (which_alternative == 2)) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative != 0) && ((which_alternative != 1) && ((which_alternative != 2) && ((which_alternative != 3) && (! (symbolic_memory_operand (operands[0], VOIDmode)))))))) || (((which_alternative == 1) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative != 3) && (which_alternative != 2)) && ((which_alternative == 0) || ((which_alternative == 2) || (which_alternative == 3)))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 90:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (which_alternative != 0)
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 85:
        case 84:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if ((which_alternative == 0) || (which_alternative == 1))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 74:
        case 70:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || (((which_alternative == 2) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || ((which_alternative != 0) && (which_alternative != 2))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 59:
	  insn_extract (insn);
	  if (! constrain_operands (INSN_CODE (insn), reload_completed))
	    fatal_insn_not_found (insn);
	  if (((which_alternative == 0) && ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))) || ((which_alternative == 1) || ((which_alternative == 2) || (((which_alternative == 3) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 4) && (! (symbolic_memory_operand (operands[1], VOIDmode)))) || (((which_alternative == 5) && (! (symbolic_memory_operand (operands[0], VOIDmode)))) || ((which_alternative == 6) && (! (symbolic_memory_operand (operands[0], VOIDmode))))))))))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 270:
        case 269:
        case 268:
        case 261:
        case 260:
        case 259:
        case 258:
        case 257:
        case 256:
        case 255:
        case 254:
        case 253:
        case 252:
        case 251:
        case 250:
        case 249:
        case 248:
        case 247:
        case 227:
        case 208:
        case 207:
        case 203:
        case 168:
        case 166:
        case 164:
        case 162:
        case 158:
        case 155:
        case 152:
        case 149:
        case 146:
        case 133:
        case 132:
        case 131:
        case 129:
        case 127:
        case 125:
        case 111:
        case 41:
        case 40:
        case 38:
	  insn_extract (insn);
	  if ((arith_operand (operands[2], VOIDmode)) || (arith_double_operand (operands[2], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 177:
        case 175:
        case 172:
        case 170:
        case 104:
        case 102:
        case 43:
        case 42:
        case 39:
        case 37:
        case 34:
        case 33:
	  insn_extract (insn);
	  if ((arith_operand (operands[1], VOIDmode)) || (arith_double_operand (operands[1], VOIDmode)))
	    {
	      return 1;
	    }
	  else
	    {
	      return 0;
	    }

        case 230:
        case 200:
        case 199:
        case 198:
        case 197:
        case 194:
        case 191:
        case 190:
        case 189:
        case 188:
        case 187:
        case 186:
        case 185:
        case 184:
        case 183:
        case 182:
        case 181:
        case 180:
        case 179:
        case 178:
        case 176:
        case 171:
        case 167:
        case 165:
        case 163:
        case 130:
        case 126:
        case 123:
        case 122:
        case 121:
        case 120:
        case 119:
        case 118:
        case 117:
        case 116:
        case 115:
        case 114:
        case 113:
        case 112:
        case 103:
        case 101:
        case 100:
        case 98:
        case 75:
        case 71:
        case 67:
        case 66:
        case 65:
        case 64:
        case 63:
        case 62:
        case 36:
        case 35:
        case 32:
        case 31:
        case 22:
        case 21:
        case 20:
        case 19:
        case 18:
        case 17:
        case 16:
	  return 1;

        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    case 1:
      switch (recog_memoized (insn))
	{
        case -1:
	  if (GET_CODE (PATTERN (insn)) != ASM_INPUT
	      && asm_noperands (PATTERN (insn)) < 0)
	    fatal_insn_not_found (insn);
        default:
	  return 0;

      }
    default:
      abort ();
    }
}

struct function_unit_desc function_units[] = {
  {"memory", 1, 1, 1, 1, 1, memory_unit_ready_cost, 0, 2, 0, 0}, 
  {"fp_alu", 2, 1, 1, 1, 1, fp_alu_unit_ready_cost, 0, 5, 0, 0}, 
  {"fp_mds", 4, 1, 1, 1, 1, fp_mds_unit_ready_cost, 0, 63, fp_mds_unit_blockage_range, 0}, 
};

int
const_num_delay_slots (insn)
     rtx insn;
{
  switch (recog_memoized (insn))
    {
    default:
      return 1;
    }
}
