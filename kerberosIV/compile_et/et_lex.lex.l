%{
  unsigned lineno = 1;
%}

PC	[^\"]
AN	[A-Z_a-z0-9]
%%

error_table	return ERROR_TABLE;
et		return ERROR_TABLE;
error_code	return ERROR_CODE_ENTRY;
ec		return ERROR_CODE_ENTRY;
end		return END;

[\t ]		;
\n		++lineno;

\"{PC}*\"	{ register char *p; yylval.dynstr = ds(yytext+1);
		  if (p=strrchr(yylval.dynstr, '"')) *p='\0';
		  return QUOTED_STRING;
		}

{AN}*	{ yylval.dynstr = ds(yytext); return STRING; }

#.*\n		++lineno;

.		{ return (*yytext); }
%%
#ifndef lint
static char rcsid_et_lex_lex_l[] = "$Id: et_lex.lex.l,v 1.1.1.1 1995/12/14 06:52:49 tholo Exp $";
#endif
