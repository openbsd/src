#include <stdio.h>
#include "awk.h"
#include "ytab.h"

static char *printname[93] = {
	(char *) "FIRSTTOKEN",	/* 257 */
	(char *) "PROGRAM",	/* 258 */
	(char *) "PASTAT",	/* 259 */
	(char *) "PASTAT2",	/* 260 */
	(char *) "XBEGIN",	/* 261 */
	(char *) "XEND",	/* 262 */
	(char *) "NL",	/* 263 */
	(char *) "ARRAY",	/* 264 */
	(char *) "MATCH",	/* 265 */
	(char *) "NOTMATCH",	/* 266 */
	(char *) "MATCHOP",	/* 267 */
	(char *) "FINAL",	/* 268 */
	(char *) "DOT",	/* 269 */
	(char *) "ALL",	/* 270 */
	(char *) "CCL",	/* 271 */
	(char *) "NCCL",	/* 272 */
	(char *) "CHAR",	/* 273 */
	(char *) "OR",	/* 274 */
	(char *) "STAR",	/* 275 */
	(char *) "QUEST",	/* 276 */
	(char *) "PLUS",	/* 277 */
	(char *) "AND",	/* 278 */
	(char *) "BOR",	/* 279 */
	(char *) "APPEND",	/* 280 */
	(char *) "EQ",	/* 281 */
	(char *) "GE",	/* 282 */
	(char *) "GT",	/* 283 */
	(char *) "LE",	/* 284 */
	(char *) "LT",	/* 285 */
	(char *) "NE",	/* 286 */
	(char *) "IN",	/* 287 */
	(char *) "ARG",	/* 288 */
	(char *) "BLTIN",	/* 289 */
	(char *) "BREAK",	/* 290 */
	(char *) "CLOSE",	/* 291 */
	(char *) "CONTINUE",	/* 292 */
	(char *) "DELETE",	/* 293 */
	(char *) "DO",	/* 294 */
	(char *) "EXIT",	/* 295 */
	(char *) "FOR",	/* 296 */
	(char *) "FUNC",	/* 297 */
	(char *) "SUB",	/* 298 */
	(char *) "GSUB",	/* 299 */
	(char *) "IF",	/* 300 */
	(char *) "INDEX",	/* 301 */
	(char *) "LSUBSTR",	/* 302 */
	(char *) "MATCHFCN",	/* 303 */
	(char *) "NEXT",	/* 304 */
	(char *) "NEXTFILE",	/* 305 */
	(char *) "ADD",	/* 306 */
	(char *) "MINUS",	/* 307 */
	(char *) "MULT",	/* 308 */
	(char *) "DIVIDE",	/* 309 */
	(char *) "MOD",	/* 310 */
	(char *) "ASSIGN",	/* 311 */
	(char *) "ASGNOP",	/* 312 */
	(char *) "ADDEQ",	/* 313 */
	(char *) "SUBEQ",	/* 314 */
	(char *) "MULTEQ",	/* 315 */
	(char *) "DIVEQ",	/* 316 */
	(char *) "MODEQ",	/* 317 */
	(char *) "POWEQ",	/* 318 */
	(char *) "PRINT",	/* 319 */
	(char *) "PRINTF",	/* 320 */
	(char *) "SPRINTF",	/* 321 */
	(char *) "ELSE",	/* 322 */
	(char *) "INTEST",	/* 323 */
	(char *) "CONDEXPR",	/* 324 */
	(char *) "POSTINCR",	/* 325 */
	(char *) "PREINCR",	/* 326 */
	(char *) "POSTDECR",	/* 327 */
	(char *) "PREDECR",	/* 328 */
	(char *) "VAR",	/* 329 */
	(char *) "IVAR",	/* 330 */
	(char *) "VARNF",	/* 331 */
	(char *) "CALL",	/* 332 */
	(char *) "NUMBER",	/* 333 */
	(char *) "STRING",	/* 334 */
	(char *) "FIELD",	/* 335 */
	(char *) "REGEXPR",	/* 336 */
	(char *) "GETLINE",	/* 337 */
	(char *) "RETURN",	/* 338 */
	(char *) "SPLIT",	/* 339 */
	(char *) "SUBSTR",	/* 340 */
	(char *) "WHILE",	/* 341 */
	(char *) "CAT",	/* 342 */
	(char *) "NOT",	/* 343 */
	(char *) "UMINUS",	/* 344 */
	(char *) "POWER",	/* 345 */
	(char *) "DECR",	/* 346 */
	(char *) "INCR",	/* 347 */
	(char *) "INDIRECT",	/* 348 */
	(char *) "LASTTOKEN",	/* 349 */
};


