#define pp_i_preinc pp_preinc
#define pp_i_predec pp_predec
#define pp_i_postinc pp_postinc
#define pp_i_postdec pp_postdec

typedef enum {
	OP_NULL,	/* 0 */
	OP_STUB,	/* 1 */
	OP_SCALAR,	/* 2 */
	OP_PUSHMARK,	/* 3 */
	OP_WANTARRAY,	/* 4 */
	OP_CONST,	/* 5 */
	OP_GVSV,	/* 6 */
	OP_GV,		/* 7 */
	OP_GELEM,	/* 8 */
	OP_PADSV,	/* 9 */
	OP_PADAV,	/* 10 */
	OP_PADHV,	/* 11 */
	OP_PADANY,	/* 12 */
	OP_PUSHRE,	/* 13 */
	OP_RV2GV,	/* 14 */
	OP_RV2SV,	/* 15 */
	OP_AV2ARYLEN,	/* 16 */
	OP_RV2CV,	/* 17 */
	OP_ANONCODE,	/* 18 */
	OP_PROTOTYPE,	/* 19 */
	OP_REFGEN,	/* 20 */
	OP_SREFGEN,	/* 21 */
	OP_REF,		/* 22 */
	OP_BLESS,	/* 23 */
	OP_BACKTICK,	/* 24 */
	OP_GLOB,	/* 25 */
	OP_READLINE,	/* 26 */
	OP_RCATLINE,	/* 27 */
	OP_REGCMAYBE,	/* 28 */
	OP_REGCOMP,	/* 29 */
	OP_MATCH,	/* 30 */
	OP_SUBST,	/* 31 */
	OP_SUBSTCONT,	/* 32 */
	OP_TRANS,	/* 33 */
	OP_SASSIGN,	/* 34 */
	OP_AASSIGN,	/* 35 */
	OP_CHOP,	/* 36 */
	OP_SCHOP,	/* 37 */
	OP_CHOMP,	/* 38 */
	OP_SCHOMP,	/* 39 */
	OP_DEFINED,	/* 40 */
	OP_UNDEF,	/* 41 */
	OP_STUDY,	/* 42 */
	OP_POS,		/* 43 */
	OP_PREINC,	/* 44 */
	OP_I_PREINC,	/* 45 */
	OP_PREDEC,	/* 46 */
	OP_I_PREDEC,	/* 47 */
	OP_POSTINC,	/* 48 */
	OP_I_POSTINC,	/* 49 */
	OP_POSTDEC,	/* 50 */
	OP_I_POSTDEC,	/* 51 */
	OP_POW,		/* 52 */
	OP_MULTIPLY,	/* 53 */
	OP_I_MULTIPLY,	/* 54 */
	OP_DIVIDE,	/* 55 */
	OP_I_DIVIDE,	/* 56 */
	OP_MODULO,	/* 57 */
	OP_I_MODULO,	/* 58 */
	OP_REPEAT,	/* 59 */
	OP_ADD,		/* 60 */
	OP_I_ADD,	/* 61 */
	OP_SUBTRACT,	/* 62 */
	OP_I_SUBTRACT,	/* 63 */
	OP_CONCAT,	/* 64 */
	OP_STRINGIFY,	/* 65 */
	OP_LEFT_SHIFT,	/* 66 */
	OP_RIGHT_SHIFT,	/* 67 */
	OP_LT,		/* 68 */
	OP_I_LT,	/* 69 */
	OP_GT,		/* 70 */
	OP_I_GT,	/* 71 */
	OP_LE,		/* 72 */
	OP_I_LE,	/* 73 */
	OP_GE,		/* 74 */
	OP_I_GE,	/* 75 */
	OP_EQ,		/* 76 */
	OP_I_EQ,	/* 77 */
	OP_NE,		/* 78 */
	OP_I_NE,	/* 79 */
	OP_NCMP,	/* 80 */
	OP_I_NCMP,	/* 81 */
	OP_SLT,		/* 82 */
	OP_SGT,		/* 83 */
	OP_SLE,		/* 84 */
	OP_SGE,		/* 85 */
	OP_SEQ,		/* 86 */
	OP_SNE,		/* 87 */
	OP_SCMP,	/* 88 */
	OP_BIT_AND,	/* 89 */
	OP_BIT_XOR,	/* 90 */
	OP_BIT_OR,	/* 91 */
	OP_NEGATE,	/* 92 */
	OP_I_NEGATE,	/* 93 */
	OP_NOT,		/* 94 */
	OP_COMPLEMENT,	/* 95 */
	OP_ATAN2,	/* 96 */
	OP_SIN,		/* 97 */
	OP_COS,		/* 98 */
	OP_RAND,	/* 99 */
	OP_SRAND,	/* 100 */
	OP_EXP,		/* 101 */
	OP_LOG,		/* 102 */
	OP_SQRT,	/* 103 */
	OP_INT,		/* 104 */
	OP_HEX,		/* 105 */
	OP_OCT,		/* 106 */
	OP_ABS,		/* 107 */
	OP_LENGTH,	/* 108 */
	OP_SUBSTR,	/* 109 */
	OP_VEC,		/* 110 */
	OP_INDEX,	/* 111 */
	OP_RINDEX,	/* 112 */
	OP_SPRINTF,	/* 113 */
	OP_FORMLINE,	/* 114 */
	OP_ORD,		/* 115 */
	OP_CHR,		/* 116 */
	OP_CRYPT,	/* 117 */
	OP_UCFIRST,	/* 118 */
	OP_LCFIRST,	/* 119 */
	OP_UC,		/* 120 */
	OP_LC,		/* 121 */
	OP_QUOTEMETA,	/* 122 */
	OP_RV2AV,	/* 123 */
	OP_AELEMFAST,	/* 124 */
	OP_AELEM,	/* 125 */
	OP_ASLICE,	/* 126 */
	OP_EACH,	/* 127 */
	OP_VALUES,	/* 128 */
	OP_KEYS,	/* 129 */
	OP_DELETE,	/* 130 */
	OP_EXISTS,	/* 131 */
	OP_RV2HV,	/* 132 */
	OP_HELEM,	/* 133 */
	OP_HSLICE,	/* 134 */
	OP_UNPACK,	/* 135 */
	OP_PACK,	/* 136 */
	OP_SPLIT,	/* 137 */
	OP_JOIN,	/* 138 */
	OP_LIST,	/* 139 */
	OP_LSLICE,	/* 140 */
	OP_ANONLIST,	/* 141 */
	OP_ANONHASH,	/* 142 */
	OP_SPLICE,	/* 143 */
	OP_PUSH,	/* 144 */
	OP_POP,		/* 145 */
	OP_SHIFT,	/* 146 */
	OP_UNSHIFT,	/* 147 */
	OP_SORT,	/* 148 */
	OP_REVERSE,	/* 149 */
	OP_GREPSTART,	/* 150 */
	OP_GREPWHILE,	/* 151 */
	OP_MAPSTART,	/* 152 */
	OP_MAPWHILE,	/* 153 */
	OP_RANGE,	/* 154 */
	OP_FLIP,	/* 155 */
	OP_FLOP,	/* 156 */
	OP_AND,		/* 157 */
	OP_OR,		/* 158 */
	OP_XOR,		/* 159 */
	OP_COND_EXPR,	/* 160 */
	OP_ANDASSIGN,	/* 161 */
	OP_ORASSIGN,	/* 162 */
	OP_METHOD,	/* 163 */
	OP_ENTERSUB,	/* 164 */
	OP_LEAVESUB,	/* 165 */
	OP_CALLER,	/* 166 */
	OP_WARN,	/* 167 */
	OP_DIE,		/* 168 */
	OP_RESET,	/* 169 */
	OP_LINESEQ,	/* 170 */
	OP_NEXTSTATE,	/* 171 */
	OP_DBSTATE,	/* 172 */
	OP_UNSTACK,	/* 173 */
	OP_ENTER,	/* 174 */
	OP_LEAVE,	/* 175 */
	OP_SCOPE,	/* 176 */
	OP_ENTERITER,	/* 177 */
	OP_ITER,	/* 178 */
	OP_ENTERLOOP,	/* 179 */
	OP_LEAVELOOP,	/* 180 */
	OP_RETURN,	/* 181 */
	OP_LAST,	/* 182 */
	OP_NEXT,	/* 183 */
	OP_REDO,	/* 184 */
	OP_DUMP,	/* 185 */
	OP_GOTO,	/* 186 */
	OP_EXIT,	/* 187 */
	OP_OPEN,	/* 188 */
	OP_CLOSE,	/* 189 */
	OP_PIPE_OP,	/* 190 */
	OP_FILENO,	/* 191 */
	OP_UMASK,	/* 192 */
	OP_BINMODE,	/* 193 */
	OP_TIE,		/* 194 */
	OP_UNTIE,	/* 195 */
	OP_TIED,	/* 196 */
	OP_DBMOPEN,	/* 197 */
	OP_DBMCLOSE,	/* 198 */
	OP_SSELECT,	/* 199 */
	OP_SELECT,	/* 200 */
	OP_GETC,	/* 201 */
	OP_READ,	/* 202 */
	OP_ENTERWRITE,	/* 203 */
	OP_LEAVEWRITE,	/* 204 */
	OP_PRTF,	/* 205 */
	OP_PRINT,	/* 206 */
	OP_SYSOPEN,	/* 207 */
	OP_SYSREAD,	/* 208 */
	OP_SYSWRITE,	/* 209 */
	OP_SEND,	/* 210 */
	OP_RECV,	/* 211 */
	OP_EOF,		/* 212 */
	OP_TELL,	/* 213 */
	OP_SEEK,	/* 214 */
	OP_TRUNCATE,	/* 215 */
	OP_FCNTL,	/* 216 */
	OP_IOCTL,	/* 217 */
	OP_FLOCK,	/* 218 */
	OP_SOCKET,	/* 219 */
	OP_SOCKPAIR,	/* 220 */
	OP_BIND,	/* 221 */
	OP_CONNECT,	/* 222 */
	OP_LISTEN,	/* 223 */
	OP_ACCEPT,	/* 224 */
	OP_SHUTDOWN,	/* 225 */
	OP_GSOCKOPT,	/* 226 */
	OP_SSOCKOPT,	/* 227 */
	OP_GETSOCKNAME,	/* 228 */
	OP_GETPEERNAME,	/* 229 */
	OP_LSTAT,	/* 230 */
	OP_STAT,	/* 231 */
	OP_FTRREAD,	/* 232 */
	OP_FTRWRITE,	/* 233 */
	OP_FTREXEC,	/* 234 */
	OP_FTEREAD,	/* 235 */
	OP_FTEWRITE,	/* 236 */
	OP_FTEEXEC,	/* 237 */
	OP_FTIS,	/* 238 */
	OP_FTEOWNED,	/* 239 */
	OP_FTROWNED,	/* 240 */
	OP_FTZERO,	/* 241 */
	OP_FTSIZE,	/* 242 */
	OP_FTMTIME,	/* 243 */
	OP_FTATIME,	/* 244 */
	OP_FTCTIME,	/* 245 */
	OP_FTSOCK,	/* 246 */
	OP_FTCHR,	/* 247 */
	OP_FTBLK,	/* 248 */
	OP_FTFILE,	/* 249 */
	OP_FTDIR,	/* 250 */
	OP_FTPIPE,	/* 251 */
	OP_FTLINK,	/* 252 */
	OP_FTSUID,	/* 253 */
	OP_FTSGID,	/* 254 */
	OP_FTSVTX,	/* 255 */
	OP_FTTTY,	/* 256 */
	OP_FTTEXT,	/* 257 */
	OP_FTBINARY,	/* 258 */
	OP_CHDIR,	/* 259 */
	OP_CHOWN,	/* 260 */
	OP_CHROOT,	/* 261 */
	OP_UNLINK,	/* 262 */
	OP_CHMOD,	/* 263 */
	OP_UTIME,	/* 264 */
	OP_RENAME,	/* 265 */
	OP_LINK,	/* 266 */
	OP_SYMLINK,	/* 267 */
	OP_READLINK,	/* 268 */
	OP_MKDIR,	/* 269 */
	OP_RMDIR,	/* 270 */
	OP_OPEN_DIR,	/* 271 */
	OP_READDIR,	/* 272 */
	OP_TELLDIR,	/* 273 */
	OP_SEEKDIR,	/* 274 */
	OP_REWINDDIR,	/* 275 */
	OP_CLOSEDIR,	/* 276 */
	OP_FORK,	/* 277 */
	OP_WAIT,	/* 278 */
	OP_WAITPID,	/* 279 */
	OP_SYSTEM,	/* 280 */
	OP_EXEC,	/* 281 */
	OP_KILL,	/* 282 */
	OP_GETPPID,	/* 283 */
	OP_GETPGRP,	/* 284 */
	OP_SETPGRP,	/* 285 */
	OP_GETPRIORITY,	/* 286 */
	OP_SETPRIORITY,	/* 287 */
	OP_TIME,	/* 288 */
	OP_TMS,		/* 289 */
	OP_LOCALTIME,	/* 290 */
	OP_GMTIME,	/* 291 */
	OP_ALARM,	/* 292 */
	OP_SLEEP,	/* 293 */
	OP_SHMGET,	/* 294 */
	OP_SHMCTL,	/* 295 */
	OP_SHMREAD,	/* 296 */
	OP_SHMWRITE,	/* 297 */
	OP_MSGGET,	/* 298 */
	OP_MSGCTL,	/* 299 */
	OP_MSGSND,	/* 300 */
	OP_MSGRCV,	/* 301 */
	OP_SEMGET,	/* 302 */
	OP_SEMCTL,	/* 303 */
	OP_SEMOP,	/* 304 */
	OP_REQUIRE,	/* 305 */
	OP_DOFILE,	/* 306 */
	OP_ENTEREVAL,	/* 307 */
	OP_LEAVEEVAL,	/* 308 */
	OP_ENTERTRY,	/* 309 */
	OP_LEAVETRY,	/* 310 */
	OP_GHBYNAME,	/* 311 */
	OP_GHBYADDR,	/* 312 */
	OP_GHOSTENT,	/* 313 */
	OP_GNBYNAME,	/* 314 */
	OP_GNBYADDR,	/* 315 */
	OP_GNETENT,	/* 316 */
	OP_GPBYNAME,	/* 317 */
	OP_GPBYNUMBER,	/* 318 */
	OP_GPROTOENT,	/* 319 */
	OP_GSBYNAME,	/* 320 */
	OP_GSBYPORT,	/* 321 */
	OP_GSERVENT,	/* 322 */
	OP_SHOSTENT,	/* 323 */
	OP_SNETENT,	/* 324 */
	OP_SPROTOENT,	/* 325 */
	OP_SSERVENT,	/* 326 */
	OP_EHOSTENT,	/* 327 */
	OP_ENETENT,	/* 328 */
	OP_EPROTOENT,	/* 329 */
	OP_ESERVENT,	/* 330 */
	OP_GPWNAM,	/* 331 */
	OP_GPWUID,	/* 332 */
	OP_GPWENT,	/* 333 */
	OP_SPWENT,	/* 334 */
	OP_EPWENT,	/* 335 */
	OP_GGRNAM,	/* 336 */
	OP_GGRGID,	/* 337 */
	OP_GGRENT,	/* 338 */
	OP_SGRENT,	/* 339 */
	OP_EGRENT,	/* 340 */
	OP_GETLOGIN,	/* 341 */
	OP_SYSCALL,	/* 342 */
	OP_max		
} opcode;

