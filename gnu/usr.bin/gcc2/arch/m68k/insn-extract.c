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

    case 305:
    case 304:
    case 303:
    case 302:
    case 301:
    case 300:
    case 299:
#if __GNUC__ > 1 && !defined (bcopy)
#define bcopy(FROM,TO,COUNT) __builtin_memcpy(TO,FROM,COUNT)
#endif
      bcopy (&XVECEXP (pat, 0, 0), ro,
             sizeof (rtx) * XVECLEN (pat, 0));
      break;

    case 317:
    case 315:
    case 309:
    case 308:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (XEXP (pat, 1), 1), 1));
      break;

    case 316:
    case 314:
    case 313:
    case 312:
    case 311:
    case 310:
    case 307:
    case 306:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 0), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 296:
    case 295:
    case 294:
    case 293:
      break;

    case 285:
    case 284:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0));
      recog_dup_loc[0] = &XEXP (XVECEXP (pat, 0, 1), 0);
      recog_dup_num[0] = 0;
      recog_dup_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
      recog_dup_num[1] = 0;
      break;

    case 283:
    case 282:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1), 0));
      recog_dup_loc[0] = &XEXP (XVECEXP (pat, 0, 1), 0);
      recog_dup_num[0] = 0;
      recog_dup_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
      recog_dup_num[1] = 0;
      break;

    case 281:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 0), 0));
      break;

    case 280:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 1));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 0), 0));
      break;

    case 278:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      break;

    case 277:
    case 276:
    case 275:
    case 274:
    case 273:
    case 272:
    case 271:
    case 270:
    case 269:
    case 268:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (pat, 1), 2), 0));
      break;

    case 267:
    case 266:
    case 265:
    case 264:
    case 263:
    case 262:
    case 261:
    case 260:
    case 259:
    case 258:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 247:
    case 246:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 2));
      break;

    case 244:
    case 243:
    case 239:
    case 238:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 0), 2));
      break;

    case 237:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 0), 2));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (pat, 1), 1));
      recog_dup_loc[0] = &XEXP (XEXP (XEXP (pat, 1), 0), 0);
      recog_dup_num[0] = 0;
      recog_dup_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 1);
      recog_dup_num[1] = 1;
      recog_dup_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 0), 2);
      recog_dup_num[2] = 2;
      break;

    case 242:
    case 241:
    case 236:
    case 235:
    case 234:
    case 233:
    case 232:
    case 231:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XEXP (pat, 1), 2));
      break;

    case 245:
    case 240:
    case 230:
    case 229:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 0), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 0), 2));
      ro[3] = *(ro_loc[3] = &XEXP (pat, 1));
      break;

    case 190:
    case 188:
    case 172:
    case 170:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      recog_dup_loc[0] = &XEXP (XEXP (pat, 1), 0);
      recog_dup_num[0] = 0;
      break;

    case 146:
    case 145:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XVECEXP (pat, 0, 1), 0));
      recog_dup_loc[0] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0);
      recog_dup_num[0] = 1;
      recog_dup_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 1);
      recog_dup_num[1] = 2;
      break;

    case 144:
    case 141:
    case 132:
    case 129:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 0), 1));
      break;

    case 143:
    case 140:
    case 131:
    case 128:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (XEXP (pat, 1), 0), 1), 0));
      break;

    case 120:
    case 117:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XVECEXP (pat, 0, 1), 0));
      recog_dup_loc[0] = &XEXP (XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0), 0), 0), 0);
      recog_dup_num[0] = 1;
      recog_dup_loc[1] = &XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0), 0), 1);
      recog_dup_num[1] = 2;
      break;

    case 119:
    case 116:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[3] = *(ro_loc[3] = &XEXP (XVECEXP (pat, 0, 1), 0));
      recog_dup_loc[0] = &XEXP (XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0), 0), 0), 0);
      recog_dup_num[0] = 1;
      recog_dup_loc[1] = &XEXP (XEXP (XEXP (XEXP (XEXP (XVECEXP (pat, 0, 1), 1), 0), 0), 1), 0);
      recog_dup_num[1] = 2;
      break;

    case 114:
    case 111:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 113:
    case 110:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 167:
    case 164:
    case 160:
    case 157:
    case 153:
    case 150:
    case 90:
    case 87:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      recog_dup_loc[0] = &XEXP (XEXP (pat, 1), 1);
      recog_dup_num[0] = 0;
      break;

    case 228:
    case 226:
    case 223:
    case 221:
    case 218:
    case 216:
    case 211:
    case 209:
    case 204:
    case 202:
    case 197:
    case 195:
    case 166:
    case 163:
    case 159:
    case 156:
    case 152:
    case 149:
    case 102:
    case 100:
    case 89:
    case 86:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 1));
      recog_dup_loc[0] = &XEXP (XEXP (pat, 1), 0);
      recog_dup_num[0] = 0;
      break;

    case 98:
    case 84:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (XEXP (pat, 1), 1), 0));
      break;

    case 339:
    case 337:
    case 335:
    case 333:
    case 291:
    case 290:
    case 227:
    case 225:
    case 224:
    case 222:
    case 220:
    case 219:
    case 217:
    case 215:
    case 214:
    case 213:
    case 212:
    case 210:
    case 208:
    case 207:
    case 206:
    case 205:
    case 203:
    case 201:
    case 200:
    case 199:
    case 198:
    case 196:
    case 194:
    case 193:
    case 192:
    case 191:
    case 165:
    case 162:
    case 161:
    case 158:
    case 155:
    case 154:
    case 151:
    case 148:
    case 147:
    case 142:
    case 139:
    case 138:
    case 137:
    case 135:
    case 134:
    case 130:
    case 127:
    case 126:
    case 125:
    case 123:
    case 122:
    case 112:
    case 109:
    case 108:
    case 107:
    case 105:
    case 104:
    case 101:
    case 99:
    case 97:
    case 96:
    case 95:
    case 93:
    case 92:
    case 88:
    case 85:
    case 83:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 82:
    case 81:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 0), 0));
      break;

    case 72:
    case 71:
    case 70:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0), 0));
      ro[2] = *(ro_loc[2] = &XEXP (XVECEXP (pat, 0, 1), 0));
      ro[3] = *(ro_loc[3] = &XEXP (XVECEXP (pat, 0, 2), 0));
      break;

    case 342:
    case 341:
    case 340:
    case 331:
    case 330:
    case 329:
    case 328:
    case 327:
    case 326:
    case 325:
    case 324:
    case 323:
    case 322:
    case 321:
    case 189:
    case 187:
    case 186:
    case 185:
    case 184:
    case 182:
    case 181:
    case 179:
    case 178:
    case 177:
    case 175:
    case 174:
    case 171:
    case 169:
    case 168:
    case 80:
    case 79:
    case 78:
    case 77:
    case 76:
    case 75:
    case 74:
    case 73:
    case 69:
    case 68:
    case 67:
    case 66:
    case 65:
    case 64:
    case 62:
    case 61:
    case 59:
    case 58:
    case 57:
    case 55:
    case 54:
    case 52:
    case 51:
    case 50:
    case 49:
    case 48:
    case 47:
    case 43:
    case 42:
    case 41:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 0));
      break;

    case 33:
    case 31:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 0), 0));
      ro[1] = *(ro_loc[1] = &XEXP (pat, 1));
      break;

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
    case 27:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      break;

    case 25:
    case 24:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 2));
      break;

    case 23:
    case 22:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (XEXP (pat, 1), 2), 1), 0));
      break;

    case 21:
    case 20:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XEXP (pat, 1), 2), 1));
      break;

    case 18:
    case 15:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (XVECEXP (pat, 0, 0), 1), 1));
      ro[2] = *(ro_loc[2] = &XEXP (XVECEXP (pat, 0, 1), 0));
      break;

    case 320:
    case 19:
    case 16:
    case 13:
    case 12:
    case 11:
      ro[0] = *(ro_loc[0] = &XEXP (XEXP (pat, 1), 0));
      ro[1] = *(ro_loc[1] = &XEXP (XEXP (pat, 1), 1));
      break;

    case 9:
    case 6:
      ro[0] = *(ro_loc[0] = &XEXP (XVECEXP (pat, 0, 0), 1));
      ro[1] = *(ro_loc[1] = &XEXP (XVECEXP (pat, 0, 1), 0));
      break;

    case 318:
    case 297:
    case 10:
    case 7:
    case 4:
    case 3:
    case 2:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 1));
      break;

    case 298:
    case 288:
    case 287:
    case 40:
    case 39:
    case 38:
    case 37:
    case 35:
    case 34:
    case 32:
    case 30:
    case 29:
    case 26:
    case 1:
    case 0:
      ro[0] = *(ro_loc[0] = &XEXP (pat, 0));
      ro[1] = *(ro_loc[1] = &XEXP (pat, 1));
      break;

    default:
      abort ();
    }
}
