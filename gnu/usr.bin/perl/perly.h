#ifdef PERL_CORE

/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     WORD = 258,
     METHOD = 259,
     FUNCMETH = 260,
     THING = 261,
     PMFUNC = 262,
     PRIVATEREF = 263,
     FUNC0SUB = 264,
     UNIOPSUB = 265,
     LSTOPSUB = 266,
     PLUGEXPR = 267,
     PLUGSTMT = 268,
     LABEL = 269,
     FORMAT = 270,
     SUB = 271,
     ANONSUB = 272,
     PACKAGE = 273,
     USE = 274,
     WHILE = 275,
     UNTIL = 276,
     IF = 277,
     UNLESS = 278,
     ELSE = 279,
     ELSIF = 280,
     CONTINUE = 281,
     FOR = 282,
     GIVEN = 283,
     WHEN = 284,
     DEFAULT = 285,
     LOOPEX = 286,
     DOTDOT = 287,
     YADAYADA = 288,
     FUNC0 = 289,
     FUNC1 = 290,
     FUNC = 291,
     UNIOP = 292,
     LSTOP = 293,
     RELOP = 294,
     EQOP = 295,
     MULOP = 296,
     ADDOP = 297,
     DOLSHARP = 298,
     DO = 299,
     HASHBRACK = 300,
     NOAMP = 301,
     LOCAL = 302,
     MY = 303,
     MYSUB = 304,
     REQUIRE = 305,
     COLONATTR = 306,
     PREC_LOW = 307,
     DOROP = 308,
     OROP = 309,
     ANDOP = 310,
     NOTOP = 311,
     ASSIGNOP = 312,
     DORDOR = 313,
     OROR = 314,
     ANDAND = 315,
     BITOROP = 316,
     BITANDOP = 317,
     SHIFTOP = 318,
     MATCHOP = 319,
     REFGEN = 320,
     UMINUS = 321,
     POWOP = 322,
     POSTDEC = 323,
     POSTINC = 324,
     PREDEC = 325,
     PREINC = 326,
     ARROW = 327,
     PEG = 328
   };
#endif

/* Tokens.  */
#define WORD 258
#define METHOD 259
#define FUNCMETH 260
#define THING 261
#define PMFUNC 262
#define PRIVATEREF 263
#define FUNC0SUB 264
#define UNIOPSUB 265
#define LSTOPSUB 266
#define PLUGEXPR 267
#define PLUGSTMT 268
#define LABEL 269
#define FORMAT 270
#define SUB 271
#define ANONSUB 272
#define PACKAGE 273
#define USE 274
#define WHILE 275
#define UNTIL 276
#define IF 277
#define UNLESS 278
#define ELSE 279
#define ELSIF 280
#define CONTINUE 281
#define FOR 282
#define GIVEN 283
#define WHEN 284
#define DEFAULT 285
#define LOOPEX 286
#define DOTDOT 287
#define YADAYADA 288
#define FUNC0 289
#define FUNC1 290
#define FUNC 291
#define UNIOP 292
#define LSTOP 293
#define RELOP 294
#define EQOP 295
#define MULOP 296
#define ADDOP 297
#define DOLSHARP 298
#define DO 299
#define HASHBRACK 300
#define NOAMP 301
#define LOCAL 302
#define MY 303
#define MYSUB 304
#define REQUIRE 305
#define COLONATTR 306
#define PREC_LOW 307
#define DOROP 308
#define OROP 309
#define ANDOP 310
#define NOTOP 311
#define ASSIGNOP 312
#define DORDOR 313
#define OROR 314
#define ANDAND 315
#define BITOROP 316
#define BITANDOP 317
#define SHIFTOP 318
#define MATCHOP 319
#define REFGEN 320
#define UMINUS 321
#define POWOP 322
#define POSTDEC 323
#define POSTINC 324
#define PREDEC 325
#define PREINC 326
#define ARROW 327
#define PEG 328



#endif /* PERL_CORE */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */

    I32	ival; /* __DEFAULT__ (marker for regen_perly.pl;
				must always be 1st union member) */
    char *pval;
    OP *opval;
    GV *gvval;
#ifdef PERL_IN_MADLY_C
    TOKEN* p_tkval;
    TOKEN* i_tkval;
#else
    char *p_tkval;
    I32	i_tkval;
#endif
#ifdef PERL_MAD
    TOKEN* tkval;
#endif



/* Line 1676 of yacc.c  */
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