#define MAXO 343

#ifndef DOINIT
EXT char *op_name[];
#else
EXT char *op_name[] = {
	"null",
	"stub",
	"scalar",
	"pushmark",
	"wantarray",
	"const",
	"gvsv",
	"gv",
	"gelem",
	"padsv",
	"padav",
	"padhv",
	"padany",
	"pushre",
	"rv2gv",
	"rv2sv",
	"av2arylen",
	"rv2cv",
	"anoncode",
	"prototype",
	"refgen",
	"srefgen",
	"ref",
	"bless",
	"backtick",
	"glob",
	"readline",
	"rcatline",
	"regcmaybe",
	"regcomp",
	"match",
	"subst",
	"substcont",
	"trans",
	"sassign",
	"aassign",
	"chop",
	"schop",
	"chomp",
	"schomp",
	"defined",
	"undef",
	"study",
	"pos",
	"preinc",
	"i_preinc",
	"predec",
	"i_predec",
	"postinc",
	"i_postinc",
	"postdec",
	"i_postdec",
	"pow",
	"multiply",
	"i_multiply",
	"divide",
	"i_divide",
	"modulo",
	"i_modulo",
	"repeat",
	"add",
	"i_add",
	"subtract",
	"i_subtract",
	"concat",
	"stringify",
	"left_shift",
	"right_shift",
	"lt",
	"i_lt",
	"gt",
	"i_gt",
	"le",
	"i_le",
	"ge",
	"i_ge",
	"eq",
	"i_eq",
	"ne",
	"i_ne",
	"ncmp",
	"i_ncmp",
	"slt",
	"sgt",
	"sle",
	"sge",
	"seq",
	"sne",
	"scmp",
	"bit_and",
	"bit_xor",
	"bit_or",
	"negate",
	"i_negate",
	"not",
	"complement",
	"atan2",
	"sin",
	"cos",
	"rand",
	"srand",
	"exp",
	"log",
	"sqrt",
	"int",
	"hex",
	"oct",
	"abs",
	"length",
	"substr",
	"vec",
	"index",
	"rindex",
	"sprintf",
	"formline",
	"ord",
	"chr",
	"crypt",
	"ucfirst",
	"lcfirst",
	"uc",
	"lc",
	"quotemeta",
	"rv2av",
	"aelemfast",
	"aelem",
	"aslice",
	"each",
	"values",
	"keys",
	"delete",
	"exists",
	"rv2hv",
	"helem",
	"hslice",
	"unpack",
	"pack",
	"split",
	"join",
	"list",
	"lslice",
	"anonlist",
	"anonhash",
	"splice",
	"push",
	"pop",
	"shift",
	"unshift",
	"sort",
	"reverse",
	"grepstart",
	"grepwhile",
	"mapstart",
	"mapwhile",
	"range",
	"flip",
	"flop",
	"and",
	"or",
	"xor",
	"cond_expr",
	"andassign",
	"orassign",
	"method",
	"entersub",
	"leavesub",
	"caller",
	"warn",
	"die",
	"reset",
	"lineseq",
	"nextstate",
	"dbstate",
	"unstack",
	"enter",
	"leave",
	"scope",
	"enteriter",
	"iter",
	"enterloop",
	"leaveloop",
	"return",
	"last",
	"next",
	"redo",
	"dump",
	"goto",
	"exit",
	"open",
	"close",
	"pipe_op",
	"fileno",
	"umask",
	"binmode",
	"tie",
	"untie",
	"tied",
	"dbmopen",
	"dbmclose",
	"sselect",
	"select",
	"getc",
	"read",
	"enterwrite",
	"leavewrite",
	"prtf",
	"print",
	"sysopen",
	"sysread",
	"syswrite",
	"send",
	"recv",
	"eof",
	"tell",
	"seek",
	"truncate",
	"fcntl",
	"ioctl",
	"flock",
	"socket",
	"sockpair",
	"bind",
	"connect",
	"listen",
	"accept",
	"shutdown",
	"gsockopt",
	"ssockopt",
	"getsockname",
	"getpeername",
	"lstat",
	"stat",
	"ftrread",
	"ftrwrite",
	"ftrexec",
	"fteread",
	"ftewrite",
	"fteexec",
	"ftis",
	"fteowned",
	"ftrowned",
	"ftzero",
	"ftsize",
	"ftmtime",
	"ftatime",
	"ftctime",
	"ftsock",
	"ftchr",
	"ftblk",
	"ftfile",
	"ftdir",
	"ftpipe",
	"ftlink",
	"ftsuid",
	"ftsgid",
	"ftsvtx",
	"fttty",
	"fttext",
	"ftbinary",
	"chdir",
	"chown",
	"chroot",
	"unlink",
	"chmod",
	"utime",
	"rename",
	"link",
	"symlink",
	"readlink",
	"mkdir",
	"rmdir",
	"open_dir",
	"readdir",
	"telldir",
	"seekdir",
	"rewinddir",
	"closedir",
	"fork",
	"wait",
	"waitpid",
	"system",
	"exec",
	"kill",
	"getppid",
	"getpgrp",
	"setpgrp",
	"getpriority",
	"setpriority",
	"time",
	"tms",
	"localtime",
	"gmtime",
	"alarm",
	"sleep",
	"shmget",
	"shmctl",
	"shmread",
	"shmwrite",
	"msgget",
	"msgctl",
	"msgsnd",
	"msgrcv",
	"semget",
	"semctl",
	"semop",
	"require",
	"dofile",
	"entereval",
	"leaveeval",
	"entertry",
	"leavetry",
	"ghbyname",
	"ghbyaddr",
	"ghostent",
	"gnbyname",
	"gnbyaddr",
	"gnetent",
	"gpbyname",
	"gpbynumber",
	"gprotoent",
	"gsbyname",
	"gsbyport",
	"gservent",
	"shostent",
	"snetent",
	"sprotoent",
	"sservent",
	"ehostent",
	"enetent",
	"eprotoent",
	"eservent",
	"gpwnam",
	"gpwuid",
	"gpwent",
	"spwent",
	"epwent",
	"ggrnam",
	"ggrgid",
	"ggrent",
	"sgrent",
	"egrent",
	"getlogin",
	"syscall",
};
#endif

