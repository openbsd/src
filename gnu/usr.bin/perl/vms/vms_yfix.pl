# This script takes the output produced from perly.y by byacc and
# the perly.fixer shell script (i.e. the perly.c and perly.h built
# for Unix systems) and patches them to produce copies containing
# appropriate declarations for VMS handling of global symbols.
#
# If it finds that the input files are already patches for VMS,
# it just copies the input to the output.
#
# Revised 20-Dec-1996 by Charles Bailey  bailey@newman.upenn.edu

$VERSION = '1.11';

push(@ARGV,(qw[ perly.c perly.h vms/perly_c.vms vms/perly_h.vms])[@ARGV..4])
    if @ARGV < 4;
($cinfile,$hinfile,$coutfile,$houtfile) = @ARGV;

open C,$cinfile or die "Can't read $cinfile: $!\n";
open COUT, ">$coutfile" or die "Can't create $coutfile: $!\n";
print COUT <<EOH;
/* Postprocessed by vms_yfix.pl $VERSION to add VMS declarations of globals */
EOH
while (<C>) {
  # "y.tab.c" is illegal as a VMS filename; DECC 5.2/VAX preprocessor
  # doesn't like this.
  if ( s/^#line\s+(\d+)\s+"y.tab.c"/#line $1 "y_tab.c"/ ) { 1; }
  elsif (/char \*getenv/) {
    # accomodate old VAXC's macro susbstitution pecularities
    $_ = "#   ifndef getenv\n$_#   endif\n";
  }
  elsif ( /getenv\("YYDEBUG"\)/ ) {
    $_ = "  {\n    register int saved_errno = errno;\n"
       . "#ifdef VMS\n    register int saved_vaxc_errno = vaxc\$errno;\n"
       . "#else\n    register int saved_vaxc_errno = 0;\n#endif\n" . $_;
    # Reset the "error" status if an optional lookup fails
    while (not /^\s+\}/) { print COUT; $_ = <C>; }
    $_ .= "    else SETERRNO(saved_errno,saved_vaxc_errno);\n  }\n";
  }
  else {
    # add the dEXT tag to definitions of global vars, so we'll insert
    # a globaldef when perly.c is compiled
    s/^(short|int|YYSTYPE|char \*)\s*yy/dEXT $1 yy/;
  }
  print COUT;
}
close C;
close COUT;

open H,$hinfile  or die "Can't read $hinfile: $!\n";
open HOUT, ">$houtfile" or die "Can't create $houtfile: $!\n";
print HOUT <<EOH;
/* Postprocessed by vms_yfix.pl $VERSION to add VMS declarations of globals */
EOH
$hfixed = 0;  # keep -w happy
while (<H>) {
  $hfixed = /globalref/ unless $hfixed;  # we've already got a fixed copy
  next if /^extern YYSTYPE yylval/;  # we've got a Unix version, and this
                                     # is what we want to replace
  print HOUT;
}
close H;

print HOUT <<'EODECL' unless $hfixed;
#ifndef vax11c
  extern YYSTYPE yylval;
#else
  globalref YYSTYPE yylval;
#endif
EODECL

close HOUT;
