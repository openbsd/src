/* Generated automatically by the program `genextract'
from the machine description file `md'.  */

#include "config.h"
#include "rtl.h"

static rtx junk;
extern rtx recog_operand[];
extern rtx *recog_operand_loc[];
extern rtx *recog_dup_loc[];
extern char recog_dup_num[];
extern
#ifdef __GNUC__
__volatile__
#endif
void fatal_insn_not_found ();

void
insn_extract (insn)
     rtx insn;
{
  register rtx *ro = recog_operand;
  register rtx **ro_loc = recog_operand_loc;
  rtx pat = PATTERN (insn);
  switch (INSN_CODE (insn))
    {
    case -1:
      fatal_insn_not_found (insn);

    case 236:
    case 235:
    case 234:
    case 233:
    case 232:
    case 231:
#if __GNUC__ > 1 && !defined (bcopy)
#define bcopy(FROM,TO,COUNT) __builtin_memcpy(TO,FROM,COUNT)
#endif
      bcopy (&XVECEXP (pat, 0, 0), ro,
             sizeof (rtx) * XVECLEN (pat, 0));
      break;

    case 227:
    case 226:
    case 225:
    case 224:
    case 223:
    case 222:
    case 221:
    case 220:
    case 219:
    case 218:
    case 217:
    case 216:
    case 215:
    case 214:
    case 213:
    case 212:
    case 211:
    case 210:
    case 209:
    case 208:
    case 207:
    case 206:
    case 205:
    case 204:
    case 203:
    case 202:
    case 201:
    case 200:
    case 199:
    case 198:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      break;

    case 197:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 0), 0));
      break;

    case 195:
    case 189:
    case 188:
      break;

    case 184:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 1));
      recog_dup_loc[0] = &XEXP (XVECEXP (pat, 0, 1), 0);
      recog_dup_num[0] = 0;
      recog_dup_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
      recog_dup_num[1] = 0;
      break;

    case 183:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0));
      recog_dup_loc[0] = &XEXP (XVECEXP (pat, 0, 1), 0);
      recog_dup_num[0] = 0;
      recog_dup_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
      recog_dup_num[1] = 0;
      recog_dup_loc[2] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 1);
      recog_dup_num[2] = 1;
      break;

    case 182:
    case 181:
    case 180:
    case 179:
    case 178:
    case 177:
    case 176:
    case 175:
    case 174:
    case 173:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (pat, 1), 2), 0));
      break;

    case 172:
    case 171:
    case 170:
    case 169:
    case 168:
    case 167:
    case 166:
    case 165:
    case 164:
    case 163:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 162:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      break;

    case 161:
    case 160:
    case 159:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 0), 2));
      ro[3] = *(ro_loc[3] = &XEXP (pat, 1));
      break;

    case 158:
    case 157:
    case 156:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (pat, 1), 2));
      break;

    case 155:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 2));
      break;

    case 154:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XEXP (pat, 1), 0), 0), 1));
      recog_dup_loc[0] = &XEXP (XEXP (pat, 1), 1);
      recog_dup_num[0] = 0;
      break;

    case 153:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 1));
      recog_dup_loc[0] = &XEXP (XEXP (pat, 1), 1);
      recog_dup_num[0] = 0;
      break;

    case 152:
    case 151:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 0), 2));
      break;

    case 149:
    case 146:
    case 143:
    case 137:
    case 134:
    case 131:
    case 125:
    case 122:
    case 119:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 99:
    case 98:
    case 97:
    case 93:
    case 92:
    case 91:
    case 87:
    case 86:
    case 85:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 79:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 73:
    case 71:
    case 65:
    case 63:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 60:
    case 59:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 68:
    case 58:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 186:
    case 148:
    case 145:
    case 142:
    case 140:
    case 139:
    case 138:
    case 136:
    case 133:
    case 130:
    case 128:
    case 127:
    case 126:
    case 124:
    case 121:
    case 118:
    case 116:
    case 115:
    case 114:
    case 105:
    case 104:
    case 103:
    case 102:
    case 101:
    case 100:
    case 96:
    case 95:
    case 94:
    case 90:
    case 89:
    case 88:
    case 84:
    case 83:
    case 82:
    case 81:
    case 80:
    case 78:
    case 77:
    case 76:
    case 75:
    case 74:
    case 72:
    case 70:
    case 69:
    case 67:
    case 66:
    case 64:
    case 62:
    case 61:
    case 57:
    case 56:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 49:
    case 48:
    case 47:
    case 46:
    case 45:
    case 44:
    case 43:
    case 42:
    case 41:
    case 40:
    case 39:
    case 38:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      break;

    case 230:
    case 229:
    case 228:
    case 194:
    case 193:
    case 192:
    case 191:
    case 190:
    case 113:
    case 112:
    case 111:
    case 110:
    case 109:
    case 108:
    case 107:
    case 106:
    case 55:
    case 54:
    case 53:
    case 52:
    case 51:
    case 50:
    case 37:
    case 36:
    case 35:
    case 34:
    case 33:
    case 32:
    case 31:
    case 30:
    case 29:
    case 28:
    case 27:
    case 26:
    case 25:
    case 24:
    case 23:
    case 22:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      break;

    case 21:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XVECEXP (pat, 0, 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XVECEXP (pat, 0, 1), 0));
      break;

    case 19:
    case 17:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (pat, 1));
      break;

    case 185:
    case 150:
    case 18:
    case 16:
    case 15:
    case 13:
    case 12:
    case 11:
    case 10:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (pat, 1));
      break;

    case 9:
    case 8:
    case 7:
    case 6:
    case 5:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 196:
    case 14:
    case 4:
    case 3:
    case 2:
    case 1:
    case 0:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 1));
      break;

    default:
      abort ();
    }
}