#ifndef DOINIT
EXT char *op_desc[];
#else
EXT char *op_desc[] = {
	"null operation",
	"stub",
	"scalar",
	"pushmark",
	"wantarray",
	"constant item",
	"scalar variable",
	"glob value",
	"glob elem",
	"private variable",
	"private array",
	"private hash",
	"private something",
	"push regexp",
	"ref-to-glob cast",
	"scalar deref",
	"array length",
	"subroutine deref",
	"anonymous subroutine",
	"subroutine prototype",
	"reference constructor",
	"scalar ref constructor",
	"reference-type operator",
	"bless",
	"backticks",
	"glob",
	"<HANDLE>",
	"append I/O operator",
	"regexp comp once",
	"regexp compilation",
	"pattern match",
	"substitution",
	"substitution cont",
	"character translation",
	"scalar assignment",
	"list assignment",
	"chop",
	"scalar chop",
	"safe chop",
	"scalar safe chop",
	"defined operator",
	"undef operator",
	"study",
	"match position",
	"preincrement",
	"integer preincrement",
	"predecrement",
	"integer predecrement",
	"postincrement",
	"integer postincrement",
	"postdecrement",
	"integer postdecrement",
	"exponentiation",
	"multiplication",
	"integer multiplication",
	"division",
	"integer division",
	"modulus",
	"integer modulus",
	"repeat",
	"addition",
	"integer addition",
	"subtraction",
	"integer subtraction",
	"concatenation",
	"string",
	"left bitshift",
	"right bitshift",
	"numeric lt",
	"integer lt",
	"numeric gt",
	"integer gt",
	"numeric le",
	"integer le",
	"numeric ge",
	"integer ge",
	"numeric eq",
	"integer eq",
	"numeric ne",
	"integer ne",
	"spaceship operator",
	"integer spaceship",
	"string lt",
	"string gt",
	"string le",
	"string ge",
	"string eq",
	"string ne",
	"string comparison",
	"bitwise and",
	"bitwise xor",
	"bitwise or",
	"negate",
	"integer negate",
	"not",
	"1's complement",
	"atan2",
	"sin",
	"cos",
	"rand",
	"srand",
	"exp",
	"log",
	"sqrt",
	"int",
	"hex",
	"oct",
	"abs",
	"length",
	"substr",
	"vec",
	"index",
	"rindex",
	"sprintf",
	"formline",
	"ord",
	"chr",
	"crypt",
	"upper case first",
	"lower case first",
	"upper case",
	"lower case",
	"quote metachars",
	"array deref",
	"known array element",
	"array element",
	"array slice",
	"each",
	"values",
	"keys",
	"delete",
	"exists operator",
	"associative array deref",
	"associative array elem",
	"associative array slice",
	"unpack",
	"pack",
	"split",
	"join",
	"list",
	"list slice",
	"anonymous list",
	"anonymous hash",
	"splice",
	"push",
	"pop",
	"shift",
	"unshift",
	"sort",
	"reverse",
	"grep",
	"grep iterator",
	"map",
	"map iterator",
	"flipflop",
	"range (or flip)",
	"range (or flop)",
	"logical and",
	"logical or",
	"logical xor",
	"conditional expression",
	"logical and assignment",
	"logical or assignment",
	"method lookup",
	"subroutine entry",
	"subroutine exit",
	"caller",
	"warn",
	"die",
	"reset",
	"line sequence",
	"next statement",
	"debug next statement",
	"unstack",
	"block entry",
	"block exit",
	"block",
	"foreach loop entry",
	"foreach loop iterator",
	"loop entry",
	"loop exit",
	"return",
	"last",
	"next",
	"redo",
	"dump",
	"goto",
	"exit",
	"open",
	"close",
	"pipe",
	"fileno",
	"umask",
	"binmode",
	"tie",
	"untie",
	"tied",
	"dbmopen",
	"dbmclose",
	"select system call",
	"select",
	"getc",
	"read",
	"write",
	"write exit",
	"printf",
	"print",
	"sysopen",
	"sysread",
	"syswrite",
	"send",
	"recv",
	"eof",
	"tell",
	"seek",
	"truncate",
	"fcntl",
	"ioctl",
	"flock",
	"socket",
	"socketpair",
	"bind",
	"connect",
	"listen",
	"accept",
	"shutdown",
	"getsockopt",
	"setsockopt",
	"getsockname",
	"getpeername",
	"lstat",
	"stat",
	"-R",
	"-W",
	"-X",
	"-r",
	"-w",
	"-x",
	"-e",
	"-O",
	"-o",
	"-z",
	"-s",
	"-M",
	"-A",
	"-C",
	"-S",
	"-c",
	"-b",
	"-f",
	"-d",
	"-p",
	"-l",
	"-u",
	"-g",
	"-k",
	"-t",
	"-T",
	"-B",
	"chdir",
	"chown",
	"chroot",
	"unlink",
	"chmod",
	"utime",
	"rename",
	"link",
	"symlink",
	"readlink",
	"mkdir",
	"rmdir",
	"opendir",
	"readdir",
	"telldir",
	"seekdir",
	"rewinddir",
	"closedir",
	"fork",
	"wait",
	"waitpid",
	"system",
	"exec",
	"kill",
	"getppid",
	"getpgrp",
	"setpgrp",
	"getpriority",
	"setpriority",
	"time",
	"times",
	"localtime",
	"gmtime",
	"alarm",
	"sleep",
	"shmget",
	"shmctl",
	"shmread",
	"shmwrite",
	"msgget",
	"msgctl",
	"msgsnd",
	"msgrcv",
	"semget",
	"semctl",
	"semop",
	"require",
	"do 'file'",
	"eval string",
	"eval exit",
	"eval block",
	"eval block exit",
	"gethostbyname",
	"gethostbyaddr",
	"gethostent",
	"getnetbyname",
	"getnetbyaddr",
	"getnetent",
	"getprotobyname",
	"getprotobynumber",
	"getprotoent",
	"getservbyname",
	"getservbyport",
	"getservent",
	"sethostent",
	"setnetent",
	"setprotoent",
	"setservent",
	"endhostent",
	"endnetent",
	"endprotoent",
	"endservent",
	"getpwnam",
	"getpwuid",
	"getpwent",
	"setpwent",
	"endpwent",
	"getgrnam",
	"getgrgid",
	"getgrent",
	"setgrent",
	"endgrent",
	"getlogin",
	"syscall",
};
#endif

OP *	ck_concat	_((OP* op));
OP *	ck_delete	_((OP* op));
OP *	ck_eof		_((OP* op));
OP *	ck_eval		_((OP* op));
OP *	ck_exec		_((OP* op));
OP *	ck_formline	_((OP* op));
OP *	ck_ftst		_((OP* op));
OP *	ck_fun		_((OP* op));
OP *	ck_glob		_((OP* op));
OP *	ck_grep		_((OP* op));
OP *	ck_index	_((OP* op));
OP *	ck_lengthconst	_((OP* op));
OP *	ck_lfun		_((OP* op));
OP *	ck_listiob	_((OP* op));
OP *	ck_match	_((OP* op));
OP *	ck_null		_((OP* op));
OP *	ck_repeat	_((OP* op));
OP *	ck_require	_((OP* op));
OP *	ck_rfun		_((OP* op));
OP *	ck_rvconst	_((OP* op));
OP *	ck_select	_((OP* op));
OP *	ck_shift	_((OP* op));
OP *	ck_sort		_((OP* op));
OP *	ck_spair	_((OP* op));
OP *	ck_split	_((OP* op));
OP *	ck_subr		_((OP* op));
OP *	ck_svconst	_((OP* op));
OP *	ck_trunc	_((OP* op));