Cell *(*proctab[93])(Node **, int) = {
	nullproc,	/* FIRSTTOKEN */
	program,	/* PROGRAM */
	pastat,	/* PASTAT */
	dopa2,	/* PASTAT2 */
	nullproc,	/* XBEGIN */
	nullproc,	/* XEND */
	nullproc,	/* NL */
	array,	/* ARRAY */
	matchop,	/* MATCH */
	matchop,	/* NOTMATCH */
	nullproc,	/* MATCHOP */
	nullproc,	/* FINAL */
	nullproc,	/* DOT */
	nullproc,	/* ALL */
	nullproc,	/* CCL */
	nullproc,	/* NCCL */
	nullproc,	/* CHAR */
	nullproc,	/* OR */
	nullproc,	/* STAR */
	nullproc,	/* QUEST */
	nullproc,	/* PLUS */
	boolop,	/* AND */
	boolop,	/* BOR */
	nullproc,	/* APPEND */
	relop,	/* EQ */
	relop,	/* GE */
	relop,	/* GT */
	relop,	/* LE */
	relop,	/* LT */
	relop,	/* NE */
	instat,	/* IN */
	arg,	/* ARG */
	bltin,	/* BLTIN */
	jump,	/* BREAK */
	closefile,	/* CLOSE */
	jump,	/* CONTINUE */
	adelete,	/* DELETE */
	dostat,	/* DO */
	jump,	/* EXIT */
	forstat,	/* FOR */
	nullproc,	/* FUNC */
	sub,	/* SUB */
	gsub,	/* GSUB */
	ifstat,	/* IF */
	sindex,	/* INDEX */
	nullproc,	/* LSUBSTR */
	matchop,	/* MATCHFCN */
	jump,	/* NEXT */
	jump,	/* NEXTFILE */
	arith,	/* ADD */
	arith,	/* MINUS */
	arith,	/* MULT */
	arith,	/* DIVIDE */
	arith,	/* MOD */
	assign,	/* ASSIGN */
	nullproc,	/* ASGNOP */
	assign,	/* ADDEQ */
	assign,	/* SUBEQ */
	assign,	/* MULTEQ */
	assign,	/* DIVEQ */
	assign,	/* MODEQ */
	assign,	/* POWEQ */
	printstat,	/* PRINT */
	awkprintf,	/* PRINTF */
	awksprintf,	/* SPRINTF */
	nullproc,	/* ELSE */
	intest,	/* INTEST */
	condexpr,	/* CONDEXPR */
	incrdecr,	/* POSTINCR */
	incrdecr,	/* PREINCR */
	incrdecr,	/* POSTDECR */
	incrdecr,	/* PREDECR */
	nullproc,	/* VAR */
	nullproc,	/* IVAR */
	getnf,	/* VARNF */
	call,	/* CALL */
	nullproc,	/* NUMBER */
	nullproc,	/* STRING */
	nullproc,	/* FIELD */
	nullproc,	/* REGEXPR */
	getline,	/* GETLINE */
	jump,	/* RETURN */
	split,	/* SPLIT */
	substr,	/* SUBSTR */
	whilestat,	/* WHILE */
	cat,	/* CAT */
	boolop,	/* NOT */
	arith,	/* UMINUS */
	arith,	/* POWER */
	nullproc,	/* DECR */
	nullproc,	/* INCR */
	indirect,	/* INDIRECT */
	nullproc,	/* LASTTOKEN */
};

char *tokname(int n)
{
	static char buf[100];

	if (n < FIRSTTOKEN || n > LASTTOKEN) {
		sprintf(buf, "token %d", n);
		return buf;
	}
	return printname[n-FIRSTTOKEN];
}
