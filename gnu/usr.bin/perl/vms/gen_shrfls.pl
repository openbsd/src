# Create global symbol declarations, transfer vector, and
# linker options files for PerlShr.
#
# Input:
#    $cflags - command line qualifiers passed to cc when preprocesing perl.h
#        Note: A rather simple-minded attempt is made to restore quotes to
#        a /Define clause - use with care.
#    $objsuffix - file type (including '.') used for object files.
#    $libperl - Perl object library.
#    $extnames - package names for static extensions (used to generate
#        linker options file entries for boot functions)
#    $rtlopt - name of options file specifying RTLs to which PerlShr.Exe
#        must be linked
#
# Output:
#    PerlShr_Attr.Opt - linker options file which speficies that global vars
#        be placed in NOSHR,WRT psects.  Use when linking any object files
#        against PerlShr.Exe, since cc places global vars in SHR,WRT psects
#        by default.
#    PerlShr_Bld.Opt - declares universal symbols for PerlShr.Exe
#    Perlshr_Gbl*.Mar, Perlshr_Gbl*.Obj (VAX  only) - declares global symbols
#        for global vars (done here because gcc can't globaldef) and creates
#        transfer vectors for routines on a VAX.
#    PerlShr_Gbl.Opt (VAX only) - list of PerlShr_Gbl*.Obj, used for input
#        to the linker when building PerlShr.Exe.
#
# To do:
#   - figure out a good way to collect global vars in one psect, given that
#     we can't use globaldef because of gcc.
#   - then, check for existing files and preserve symbol and transfer vector
#     order for upward compatibility
#   - then, add GSMATCH to options file - but how do we insure that new
#     library has everything old one did
#     (i.e. /Define=DEBUGGING,EMBED,MULTIPLICITY)?
#
# Author: Charles Bailey  bailey@genetics.upenn.edu

require 5.000;

$debug = $ENV{'GEN_SHRFLS_DEBUG'};

print "gen_shrfls.pl Rev. 14-Dec-1997\n" if $debug;

if ($ARGV[0] eq '-f') {
  open(INP,$ARGV[1]) or die "Can't read input file $ARGV[1]: $!\n";
  print "Input taken from file $ARGV[1]\n" if $debug;
  @ARGV = ();
  while (<INP>) {
    chomp;
    push(@ARGV,split(/\|/,$_));
  }
  close INP;
  print "Read input data | ",join(' | ',@ARGV)," |\n" if $debug > 1;
}

$cc_cmd = shift @ARGV;

# Someday, we'll have $GetSyI built into perl . . .
$isvax = `\$ Write Sys\$Output F\$GetSyI(\"HW_MODEL\")` <= 1024;
print "\$isvax: \\$isvax\\\n" if $debug;

print "Input \$cc_cmd: \\$cc_cmd\\\n" if $debug;
$docc = ($cc_cmd !~ /^~~/);
print "\$docc = $docc\n" if $debug;