OP *	pp_null		_((void));
OP *	pp_stub		_((void));
OP *	pp_scalar	_((void));
OP *	pp_pushmark	_((void));
OP *	pp_wantarray	_((void));
OP *	pp_const	_((void));
OP *	pp_gvsv		_((void));
OP *	pp_gv		_((void));
OP *	pp_gelem	_((void));
OP *	pp_padsv	_((void));
OP *	pp_padav	_((void));
OP *	pp_padhv	_((void));
OP *	pp_padany	_((void));
OP *	pp_pushre	_((void));
OP *	pp_rv2gv	_((void));
OP *	pp_rv2sv	_((void));
OP *	pp_av2arylen	_((void));
OP *	pp_rv2cv	_((void));
OP *	pp_anoncode	_((void));
OP *	pp_prototype	_((void));
OP *	pp_refgen	_((void));
OP *	pp_srefgen	_((void));
OP *	pp_ref		_((void));
OP *	pp_bless	_((void));
OP *	pp_backtick	_((void));
OP *	pp_glob		_((void));
OP *	pp_readline	_((void));
OP *	pp_rcatline	_((void));
OP *	pp_regcmaybe	_((void));
OP *	pp_regcomp	_((void));
OP *	pp_match	_((void));
OP *	pp_subst	_((void));
OP *	pp_substcont	_((void));
OP *	pp_trans	_((void));
OP *	pp_sassign	_((void));
OP *	pp_aassign	_((void));
OP *	pp_chop		_((void));
OP *	pp_schop	_((void));
OP *	pp_chomp	_((void));
OP *	pp_schomp	_((void));
OP *	pp_defined	_((void));
OP *	pp_undef	_((void));
OP *	pp_study	_((void));
OP *	pp_pos		_((void));
OP *	pp_preinc	_((void));
OP *	pp_i_preinc	_((void));
OP *	pp_predec	_((void));
OP *	pp_i_predec	_((void));
OP *	pp_postinc	_((void));
OP *	pp_i_postinc	_((void));
OP *	pp_postdec	_((void));
OP *	pp_i_postdec	_((void));
OP *	pp_pow		_((void));
OP *	pp_multiply	_((void));
OP *	pp_i_multiply	_((void));
OP *	pp_divide	_((void));
OP *	pp_i_divide	_((void));
OP *	pp_modulo	_((void));
OP *	pp_i_modulo	_((void));
OP *	pp_repeat	_((void));
OP *	pp_add		_((void));
OP *	pp_i_add	_((void));
OP *	pp_subtract	_((void));
OP *	pp_i_subtract	_((void));
OP *	pp_concat	_((void));
OP *	pp_stringify	_((void));
OP *	pp_left_shift	_((void));
OP *	pp_right_shift	_((void));
OP *	pp_lt		_((void));
OP *	pp_i_lt		_((void));
OP *	pp_gt		_((void));
OP *	pp_i_gt		_((void));
OP *	pp_le		_((void));
OP *	pp_i_le		_((void));
OP *	pp_ge		_((void));
OP *	pp_i_ge		_((void));
OP *	pp_eq		_((void));
OP *	pp_i_eq		_((void));
OP *	pp_ne		_((void));
OP *	pp_i_ne		_((void));
OP *	pp_ncmp		_((void));
OP *	pp_i_ncmp	_((void));
OP *	pp_slt		_((void));
OP *	pp_sgt		_((void));
OP *	pp_sle		_((void));
OP *	pp_sge		_((void));
OP *	pp_seq		_((void));
OP *	pp_sne		_((void));
OP *	pp_scmp		_((void));
OP *	pp_bit_and	_((void));
OP *	pp_bit_xor	_((void));
OP *	pp_bit_or	_((void));
OP *	pp_negate	_((void));
OP *	pp_i_negate	_((void));
OP *	pp_not		_((void));
OP *	pp_complement	_((void));
OP *	pp_atan2	_((void));
OP *	pp_sin		_((void));
OP *	pp_cos		_((void));
OP *	pp_rand		_((void));
OP *	pp_srand	_((void));
OP *	pp_exp		_((void));
OP *	pp_log		_((void));
OP *	pp_sqrt		_((void));
OP *	pp_int		_((void));
OP *	pp_hex		_((void));
OP *	pp_oct		_((void));
OP *	pp_abs		_((void));
OP *	pp_length	_((void));
OP *	pp_substr	_((void));
OP *	pp_vec		_((void));
OP *	pp_index	_((void));
OP *	pp_rindex	_((void));
OP *	pp_sprintf	_((void));
OP *	pp_formline	_((void));
OP *	pp_ord		_((void));
OP *	pp_chr		_((void));
OP *	pp_crypt	_((void));
OP *	pp_ucfirst	_((void));
OP *	pp_lcfirst	_((void));
OP *	pp_uc		_((void));
OP *	pp_lc		_((void));
OP *	pp_quotemeta	_((void));
OP *	pp_rv2av	_((void));
OP *	pp_aelemfast	_((void));
OP *	pp_aelem	_((void));
OP *	pp_aslice	_((void));
OP *	pp_each		_((void));
OP *	pp_values	_((void));
OP *	pp_keys		_((void));
OP *	pp_delete	_((void));
OP *	pp_exists	_((void));
OP *	pp_rv2hv	_((void));
OP *	pp_helem	_((void));
OP *	pp_hslice	_((void));
OP *	pp_unpack	_((void));
OP *	pp_pack		_((void));
OP *	pp_split	_((void));
OP *	pp_join		_((void));
OP *	pp_list		_((void));
OP *	pp_lslice	_((void));
OP *	pp_anonlist	_((void));
OP *	pp_anonhash	_((void));
OP *	pp_splice	_((void));
OP *	pp_push		_((void));
OP *	pp_pop		_((void));
OP *	pp_shift	_((void));
OP *	pp_unshift	_((void));
OP *	pp_sort		_((void));
OP *	pp_reverse	_((void));
OP *	pp_grepstart	_((void));
OP *	pp_grepwhile	_((void));
OP *	pp_mapstart	_((void));
OP *	pp_mapwhile	_((void));
OP *	pp_range	_((void));
OP *	pp_flip		_((void));
OP *	pp_flop		_((void));
OP *	pp_and		_((void));
OP *	pp_or		_((void));
OP *	pp_xor		_((void));
OP *	pp_cond_expr	_((void));
OP *	pp_andassign	_((void));
OP *	pp_orassign	_((void));
OP *	pp_method	_((void));
OP *	pp_entersub	_((void));
OP *	pp_leavesub	_((void));
OP *	pp_caller	_((void));
OP *	pp_warn		_((void));
OP *	pp_die		_((void));
OP *	pp_reset	_((void));
OP *	pp_lineseq	_((void));
OP *	pp_nextstate	_((void));
OP *	pp_dbstate	_((void));
OP *	pp_unstack	_((void));
OP *	pp_enter	_((void));
OP *	pp_leave	_((void));
OP *	pp_scope	_((void));
OP *	pp_enteriter	_((void));
OP *	pp_iter		_((void));
OP *	pp_enterloop	_((void));
OP *	pp_leaveloop	_((void));
OP *	pp_return	_((void));
OP *	pp_last		_((void));
OP *	pp_next		_((void));
OP *	pp_redo		_((void));
OP *	pp_dump		_((void));
OP *	pp_goto		_((void));
OP *	pp_exit		_((void));
OP *	pp_open		_((void));
OP *	pp_close	_((void));
OP *	pp_pipe_op	_((void));
OP *	pp_fileno	_((void));
OP *	pp_umask	_((void));
OP *	pp_binmode	_((void));
OP *	pp_tie		_((void));
OP *	pp_untie	_((void));
OP *	pp_tied		_((void));
OP *	pp_dbmopen	_((void));
OP *	pp_dbmclose	_((void));
OP *	pp_sselect	_((void));
OP *	pp_select	_((void));
OP *	pp_getc		_((void));
OP *	pp_read		_((void));
OP *	pp_enterwrite	_((void));
OP *	pp_leavewrite	_((void));
OP *	pp_prtf		_((void));
OP *	pp_print	_((void));
OP *	pp_sysopen	_((void));
OP *	pp_sysread	_((void));
OP *	pp_syswrite	_((void));
OP *	pp_send		_((void));
OP *	pp_recv		_((void));
OP *	pp_eof		_((void));
OP *	pp_tell		_((void));
OP *	pp_seek		_((void));
OP *	pp_truncate	_((void));
OP *	pp_fcntl	_((void));
OP *	pp_ioctl	_((void));
OP *	pp_flock	_((void));
OP *	pp_socket	_((void));
OP *	pp_sockpair	_((void));
OP *	pp_bind		_((void));
OP *	pp_connect	_((void));
OP *	pp_listen	_((void));
OP *	pp_accept	_((void));
OP *	pp_shutdown	_((void));
OP *	pp_gsockopt	_((void));
OP *	pp_ssockopt	_((void));
OP *	pp_getsockname	_((void));
OP *	pp_getpeername	_((void));
OP *	pp_lstat	_((void));
OP *	pp_stat		_((void));
OP *	pp_ftrread	_((void));
OP *	pp_ftrwrite	_((void));
OP *	pp_ftrexec	_((void));
OP *	pp_fteread	_((void));
OP *	pp_ftewrite	_((void));
OP *	pp_fteexec	_((void));
OP *	pp_ftis		_((void));
OP *	pp_fteowned	_((void));
OP *	pp_ftrowned	_((void));
OP *	pp_ftzero	_((void));
OP *	pp_ftsize	_((void));
OP *	pp_ftmtime	_((void));
OP *	pp_ftatime	_((void));
OP *	pp_ftctime	_((void));
OP *	pp_ftsock	_((void));
OP *	pp_ftchr	_((void));
OP *	pp_ftblk	_((void));
OP *	pp_ftfile	_((void));
OP *	pp_ftdir	_((void));
OP *	pp_ftpipe	_((void));
OP *	pp_ftlink	_((void));
OP *	pp_ftsuid	_((void));
OP *	pp_ftsgid	_((void));
OP *	pp_ftsvtx	_((void));
OP *	pp_fttty	_((void));
OP *	pp_fttext	_((void));
OP *	pp_ftbinary	_((void));
OP *	pp_chdir	_((void));
OP *	pp_chown	_((void));
OP *	pp_chroot	_((void));
OP *	pp_unlink	_((void));
OP *	pp_chmod	_((void));
OP *	pp_utime	_((void));
OP *	pp_rename	_((void));
OP *	pp_link		_((void));
OP *	pp_symlink	_((void));
OP *	pp_readlink	_((void));
OP *	pp_mkdir	_((void));
OP *	pp_rmdir	_((void));
OP *	pp_open_dir	_((void));
OP *	pp_readdir	_((void));
OP *	pp_telldir	_((void));
OP *	pp_seekdir	_((void));
OP *	pp_rewinddir	_((void));
OP *	pp_closedir	_((void));
OP *	pp_fork		_((void));
OP *	pp_wait		_((void));
OP *	pp_waitpid	_((void));
OP *	pp_system	_((void));
OP *	pp_exec		_((void));
OP *	pp_kill		_((void));
OP *	pp_getppid	_((void));
OP *	pp_getpgrp	_((void));
OP *	pp_setpgrp	_((void));
OP *	pp_getpriority	_((void));
OP *	pp_setpriority	_((void));
OP *	pp_time		_((void));
OP *	pp_tms		_((void));
OP *	pp_localtime	_((void));
OP *	pp_gmtime	_((void));
OP *	pp_alarm	_((void));
OP *	pp_sleep	_((void));
OP *	pp_shmget	_((void));
OP *	pp_shmctl	_((void));
OP *	pp_shmread	_((void));
OP *	pp_shmwrite	_((void));
OP *	pp_msgget	_((void));
OP *	pp_msgctl	_((void));
OP *	pp_msgsnd	_((void));
OP *	pp_msgrcv	_((void));
OP *	pp_semget	_((void));
OP *	pp_semctl	_((void));
OP *	pp_semop	_((void));
OP *	pp_require	_((void));
OP *	pp_dofile	_((void));
OP *	pp_entereval	_((void));
OP *	pp_leaveeval	_((void));
OP *	pp_entertry	_((void));
OP *	pp_leavetry	_((void));
OP *	pp_ghbyname	_((void));
OP *	pp_ghbyaddr	_((void));
OP *	pp_ghostent	_((void));
OP *	pp_gnbyname	_((void));
OP *	pp_gnbyaddr	_((void));
OP *	pp_gnetent	_((void));
OP *	pp_gpbyname	_((void));
OP *	pp_gpbynumber	_((void));
OP *	pp_gprotoent	_((void));
OP *	pp_gsbyname	_((void));
OP *	pp_gsbyport	_((void));
OP *	pp_gservent	_((void));
OP *	pp_shostent	_((void));
OP *	pp_snetent	_((void));
OP *	pp_sprotoent	_((void));
OP *	pp_sservent	_((void));
OP *	pp_ehostent	_((void));
OP *	pp_enetent	_((void));
OP *	pp_eprotoent	_((void));
OP *	pp_eservent	_((void));
OP *	pp_gpwnam	_((void));
OP *	pp_gpwuid	_((void));
OP *	pp_gpwent	_((void));
OP *	pp_spwent	_((void));
OP *	pp_epwent	_((void));
OP *	pp_ggrnam	_((void));
OP *	pp_ggrgid	_((void));
OP *	pp_ggrent	_((void));
OP *	pp_sgrent	_((void));
OP *	pp_egrent	_((void));
OP *	pp_getlogin	_((void));
OP *	pp_syscall	_((void));

