$line = 1;
while (<>)
 {
  $line++;
  # 1st correct #line directives for perly.c itself
  s/^(#line\s+)\d+(\s*"perly\.c"\s*)$/$1$line$2/;
  # now add () round things gcc dislikes
  s/if \(yyn = yydefred\[yystate\]\)/if ((yyn = yydefred[yystate]))/;
  s/if \(yys = getenv\("YYDEBUG"\)\)/if ((yys = getenv("YYDEBUG")))/;
  print;
 }