if ($docc) {
  if (-f 'perl.h') { $dir = '[]'; }
  elsif (-f '[-]perl.h') { $dir = '[-]'; }
  else { die "$0: Can't find perl.h\n"; }

  # Go see if debugging is enabled in config.h
  $config = $dir . "config.h";
  open CONFIG, "< $config";
  while(<CONFIG>) {
    $debugging_enabled++ if /define\s+DEBUGGING/;
    $hide_mymalloc++ if /define\s+EMBEDMYMALLOC/;
    $use_mymalloc++ if /define\s+MYMALLOC/;
  }
  
  # put quotes back onto defines - they were removed by DCL on the way in
  if (($prefix,$defines,$suffix) =
         ($cc_cmd =~ m#(.*)/Define=(.*?)([/\s].*)#i)) {
    $defines =~ s/^\((.*)\)$/$1/;
    $debugging_enabled ||= $defines =~ /\bDEBUGGING\b/;
    @defines = split(/,/,$defines);
    $cc_cmd = "$prefix/Define=(" . join(',',grep($_ = "\"$_\"",@defines)) 
              . ')' . $suffix;
  }
  print "Filtered \$cc_cmd: \\$cc_cmd\\\n" if $debug;

  # check for gcc - if present, we'll need to use MACRO hack to
  # define global symbols for shared variables
  $isvaxc = 0;
  $isgcc = `$cc_cmd _nla0:/Version` =~ /GNU/
           or 0; # make debug output nice
  $isvaxc = (!$isgcc && $isvax &&
             # Check exit status too, in case message is shut off
             (`$cc_cmd /prefix=all _nla0:` =~ /IVQUAL/ || $? == 0x38240))
            or 0; # again, make debug output nice
  print "\$isgcc: $isgcc\n" if $debug;
  print "\$isvaxc: $isvaxc\n" if $debug;
  print "\$debugging_enabled: $debugging_enabled\n" if $debug;

}
else { 
  ($junk,$junk,$cpp_file,$cc_cmd) = split(/~~/,$cc_cmd,4);
  $isgcc = $cc_cmd =~ /case_hack/i
           or 0;  # for nice debug output
  $isvaxc = (!$isgcc && $cc_cmd !~ /standard=/i)
            or 0;  # again, for nice debug output
  $debugging_enabled = $cc_cmd =~ /\bdebugging\b/i;
  print "\$isgcc: \\$isgcc\\\n" if $debug;
  print "\$isvaxc: \\$isvaxc\\\n" if $debug;
  print "\$debugging_enabled: \\$debugging_enabled\\\n" if $debug;
  print "Not running cc, preprocesor output in \\$cpp_file\\\n" if $debug;
}

$objsuffix = shift @ARGV;
print "\$objsuffix: \\$objsuffix\\\n" if $debug;
$dbgprefix = shift @ARGV;
print "\$dbgprefix: \\$dbgprefix\\\n" if $debug;
$olbsuffix = shift @ARGV;
print "\$olbsuffix: \\$olbsuffix\\\n" if $debug;
$libperl = "${dbgprefix}libperl$olbsuffix";
$extnames = shift @ARGV;
print "\$extnames: \\$extnames\\\n" if $debug;
$rtlopt = shift @ARGV;
print "\$rtlopt: \\$rtlopt\\\n" if $debug;

# This part gets tricky.  VAXC creates global symbols for each of the
# constants in an enum if that enum is ever used as the data type of a
# global[dr]ef.  We have to detect enums which are used in this way, so we
# can set up the constants as universal symbols, since anything which
# #includes perl.h will want to resolve these global symbols.
# We're using a weak test here - we basically know that the only enums
# we need to handle now are the big one in opcode.h, and the
# "typedef enum { ... } expectation" in perl.h, so we hard code
# appropriate tests below. Since we can't know in general whether a given
# enum will be used elsewhere in a globaldef, it's hard to decide a
# priori whether its constants need to be treated as global symbols.
sub scan_enum {
  my($line) = @_;

  return unless $isvaxc;

  return unless /^\s+(OP|X)/;  # we only want opcode and expectation enums
  print "\tchecking for enum constant\n" if $debug > 1;
  $line =~ s#/\*.+##;
  $line =~ s/,?\s*\n?$//;
  print "\tfiltered to \\$line\\\n" if $debug > 1;
  if ($line =~ /(\w+)$/) {
    print "\tconstant name is \\$1\\\n" if $debug > 1;
    $enums{$1}++;
  }
}

sub scan_var {
  my($line) = @_;
  my($const) = $line =~ /^EXTCONST/;

  print "\tchecking for global variable\n" if $debug > 1;
  $line =~ s/\s*EXT/EXT/;
  $line =~ s/INIT\s*\(.*\)//;
  $line =~ s/\[.*//;
  $line =~ s/=.*//;
  $line =~ s/\W*;?\s*$//;
  $line =~ s/\W*\)\s*\(.*$//; # closing paren for args stripped in previous stmt
  print "\tfiltered to \\$line\\\n" if $debug > 1;
  if ($line =~ /(\w+)$/) {
    print "\tvar name is \\$1\\" . ($const ? ' (const)' : '') . "\n" if $debug > 1;
   if ($const) { $cvars{$1}++; }
   else        { $vars{$1}++;  }
  }
  if ($isvaxc) {
    my($type) = $line =~ /^\s*EXT\w*\s+(\w+)/;
    print "\tchecking for use of enum (type is \"$type\")\n" if $debug > 2;
    if ($type eq 'expectation') {
      $used_expectation_enum++;
      print "\tsaw global use of enum \"expectation\"\n" if $debug > 1;
    }
    if ($type eq 'opcode') {
      $used_opcode_enum++;
      print "\tsaw global use of enum \"opcode\"\n" if $debug > 1;
    }
  }
}

sub scan_func {
  my($line) = @_;

  print "\tchecking for global routine\n" if $debug > 1;
  if ( $line =~ /(\w+)\s+\(/ ) {
    print "\troutine name is \\$1\\\n" if $debug > 1;
    if ($1 eq 'main' || $1 eq 'perl_init_ext') {
      print "\tskipped\n" if $debug > 1;
    }
    else { $fcns{$1}++ }
  }
}

# Go add some right up front if we need 'em
if ($use_mymalloc) {
  $fcns{'Perl_malloc'}++;
  $fcns{'Perl_calloc'}++;
  $fcns{'Perl_realloc'}++;
  $fcns{'Perl_myfree'}++;
}

$used_expectation_enum = $used_opcode_enum = 0; # avoid warnings
if ($docc) {
  open(CPP,"${cc_cmd}/NoObj/PreProc=Sys\$Output ${dir}perl.h|")
    or die "$0: Can't preprocess ${dir}perl.h: $!\n";
}
else {
  open(CPP,"$cpp_file") or die "$0: Can't read preprocessed file $cpp_file: $!\n";
}
%checkh = map { $_,1 } qw( thread bytecode byterun proto );
$ckfunc = 0;
LINE: while (<CPP>) {
  while (/^#.*vmsish\.h/i .. /^#.*perl\.h/i) {
    while (/__VMS_PROTOTYPES__/i .. /__VMS_SEPYTOTORP__/i) {
      print "vms_proto>> $_" if $debug > 2;
      if (/^\s*EXT/) { &scan_var($_);  }
      else        { &scan_func($_); }
      last LINE unless defined($_ = <CPP>);
    }
    print "vmsish.h>> $_" if $debug > 2;
    if (/^\s*EXT/) { &scan_var($_); }
    last LINE unless defined($_ = <CPP>);
  }    
  while (/^#.*opcode\.h/i .. /^#.*perl\.h/i) {
    print "opcode.h>> $_" if $debug > 2;
    if (/^OP \*\s/) { &scan_func($_); }
    if (/^\s*EXT/) { &scan_var($_); }
    if (/^\s+OP_/) { &scan_enum($_); }
    last LINE unless defined($_ = <CPP>);
  }
  while (/^typedef enum/ .. /^\s*\}/) {
    print "global enum>> $_" if $debug > 2;
    &scan_enum($_);
    last LINE unless defined($_ = <CPP>);
  }
  # Check for transition to new header file
  if (/^# \d+ "(\S+)"/) {
    my $spec = $1;
    # Pull name from library module or header filespec
    $spec =~ /^(\w+)$/ or $spec =~ /(\w+)\.h/i;
    my $name = lc $1;
    $ckfunc = exists $checkh{$name} ? 1 : 0;
    $scanname = $name if $ckfunc;
    print "Header file transition: ckfunc = $ckfunc for $name.h\n" if $debug > 1;
  }
  if ($ckfunc) {
    print "$scanname>> $_" if $debug > 2;
    if (/\s*^EXT/) { &scan_var($_);  }
    else           { &scan_func($_); }
  }
  else {
    print $_ if $debug > 3 && ($debug > 5 || length($_));
    if (/^\s*EXT/) { &scan_var($_); }
  }
}
close CPP;


# Kluge to determine whether we need to add EMBED prefix to
# symbols read from local list.  vmsreaddirversions() is a VMS-
# specific function whose Perl_ prefix is added in vmsish.h
# if EMBED is #defined.
$embed = exists($fcns{'Perl_vmsreaddirversions'}) ? 'Perl_' : '';
while (<DATA>) {
  next if /^#/;
  s/\s+#.*\n//;
  next if /^\s*$/;
  ($key,$array) = split('=',$_);
  $key = "$embed$key";
  print "Adding $key to \%$array list\n" if $debug > 1;
  ${$array}{$key}++;
}
if ($debugging_enabled and $isgcc) { $vars{'colors'}++ }
foreach (split /\s+/, $extnames) {
  my($pkgname) = $_;
  $pkgname =~ s/::/__/g;
  $fcns{"boot_$pkgname"}++;
  print "Adding boot_$pkgname to \%fcns (for extension $_)\n" if $debug;
}

# If we're using VAXC, fold in the names of the constants for enums
# we've seen as the type of global vars.
if ($isvaxc) {
  foreach (keys %enums) {
    if (/^OP/) {
      $vars{$_}++ if $used_opcode_enum;
      next;
    }
    if (/^X/) {
      $vars{$_}++ if $used_expectation_enum;
      next;
    }
    print STDERR "Unrecognized enum constant \"$_\" ignored\n";
  }
}
elsif ($isgcc) {
  # gcc creates this as a SHR,WRT psect in globals.c, but we
  # don't see it in the perl.h scan, since it's only declared
  # if DOINIT is #defined.  Bleah.  It's cheaper to just add
  # it by hand than to add /Define=DOINIT to the preprocessing
  # run and wade through all the extra junk.
  $vars{"${embed}Error"}++;
}

# Eventually, we'll check against existing copies here, so we can add new
# symbols to an existing options file in an upwardly-compatible manner.

$marord++;
open(OPTBLD,">${dir}${dbgprefix}perlshr_bld.opt")
  or die "$0: Can't write to ${dir}${dbgprefix}perlshr_bld.opt: $!\n";
if ($isvax) {
  open(MAR,">${dir}perlshr_gbl${marord}.mar")
    or die "$0: Can't write to ${dir}perlshr_gbl${marord}.mar: $!\n";
  print MAR "\t.title perlshr_gbl$marord\n";
}
unless ($isgcc) {
  print OPTBLD "PSECT_ATTR=\$GLOBAL_RO_VARS,PIC,NOEXE,RD,NOWRT,SHR\n";
  print OPTBLD "PSECT_ATTR=\$GLOBAL_RW_VARS,PIC,NOEXE,RD,WRT,NOSHR\n";
}
foreach $var (sort (keys %vars,keys %cvars)) {
  if ($isvax) { print OPTBLD "UNIVERSAL=$var\n"; }
  else { print OPTBLD "SYMBOL_VECTOR=($var=DATA)\n"; }
  # This hack brought to you by the lack of a globaldef in gcc.
  if ($isgcc) {
    if ($count++ > 200) {  # max 254 psects/file
      print MAR "\t.end\n";
      close MAR;
      $marord++;
      open(MAR,">${dir}perlshr_gbl${marord}.mar")
        or die "$0: Can't write to ${dir}perlshr_gbl${marord}.mar: $!\n";
      print MAR "\t.title perlshr_gbl$marord\n";
      $count = 0;
    }
    print MAR "\t.psect ${var},long,pic,ovr,rd,wrt,noexe,noshr\n";
    print MAR "\t${var}::	.blkl 1\n";
  }
}

print MAR "\t.psect \$transfer_vec,pic,rd,nowrt,exe,shr\n" if ($isvax);
foreach $func (sort keys %fcns) {
  if ($isvax) {
    print MAR "\t.transfer $func\n";
    print MAR "\t.mask $func\n";
    print MAR "\tjmp G\^${func}+2\n";
  }
  else { print OPTBLD "SYMBOL_VECTOR=($func=PROCEDURE)\n"; }
}
if ($isvax) {
  print MAR "\t.end\n";
  close MAR;
}

open(OPTATTR,">${dir}perlshr_attr.opt")
  or die "$0: Can't write to ${dir}perlshr_attr.opt: $!\n";
if ($isvaxc) {
  print OPTATTR "PSECT_ATTR=\$CHAR_STRING_CONSTANTS,PIC,SHR,NOEXE,RD,NOWRT\n";
}
elsif ($isgcc) {
  foreach $var (sort keys %cvars) {
    print OPTATTR "PSECT_ATTR=${var},PIC,OVR,RD,NOEXE,NOWRT,SHR\n";
  }
  foreach $var (sort keys %vars) {
    print OPTATTR "PSECT_ATTR=${var},PIC,OVR,RD,NOEXE,WRT,NOSHR\n";
  }
}
else {
  print OPTATTR "! No additional linker directives are needed when using DECC\n";
}
close OPTATTR;

$incstr = 'perl,globals';
if ($isvax) {
  $drvrname = "Compile_shrmars.tmp_".time;
  open (DRVR,">$drvrname") or die "$0: Can't write to $drvrname: $!\n";
  print DRVR "\$ Set NoOn\n";  
  print DRVR "\$ Delete/NoLog/NoConfirm $drvrname;\n";
  print DRVR "\$ old_proc_vfy = F\$Environment(\"VERIFY_PROCEDURE\")\n";
  print DRVR "\$ old_img_vfy = F\$Environment(\"VERIFY_IMAGE\")\n";
  print DRVR "\$ MCR $^X -e \"\$ENV{'LIBPERL_RDT'} = (stat('$libperl'))[9]\"\n";
  print DRVR "\$ Set Verify\n";
  print DRVR "\$ If F\$Search(\"$libperl\").eqs.\"\" Then Library/Object/Create $libperl\n";
  do {
    push(@symfiles,"perlshr_gbl$marord");
    print DRVR "\$ Macro/NoDebug/Object=PerlShr_Gbl${marord}$objsuffix PerlShr_Gbl$marord.Mar\n";
    print DRVR "\$ Library/Object/Replace/Log $libperl PerlShr_Gbl${marord}$objsuffix\n";
  } while (--$marord); 
  # We had to have a working miniperl to run this program; it's probably the
  # one we just built.  It depended on LibPerl, which will be changed when
  # the PerlShr_Gbl* modules get inserted, so miniperl will be out of date,
  # and so, therefore, will all of its dependents . . .
  # We touch LibPerl here so it'll be back 'in date', and we won't rebuild
  # miniperl etc., and therefore LibPerl, the next time we invoke MM[KS].
  print DRVR "\$ old_proc_vfy = F\$Verify(old_proc_vfy,old_img_vfy)\n";
  print DRVR "\$ MCR $^X -e \"utime 0, \$ENV{'LIBPERL_RDT'}, '$libperl'\"\n";
  close DRVR;
}

# Initial hack to permit building of compatible shareable images for a
# given version of Perl.
if ($ENV{PERLSHR_USE_GSMATCH}) {
  my $major = int($] * 1000)                        & 0xFF;  # range 0..255
  my $minor = int(($] * 1000 - $major) * 100 + 0.5) & 0xFF;  # range 0..255
  print OPTBLD "GSMATCH=LEQUAL,$major,$minor\n";
  print OPTBLD 'CLUSTER=$$TRANSFER_VECTOR,,',
               map(",$_$objsuffix",@symfiles), "\n";
}
elsif (@symfiles) { $incstr .= ',' . join(',',@symfiles); }
# Include object modules and RTLs in options file
# Linker wants /Include and /Library on different lines
print OPTBLD "$libperl/Include=($incstr)\n";
print OPTBLD "$libperl/Library\n";
open(RTLOPT,$rtlopt) or die "$0: Can't read options file $rtlopt: $!\n";
while (<RTLOPT>) { print OPTBLD; }
close RTLOPT;
close OPTBLD;

exec "\$ \@$drvrname" if $isvax;


__END__

# Oddball cases, so we can keep the perl.h scan above simple
regkind=vars    # declared in regcomp.h
simple=vars     # declared in regcomp.h
varies=vars     # declared in regcomp.h
watchaddr=vars  # declared in run.c
watchok=vars    # declared in run.c
yychar=vars     # generated by byacc in perly.c
yycheck=vars    # generated by byacc in perly.c
yydebug=vars    # generated by byacc in perly.c
yydefred=vars   # generated by byacc in perly.c
yydgoto=vars    # generated by byacc in perly.c
yyerrflag=vars  # generated by byacc in perly.c
yygindex=vars   # generated by byacc in perly.c
yylen=vars      # generated by byacc in perly.c
yylhs=vars      # generated by byacc in perly.c
yylval=vars     # generated by byacc in perly.c
yyname=vars     # generated by byacc in perly.c
yynerrs=vars    # generated by byacc in perly.c
yyrindex=vars   # generated by byacc in perly.c
yyrule=vars     # generated by byacc in perly.c
yysindex=vars   # generated by byacc in perly.c
yytable=vars    # generated by byacc in perly.c
yyval=vars      # generated by byacc in perly.c