#ifndef DOINIT
EXT OP * (*ppaddr[])();
#else
EXT OP * (*ppaddr[])() = {
	pp_null,
	pp_stub,
	pp_scalar,
	pp_pushmark,
	pp_wantarray,
	pp_const,
	pp_gvsv,
	pp_gv,
	pp_gelem,
	pp_padsv,
	pp_padav,
	pp_padhv,
	pp_padany,
	pp_pushre,
	pp_rv2gv,
	pp_rv2sv,
	pp_av2arylen,
	pp_rv2cv,
	pp_anoncode,
	pp_prototype,
	pp_refgen,
	pp_srefgen,
	pp_ref,
	pp_bless,
	pp_backtick,
	pp_glob,
	pp_readline,
	pp_rcatline,
	pp_regcmaybe,
	pp_regcomp,
	pp_match,
	pp_subst,
	pp_substcont,
	pp_trans,
	pp_sassign,
	pp_aassign,
	pp_chop,
	pp_schop,
	pp_chomp,
	pp_schomp,
	pp_defined,
	pp_undef,
	pp_study,
	pp_pos,
	pp_preinc,
	pp_i_preinc,
	pp_predec,
	pp_i_predec,
	pp_postinc,
	pp_i_postinc,
	pp_postdec,
	pp_i_postdec,
	pp_pow,
	pp_multiply,
	pp_i_multiply,
	pp_divide,
	pp_i_divide,
	pp_modulo,
	pp_i_modulo,
	pp_repeat,
	pp_add,
	pp_i_add,
	pp_subtract,
	pp_i_subtract,
	pp_concat,
	pp_stringify,
	pp_left_shift,
	pp_right_shift,
	pp_lt,
	pp_i_lt,
	pp_gt,
	pp_i_gt,
	pp_le,
	pp_i_le,
	pp_ge,
	pp_i_ge,
	pp_eq,
	pp_i_eq,
	pp_ne,
	pp_i_ne,
	pp_ncmp,
	pp_i_ncmp,
	pp_slt,
	pp_sgt,
	pp_sle,
	pp_sge,
	pp_seq,
	pp_sne,
	pp_scmp,
	pp_bit_and,
	pp_bit_xor,
	pp_bit_or,
	pp_negate,
	pp_i_negate,
	pp_not,
	pp_complement,
	pp_atan2,
	pp_sin,
	pp_cos,
	pp_rand,
	pp_srand,
	pp_exp,
	pp_log,
	pp_sqrt,
	pp_int,
	pp_hex,
	pp_oct,
	pp_abs,
	pp_length,
	pp_substr,
	pp_vec,
	pp_index,
	pp_rindex,
	pp_sprintf,
	pp_formline,
	pp_ord,
	pp_chr,
	pp_crypt,
	pp_ucfirst,
	pp_lcfirst,
	pp_uc,
	pp_lc,
	pp_quotemeta,
	pp_rv2av,
	pp_aelemfast,
	pp_aelem,
	pp_aslice,
	pp_each,
	pp_values,
	pp_keys,
	pp_delete,
	pp_exists,
	pp_rv2hv,
	pp_helem,
	pp_hslice,
	pp_unpack,
	pp_pack,
	pp_split,
	pp_join,
	pp_list,
	pp_lslice,
	pp_anonlist,
	pp_anonhash,
	pp_splice,
	pp_push,
	pp_pop,
	pp_shift,
	pp_unshift,
	pp_sort,
	pp_reverse,
	pp_grepstart,
	pp_grepwhile,
	pp_mapstart,
	pp_mapwhile,
	pp_range,
	pp_flip,
	pp_flop,
	pp_and,
	pp_or,
	pp_xor,
	pp_cond_expr,
	pp_andassign,
	pp_orassign,
	pp_method,
	pp_entersub,
	pp_leavesub,
	pp_caller,
	pp_warn,
	pp_die,
	pp_reset,
	pp_lineseq,
	pp_nextstate,
	pp_dbstate,
	pp_unstack,
	pp_enter,
	pp_leave,
	pp_scope,
	pp_enteriter,
	pp_iter,
	pp_enterloop,
	pp_leaveloop,
	pp_return,
	pp_last,
	pp_next,
	pp_redo,
	pp_dump,
	pp_goto,
	pp_exit,
	pp_open,
	pp_close,
	pp_pipe_op,
	pp_fileno,
	pp_umask,
	pp_binmode,
	pp_tie,
	pp_untie,
	pp_tied,
	pp_dbmopen,
	pp_dbmclose,
	pp_sselect,
	pp_select,
	pp_getc,
	pp_read,
	pp_enterwrite,
	pp_leavewrite,
	pp_prtf,
	pp_print,
	pp_sysopen,
	pp_sysread,
	pp_syswrite,
	pp_send,
	pp_recv,
	pp_eof,
	pp_tell,
	pp_seek,
	pp_truncate,
	pp_fcntl,
	pp_ioctl,
	pp_flock,
	pp_socket,
	pp_sockpair,
	pp_bind,
	pp_connect,
	pp_listen,
	pp_accept,
	pp_shutdown,
	pp_gsockopt,
	pp_ssockopt,
	pp_getsockname,
	pp_getpeername,
	pp_lstat,
	pp_stat,
	pp_ftrread,
	pp_ftrwrite,
	pp_ftrexec,
	pp_fteread,
	pp_ftewrite,
	pp_fteexec,
	pp_ftis,
	pp_fteowned,
	pp_ftrowned,
	pp_ftzero,
	pp_ftsize,
	pp_ftmtime,
	pp_ftatime,
	pp_ftctime,
	pp_ftsock,
	pp_ftchr,
	pp_ftblk,
	pp_ftfile,
	pp_ftdir,
	pp_ftpipe,
	pp_ftlink,
	pp_ftsuid,
	pp_ftsgid,
	pp_ftsvtx,
	pp_fttty,
	pp_fttext,
	pp_ftbinary,
	pp_chdir,
	pp_chown,
	pp_chroot,
	pp_unlink,
	pp_chmod,
	pp_utime,
	pp_rename,
	pp_link,
	pp_symlink,
	pp_readlink,
	pp_mkdir,
	pp_rmdir,
	pp_open_dir,
	pp_readdir,
	pp_telldir,
	pp_seekdir,
	pp_rewinddir,
	pp_closedir,
	pp_fork,
	pp_wait,
	pp_waitpid,
	pp_system,
	pp_exec,
	pp_kill,
	pp_getppid,
	pp_getpgrp,
	pp_setpgrp,
	pp_getpriority,
	pp_setpriority,
	pp_time,
	pp_tms,
	pp_localtime,
	pp_gmtime,
	pp_alarm,
	pp_sleep,
	pp_shmget,
	pp_shmctl,
	pp_shmread,
	pp_shmwrite,
	pp_msgget,
	pp_msgctl,
	pp_msgsnd,
	pp_msgrcv,
	pp_semget,
	pp_semctl,
	pp_semop,
	pp_require,
	pp_dofile,
	pp_entereval,
	pp_leaveeval,
	pp_entertry,
	pp_leavetry,
	pp_ghbyname,
	pp_ghbyaddr,
	pp_ghostent,
	pp_gnbyname,
	pp_gnbyaddr,
	pp_gnetent,
	pp_gpbyname,
	pp_gpbynumber,
	pp_gprotoent,
	pp_gsbyname,
	pp_gsbyport,
	pp_gservent,
	pp_shostent,
	pp_snetent,
	pp_sprotoent,
	pp_sservent,
	pp_ehostent,
	pp_enetent,
	pp_eprotoent,
	pp_eservent,
	pp_gpwnam,
	pp_gpwuid,
	pp_gpwent,
	pp_spwent,
	pp_epwent,
	pp_ggrnam,
	pp_ggrgid,
	pp_ggrent,
	pp_sgrent,
	pp_egrent,
	pp_getlogin,
	pp_syscall,
};
#endif

#ifndef DOINIT
EXT OP * (*check[])();
#else
EXT OP * (*check[])() = {
	ck_null,	/* null */
	ck_null,	/* stub */
	ck_fun,		/* scalar */
	ck_null,	/* pushmark */
	ck_null,	/* wantarray */
	ck_svconst,	/* const */
	ck_null,	/* gvsv */
	ck_null,	/* gv */
	ck_null,	/* gelem */
	ck_null,	/* padsv */
	ck_null,	/* padav */
	ck_null,	/* padhv */
	ck_null,	/* padany */
	ck_null,	/* pushre */
	ck_rvconst,	/* rv2gv */
	ck_rvconst,	/* rv2sv */
	ck_null,	/* av2arylen */
	ck_rvconst,	/* rv2cv */
	ck_null,	/* anoncode */
	ck_null,	/* prototype */
	ck_spair,	/* refgen */
	ck_null,	/* srefgen */
	ck_fun,		/* ref */
	ck_fun,		/* bless */
	ck_null,	/* backtick */
	ck_glob,	/* glob */
	ck_null,	/* readline */
	ck_null,	/* rcatline */
	ck_fun,		/* regcmaybe */
	ck_null,	/* regcomp */
	ck_match,	/* match */
	ck_null,	/* subst */
	ck_null,	/* substcont */
	ck_null,	/* trans */
	ck_null,	/* sassign */
	ck_null,	/* aassign */
	ck_spair,	/* chop */
	ck_null,	/* schop */
	ck_spair,	/* chomp */
	ck_null,	/* schomp */
	ck_rfun,	/* defined */
	ck_lfun,	/* undef */
	ck_fun,		/* study */
	ck_lfun,	/* pos */
	ck_lfun,	/* preinc */
	ck_lfun,	/* i_preinc */
	ck_lfun,	/* predec */
	ck_lfun,	/* i_predec */
	ck_lfun,	/* postinc */
	ck_lfun,	/* i_postinc */
	ck_lfun,	/* postdec */
	ck_lfun,	/* i_postdec */
	ck_null,	/* pow */
	ck_null,	/* multiply */
	ck_null,	/* i_multiply */
	ck_null,	/* divide */
	ck_null,	/* i_divide */
	ck_null,	/* modulo */
	ck_null,	/* i_modulo */
	ck_repeat,	/* repeat */
	ck_null,	/* add */
	ck_null,	/* i_add */
	ck_null,	/* subtract */
	ck_null,	/* i_subtract */
	ck_concat,	/* concat */
	ck_fun,		/* stringify */
	ck_null,	/* left_shift */
	ck_null,	/* right_shift */
	ck_null,	/* lt */
	ck_null,	/* i_lt */
	ck_null,	/* gt */
	ck_null,	/* i_gt */
	ck_null,	/* le */
	ck_null,	/* i_le */
	ck_null,	/* ge */
	ck_null,	/* i_ge */
	ck_null,	/* eq */
	ck_null,	/* i_eq */
	ck_null,	/* ne */
	ck_null,	/* i_ne */
	ck_null,	/* ncmp */
	ck_null,	/* i_ncmp */
	ck_null,	/* slt */
	ck_null,	/* sgt */
	ck_null,	/* sle */
	ck_null,	/* sge */
	ck_null,	/* seq */
	ck_null,	/* sne */
	ck_null,	/* scmp */
	ck_null,	/* bit_and */
	ck_null,	/* bit_xor */
	ck_null,	/* bit_or */
	ck_null,	/* negate */
	ck_null,	/* i_negate */
	ck_null,	/* not */
	ck_null,	/* complement */
	ck_fun,		/* atan2 */
	ck_fun,		/* sin */
	ck_fun,		/* cos */
	ck_fun,		/* rand */
	ck_fun,		/* srand */
	ck_fun,		/* exp */
	ck_fun,		/* log */
	ck_fun,		/* sqrt */
	ck_fun,		/* int */
	ck_fun,		/* hex */
	ck_fun,		/* oct */
	ck_fun,		/* abs */
	ck_lengthconst,	/* length */
	ck_fun,		/* substr */
	ck_fun,		/* vec */
	ck_index,	/* index */
	ck_index,	/* rindex */
	ck_fun,		/* sprintf */
	ck_formline,	/* formline */
	ck_fun,		/* ord */
	ck_fun,		/* chr */
	ck_fun,		/* crypt */
	ck_fun,		/* ucfirst */
	ck_fun,		/* lcfirst */
	ck_fun,		/* uc */
	ck_fun,		/* lc */
	ck_fun,		/* quotemeta */
	ck_rvconst,	/* rv2av */
	ck_null,	/* aelemfast */
	ck_null,	/* aelem */
	ck_null,	/* aslice */
	ck_fun,		/* each */
	ck_fun,		/* values */
	ck_fun,		/* keys */
	ck_delete,	/* delete */
	ck_delete,	/* exists */
	ck_rvconst,	/* rv2hv */
	ck_null,	/* helem */
	ck_null,	/* hslice */
	ck_fun,		/* unpack */
	ck_fun,		/* pack */
	ck_split,	/* split */
	ck_fun,		/* join */
	ck_null,	/* list */
	ck_null,	/* lslice */
	ck_fun,		/* anonlist */
	ck_fun,		/* anonhash */
	ck_fun,		/* splice */
	ck_fun,		/* push */
	ck_shift,	/* pop */
	ck_shift,	/* shift */
	ck_fun,		/* unshift */
	ck_sort,	/* sort */
	ck_fun,		/* reverse */
	ck_grep,	/* grepstart */
	ck_null,	/* grepwhile */
	ck_grep,	/* mapstart */
	ck_null,	/* mapwhile */
	ck_null,	/* range */
	ck_null,	/* flip */
	ck_null,	/* flop */
	ck_null,	/* and */
	ck_null,	/* or */
	ck_null,	/* xor */
	ck_null,	/* cond_expr */
	ck_null,	/* andassign */
	ck_null,	/* orassign */
	ck_null,	/* method */
	ck_subr,	/* entersub */
	ck_null,	/* leavesub */
	ck_fun,		/* caller */
	ck_fun,		/* warn */
	ck_fun,		/* die */
	ck_fun,		/* reset */
	ck_null,	/* lineseq */
	ck_null,	/* nextstate */
	ck_null,	/* dbstate */
	ck_null,	/* unstack */
	ck_null,	/* enter */
	ck_null,	/* leave */
	ck_null,	/* scope */
	ck_null,	/* enteriter */
	ck_null,	/* iter */
	ck_null,	/* enterloop */
	ck_null,	/* leaveloop */
	ck_null,	/* return */
	ck_null,	/* last */
	ck_null,	/* next */
	ck_null,	/* redo */
	ck_null,	/* dump */
	ck_null,	/* goto */
	ck_fun,		/* exit */
	ck_fun,		/* open */
	ck_fun,		/* close */
	ck_fun,		/* pipe_op */
	ck_fun,		/* fileno */
	ck_fun,		/* umask */
	ck_fun,		/* binmode */
	ck_fun,		/* tie */
	ck_fun,		/* untie */
	ck_fun,		/* tied */
	ck_fun,		/* dbmopen */
	ck_fun,		/* dbmclose */
	ck_select,	/* sselect */
	ck_select,	/* select */
	ck_eof,		/* getc */
	ck_fun,		/* read */
	ck_fun,		/* enterwrite */
	ck_null,	/* leavewrite */
	ck_listiob,	/* prtf */
	ck_listiob,	/* print */
	ck_fun,		/* sysopen */
	ck_fun,		/* sysread */
	ck_fun,		/* syswrite */
	ck_fun,		/* send */
	ck_fun,		/* recv */
	ck_eof,		/* eof */
	ck_fun,		/* tell */
	ck_fun,		/* seek */
	ck_trunc,	/* truncate */
	ck_fun,		/* fcntl */
	ck_fun,		/* ioctl */
	ck_fun,		/* flock */
	ck_fun,		/* socket */
	ck_fun,		/* sockpair */
	ck_fun,		/* bind */
	ck_fun,		/* connect */
	ck_fun,		/* listen */
	ck_fun,		/* accept */
	ck_fun,		/* shutdown */
	ck_fun,		/* gsockopt */
	ck_fun,		/* ssockopt */
	ck_fun,		/* getsockname */
	ck_fun,		/* getpeername */
	ck_ftst,	/* lstat */
	ck_ftst,	/* stat */
	ck_ftst,	/* ftrread */
	ck_ftst,	/* ftrwrite */
	ck_ftst,	/* ftrexec */
	ck_ftst,	/* fteread */
	ck_ftst,	/* ftewrite */
	ck_ftst,	/* fteexec */
	ck_ftst,	/* ftis */
	ck_ftst,	/* fteowned */
	ck_ftst,	/* ftrowned */
	ck_ftst,	/* ftzero */
	ck_ftst,	/* ftsize */
	ck_ftst,	/* ftmtime */
	ck_ftst,	/* ftatime */
	ck_ftst,	/* ftctime */
	ck_ftst,	/* ftsock */
	ck_ftst,	/* ftchr */
	ck_ftst,	/* ftblk */
	ck_ftst,	/* ftfile */
	ck_ftst,	/* ftdir */
	ck_ftst,	/* ftpipe */
	ck_ftst,	/* ftlink */
	ck_ftst,	/* ftsuid */
	ck_ftst,	/* ftsgid */
	ck_ftst,	/* ftsvtx */
	ck_ftst,	/* fttty */
	ck_ftst,	/* fttext */
	ck_ftst,	/* ftbinary */
	ck_fun,		/* chdir */
	ck_fun,		/* chown */
	ck_fun,		/* chroot */
	ck_fun,		/* unlink */
	ck_fun,		/* chmod */
	ck_fun,		/* utime */
	ck_fun,		/* rename */
	ck_fun,		/* link */
	ck_fun,		/* symlink */
	ck_fun,		/* readlink */
	ck_fun,		/* mkdir */
	ck_fun,		/* rmdir */
	ck_fun,		/* open_dir */
	ck_fun,		/* readdir */
	ck_fun,		/* telldir */
	ck_fun,		/* seekdir */
	ck_fun,		/* rewinddir */
	ck_fun,		/* closedir */
	ck_null,	/* fork */
	ck_null,	/* wait */
	ck_fun,		/* waitpid */
	ck_exec,	/* system */
	ck_exec,	/* exec */
	ck_fun,		/* kill */
	ck_null,	/* getppid */
	ck_fun,		/* getpgrp */
	ck_fun,		/* setpgrp */
	ck_fun,		/* getpriority */
	ck_fun,		/* setpriority */
	ck_null,	/* time */
	ck_null,	/* tms */
	ck_fun,		/* localtime */
	ck_fun,		/* gmtime */
	ck_fun,		/* alarm */
	ck_fun,		/* sleep */
	ck_fun,		/* shmget */
	ck_fun,		/* shmctl */
	ck_fun,		/* shmread */
	ck_fun,		/* shmwrite */
	ck_fun,		/* msgget */
	ck_fun,		/* msgctl */
	ck_fun,		/* msgsnd */
	ck_fun,		/* msgrcv */
	ck_fun,		/* semget */
	ck_fun,		/* semctl */
	ck_fun,		/* semop */
	ck_require,	/* require */
	ck_fun,		/* dofile */
	ck_eval,	/* entereval */
	ck_null,	/* leaveeval */
	ck_null,	/* entertry */
	ck_null,	/* leavetry */
	ck_fun,		/* ghbyname */
	ck_fun,		/* ghbyaddr */
	ck_null,	/* ghostent */
	ck_fun,		/* gnbyname */
	ck_fun,		/* gnbyaddr */
	ck_null,	/* gnetent */
	ck_fun,		/* gpbyname */
	ck_fun,		/* gpbynumber */
	ck_null,	/* gprotoent */
	ck_fun,		/* gsbyname */
	ck_fun,		/* gsbyport */
	ck_null,	/* gservent */
	ck_fun,		/* shostent */
	ck_fun,		/* snetent */
	ck_fun,		/* sprotoent */
	ck_fun,		/* sservent */
	ck_null,	/* ehostent */
	ck_null,	/* enetent */
	ck_null,	/* eprotoent */
	ck_null,	/* eservent */
	ck_fun,		/* gpwnam */
	ck_fun,		/* gpwuid */
	ck_null,	/* gpwent */
	ck_null,	/* spwent */
	ck_null,	/* epwent */
	ck_fun,		/* ggrnam */
	ck_fun,		/* ggrgid */
	ck_null,	/* ggrent */
	ck_null,	/* sgrent */
	ck_null,	/* egrent */
	ck_null,	/* getlogin */
	ck_fun,		/* syscall */
};
#endif

#ifndef DOINIT
EXT U32 opargs[];
#else
EXT U32 opargs[] = {
	0x00000000,	/* null */
	0x00000000,	/* stub */
	0x00000104,	/* scalar */
	0x00000004,	/* pushmark */
	0x00000014,	/* wantarray */
	0x00000004,	/* const */
	0x00000044,	/* gvsv */
	0x00000044,	/* gv */
	0x00001140,	/* gelem */
	0x00000044,	/* padsv */
	0x00000040,	/* padav */
	0x00000040,	/* padhv */
	0x00000040,	/* padany */
	0x00000000,	/* pushre */
	0x00000044,	/* rv2gv */
	0x00000044,	/* rv2sv */
	0x00000014,	/* av2arylen */
	0x00000040,	/* rv2cv */
	0x00000000,	/* anoncode */
	0x00000104,	/* prototype */
	0x00000201,	/* refgen */
	0x00000106,	/* srefgen */
	0x0000098c,	/* ref */
	0x00009104,	/* bless */
	0x00000008,	/* backtick */
	0x00001108,	/* glob */
	0x00000008,	/* readline */
	0x00000008,	/* rcatline */
	0x00000104,	/* regcmaybe */
	0x00000104,	/* regcomp */
	0x00000040,	/* match */
	0x00000154,	/* subst */
	0x00000054,	/* substcont */
	0x00000114,	/* trans */
	0x00000004,	/* sassign */
	0x00002208,	/* aassign */
	0x0000020d,	/* chop */
	0x0000098c,	/* schop */
	0x0000020d,	/* chomp */
	0x0000098c,	/* schomp */
	0x00000994,	/* defined */
	0x00000904,	/* undef */
	0x00000984,	/* study */
	0x0000098c,	/* pos */
	0x00000164,	/* preinc */
	0x00000154,	/* i_preinc */
	0x00000164,	/* predec */
	0x00000154,	/* i_predec */
	0x0000016c,	/* postinc */
	0x0000015c,	/* i_postinc */
	0x0000016c,	/* postdec */
	0x0000015c,	/* i_postdec */
	0x0000110e,	/* pow */
	0x0000112e,	/* multiply */
	0x0000111e,	/* i_multiply */
	0x0000112e,	/* divide */
	0x0000111e,	/* i_divide */
	0x0000113e,	/* modulo */
	0x0000111e,	/* i_modulo */
	0x00001209,	/* repeat */
	0x0000112e,	/* add */
	0x0000111e,	/* i_add */
	0x0000112e,	/* subtract */
	0x0000111e,	/* i_subtract */
	0x0000110e,	/* concat */
	0x0000010e,	/* stringify */
	0x0000111e,	/* left_shift */
	0x0000111e,	/* right_shift */
	0x00001136,	/* lt */
	0x00001116,	/* i_lt */
	0x00001136,	/* gt */
	0x00001116,	/* i_gt */
	0x00001136,	/* le */
	0x00001116,	/* i_le */
	0x00001136,	/* ge */
	0x00001116,	/* i_ge */
	0x00001136,	/* eq */
	0x00001116,	/* i_eq */
	0x00001136,	/* ne */
	0x00001116,	/* i_ne */
	0x0000113e,	/* ncmp */
	0x0000111e,	/* i_ncmp */
	0x00001116,	/* slt */
	0x00001116,	/* sgt */
	0x00001116,	/* sle */
	0x00001116,	/* sge */
	0x00001116,	/* seq */
	0x00001116,	/* sne */
	0x0000111e,	/* scmp */
	0x0000110e,	/* bit_and */
	0x0000110e,	/* bit_xor */
	0x0000110e,	/* bit_or */
	0x0000012e,	/* negate */
	0x0000011e,	/* i_negate */
	0x00000116,	/* not */
	0x0000010e,	/* complement */
	0x0000110e,	/* atan2 */
	0x0000098e,	/* sin */
	0x0000098e,	/* cos */
	0x0000090c,	/* rand */
	0x00000904,	/* srand */
	0x0000098e,	/* exp */
	0x0000098e,	/* log */
	0x0000098e,	/* sqrt */
	0x0000098e,	/* int */
	0x0000099c,	/* hex */
	0x0000099c,	/* oct */
	0x0000098e,	/* abs */
	0x0000099c,	/* length */
	0x0009110c,	/* substr */
	0x0001111c,	/* vec */
	0x0009111c,	/* index */
	0x0009111c,	/* rindex */
	0x0000210d,	/* sprintf */
	0x00002105,	/* formline */
	0x0000099e,	/* ord */
	0x0000098e,	/* chr */
	0x0000110e,	/* crypt */
	0x0000010e,	/* ucfirst */
	0x0000010e,	/* lcfirst */
	0x0000010e,	/* uc */
	0x0000010e,	/* lc */
	0x0000010e,	/* quotemeta */
	0x00000048,	/* rv2av */
	0x00001304,	/* aelemfast */
	0x00001304,	/* aelem */
	0x00002301,	/* aslice */
	0x00000408,	/* each */
	0x00000408,	/* values */
	0x00000408,	/* keys */
	0x00000104,	/* delete */
	0x00000114,	/* exists */
	0x00000048,	/* rv2hv */
	0x00001404,	/* helem */
	0x00002401,	/* hslice */
	0x00001100,	/* unpack */
	0x0000210d,	/* pack */
	0x00011108,	/* split */
	0x0000210d,	/* join */
	0x00000201,	/* list */
	0x00022400,	/* lslice */
	0x00000205,	/* anonlist */
	0x00000205,	/* anonhash */
	0x00299301,	/* splice */
	0x0000231d,	/* push */
	0x00000304,	/* pop */
	0x00000304,	/* shift */
	0x0000231d,	/* unshift */
	0x00002d01,	/* sort */
	0x00000209,	/* reverse */
	0x00002541,	/* grepstart */
	0x00000048,	/* grepwhile */
	0x00002541,	/* mapstart */
	0x00000048,	/* mapwhile */
	0x00001100,	/* range */
	0x00001100,	/* flip */
	0x00000000,	/* flop */
	0x00000000,	/* and */
	0x00000000,	/* or */
	0x00001106,	/* xor */
	0x00000040,	/* cond_expr */
	0x00000004,	/* andassign */
	0x00000004,	/* orassign */
	0x00000040,	/* method */
	0x00000249,	/* entersub */
	0x00000000,	/* leavesub */
	0x00000908,	/* caller */
	0x0000021d,	/* warn */
	0x0000025d,	/* die */
	0x00000914,	/* reset */
	0x00000000,	/* lineseq */
	0x00000004,	/* nextstate */
	0x00000004,	/* dbstate */
	0x00000004,	/* unstack */
	0x00000000,	/* enter */
	0x00000000,	/* leave */
	0x00000000,	/* scope */
	0x00000040,	/* enteriter */
	0x00000000,	/* iter */
	0x00000040,	/* enterloop */
	0x00000000,	/* leaveloop */
	0x00000241,	/* return */
	0x00000044,	/* last */
	0x00000044,	/* next */
	0x00000044,	/* redo */
	0x00000044,	/* dump */
	0x00000044,	/* goto */
	0x00000944,	/* exit */
	0x0000961c,	/* open */
	0x00000e14,	/* close */
	0x00006614,	/* pipe_op */
	0x0000061c,	/* fileno */
	0x0000091c,	/* umask */
	0x00000604,	/* binmode */
	0x00021755,	/* tie */
	0x00000714,	/* untie */
	0x00000704,	/* tied */
	0x00011414,	/* dbmopen */
	0x00000414,	/* dbmclose */
	0x00111108,	/* sselect */
	0x00000e0c,	/* select */
	0x00000e0c,	/* getc */
	0x0091761d,	/* read */
	0x00000e54,	/* enterwrite */
	0x00000000,	/* leavewrite */
	0x00002e15,	/* prtf */
	0x00002e15,	/* print */
	0x00911604,	/* sysopen */
	0x0091761d,	/* sysread */
	0x0091161d,	/* syswrite */
	0x0091161d,	/* send */
	0x0011761d,	/* recv */
	0x00000e14,	/* eof */
	0x00000e0c,	/* tell */
	0x00011604,	/* seek */
	0x00001114,	/* truncate */
	0x0001160c,	/* fcntl */
	0x0001160c,	/* ioctl */
	0x0000161c,	/* flock */
	0x00111614,	/* socket */
	0x01116614,	/* sockpair */
	0x00001614,	/* bind */
	0x00001614,	/* connect */
	0x00001614,	/* listen */
	0x0000661c,	/* accept */
	0x0000161c,	/* shutdown */
	0x00011614,	/* gsockopt */
	0x00111614,	/* ssockopt */
	0x00000614,	/* getsockname */
	0x00000614,	/* getpeername */
	0x00000680,	/* lstat */
	0x00000680,	/* stat */
	0x00000694,	/* ftrread */
	0x00000694,	/* ftrwrite */
	0x00000694,	/* ftrexec */
	0x00000694,	/* fteread */
	0x00000694,	/* ftewrite */
	0x00000694,	/* fteexec */
	0x00000694,	/* ftis */
	0x00000694,	/* fteowned */
	0x00000694,	/* ftrowned */
	0x00000694,	/* ftzero */
	0x0000069c,	/* ftsize */
	0x0000068c,	/* ftmtime */
	0x0000068c,	/* ftatime */
	0x0000068c,	/* ftctime */
	0x00000694,	/* ftsock */
	0x00000694,	/* ftchr */
	0x00000694,	/* ftblk */
	0x00000694,	/* ftfile */
	0x00000694,	/* ftdir */
	0x00000694,	/* ftpipe */
	0x00000694,	/* ftlink */
	0x00000694,	/* ftsuid */
	0x00000694,	/* ftsgid */
	0x00000694,	/* ftsvtx */
	0x00000614,	/* fttty */
	0x00000694,	/* fttext */
	0x00000694,	/* ftbinary */
	0x0000091c,	/* chdir */
	0x0000021d,	/* chown */
	0x0000099c,	/* chroot */
	0x0000029d,	/* unlink */
	0x0000021d,	/* chmod */
	0x0000021d,	/* utime */
	0x0000111c,	/* rename */
	0x0000111c,	/* link */
	0x0000111c,	/* symlink */
	0x0000098c,	/* readlink */
	0x0000111c,	/* mkdir */
	0x0000099c,	/* rmdir */
	0x00001614,	/* open_dir */
	0x00000600,	/* readdir */
	0x0000060c,	/* telldir */
	0x00001604,	/* seekdir */
	0x00000604,	/* rewinddir */
	0x00000614,	/* closedir */
	0x0000001c,	/* fork */
	0x0000001c,	/* wait */
	0x0000111c,	/* waitpid */
	0x0000291d,	/* system */
	0x0000295d,	/* exec */
	0x0000025d,	/* kill */
	0x0000001c,	/* getppid */
	0x0000091c,	/* getpgrp */
	0x0000991c,	/* setpgrp */
	0x0000111c,	/* getpriority */
	0x0001111c,	/* setpriority */
	0x0000001c,	/* time */
	0x00000000,	/* tms */
	0x00000908,	/* localtime */
	0x00000908,	/* gmtime */
	0x0000099c,	/* alarm */
	0x0000091c,	/* sleep */
	0x0001111d,	/* shmget */
	0x0001111d,	/* shmctl */
	0x0011111d,	/* shmread */
	0x0011111d,	/* shmwrite */
	0x0000111d,	/* msgget */
	0x0001111d,	/* msgctl */
	0x0001111d,	/* msgsnd */
	0x0111111d,	/* msgrcv */
	0x0001111d,	/* semget */
	0x0011111d,	/* semctl */
	0x0000111d,	/* semop */
	0x000009c0,	/* require */
	0x00000140,	/* dofile */
	0x00000140,	/* entereval */
	0x00000100,	/* leaveeval */
	0x00000000,	/* entertry */
	0x00000000,	/* leavetry */
	0x00000100,	/* ghbyname */
	0x00001100,	/* ghbyaddr */
	0x00000000,	/* ghostent */
	0x00000100,	/* gnbyname */
	0x00001100,	/* gnbyaddr */
	0x00000000,	/* gnetent */
	0x00000100,	/* gpbyname */
	0x00000100,	/* gpbynumber */
	0x00000000,	/* gprotoent */
	0x00001100,	/* gsbyname */
	0x00001100,	/* gsbyport */
	0x00000000,	/* gservent */
	0x00000114,	/* shostent */
	0x00000114,	/* snetent */
	0x00000114,	/* sprotoent */
	0x00000114,	/* sservent */
	0x00000014,	/* ehostent */
	0x00000014,	/* enetent */
	0x00000014,	/* eprotoent */
	0x00000014,	/* eservent */
	0x00000100,	/* gpwnam */
	0x00000100,	/* gpwuid */
	0x00000000,	/* gpwent */
	0x00000014,	/* spwent */
	0x00000014,	/* epwent */
	0x00000100,	/* ggrnam */
	0x00000100,	/* ggrgid */
	0x00000000,	/* ggrent */
	0x00000014,	/* sgrent */
	0x00000014,	/* egrent */
	0x0000000c,	/* getlogin */
	0x0000211d,	/* syscall */
};
#endif
