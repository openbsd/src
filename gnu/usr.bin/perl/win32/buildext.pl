=head1 NAME

buildext.pl - build extensions

=head1 SYNOPSIS

    buildext.pl make [-make_opts] dep directory [target] [--static|--dynamic] !ext1 !ext2

E.g.

    buildext.pl nmake -nologo perldll.def ..\ext

    buildext.pl nmake -nologo perldll.def ..\ext clean

    buildext.pl dmake perldll.def ..\ext

    buildext.pl dmake perldll.def ..\ext clean

Will skip building extensions which are marked with an '!' char.
Mostly because they still not ported to specified platform.

If '--static' specified, only static extensions will be built.
If '--dynamic' specified, only dynamic extensions will be built.

--create-perllibst-h
    creates perllibst.h file for inclusion from perllib.c
--list-static-libs:
    prints libraries for static linking and exits

=cut

use Cwd;
use FindExt;
use Config;

# @ARGV with '!' at first position are exclusions
my %excl = map {$_=>1} map {/^!(.*)$/} @ARGV;
@ARGV = grep {!/^!/} @ARGV;

# --static/--dynamic
my %opts = map {$_=>1} map {/^--([\w\-]+)$/} @ARGV;
@ARGV = grep {!/^--([\w\-]+)$/} @ARGV;
my ($static,$dynamic) = ((exists $opts{static}?1:0),(exists $opts{dynamic}?1:0));
if ("$static,$dynamic" eq "0,0") {
  ($static,$dynamic) = (1,1);
}
if ($opts{'list-static-libs'} || $opts{'create-perllibst-h'}) {
  my @statics = split /\s+/, $Config{static_ext};
  if ($opts{'create-perllibst-h'}) {
    open my $fh, ">perllibst.h";
    my @statics1 = map {local $_=$_;s/\//__/g;$_} @statics;
    my @statics2 = map {local $_=$_;s/\//::/g;$_} @statics;
    print $fh "/*DO NOT EDIT\n  this file is included from perllib.c to init static extensions */\n";
    print $fh "#ifdef STATIC1\n",(map {"    \"$_\",\n"} @statics),"#undef STATIC1\n#endif\n";
    print $fh "#ifdef STATIC2\n",(map {"    EXTERN_C void boot_$_ (pTHX_ CV* cv);\n"} @statics1),"#undef STATIC2\n#endif\n";
    print $fh "#ifdef STATIC3\n",(map {"    newXS(\"$statics2[$_]::bootstrap\", boot_$statics1[$_], file);\n"} 0 .. $#statics),"#undef STATIC3\n#endif\n";
  }
  else {
    my %extralibs;
    for (@statics) {
      open my $fh, "<..\\lib\\auto\\$_\\extralibs.ld" or die "can't open <..\\lib\\auto\\$_\\extralibs.ld: $!";
      $extralibs{$_}++ for grep {/\S/} split /\s+/, join '', <$fh>;
    }
    print map {s|/|\\|g;m|([^\\]+)$|;"..\\lib\\auto\\$_\\$1$Config{_a} "} @statics;
    print map {"$_ "} sort keys %extralibs;
  }
  exit;
}

my $here = getcwd();
my $perl = $^X;
$here =~ s,/,\\,g;
if ($perl =~ m#^\.\.#)
 {
  $perl = "$here\\$perl";
 }
(my $topdir = $perl) =~ s/\\[^\\]+$//;
# miniperl needs to find perlglob and pl2bat
$ENV{PATH} = "$topdir;$topdir\\win32\\bin;$ENV{PATH}";
#print "PATH=$ENV{PATH}\n";
my $pl2bat = "$topdir\\win32\\bin\\pl2bat";
unless (-f "$pl2bat.bat") {
    my @args = ($perl, ("$pl2bat.pl") x 2);
    print "@args\n";
    system(@args) unless defined $::Cross::platform;
}
my $make = shift;
$make .= " ".shift while $ARGV[0]=~/^-/;
my $dep  = shift;
my $dmod = -M $dep;
my $dir  = shift;
chdir($dir) || die "Cannot cd to $dir\n";
my $targ  = shift;
(my $ext = getcwd()) =~ s,/,\\,g;
my $code;
FindExt::scan_ext($ext);
FindExt::set_static_extensions(split ' ', $Config{static_ext}) if $ext ne "ext";

my @ext;
push @ext, FindExt::static_ext() if $static;
push @ext, FindExt::dynamic_ext(), FindExt::nonxs_ext() if $dynamic;

foreach $dir (sort @ext)
 {
  if (exists $excl{$dir}) {
    warn "Skipping extension $ext\\$dir, not ported to current platform";
    next;
  }
  if (chdir("$ext\\$dir"))
   {
    my $mmod = -M 'Makefile';
    if (!(-f 'Makefile') || $mmod > $dmod)
     {
      print "\nRunning Makefile.PL in $dir\n";
      my @perl = ($perl, "-I$here\\..\\lib", 'Makefile.PL',
                  'INSTALLDIRS=perl', 'PERL_CORE=1',
		  (FindExt::is_static($dir)
                   ? ('LINKTYPE=static') : ()), # if ext is static
		);
      if (defined $::Cross::platform) {
	@perl = (@perl[0,1],"-MCross=$::Cross::platform",@perl[2..$#perl]);
      }
      print join(' ', @perl), "\n";
      $code = system(@perl);
      warn "$code from $dir\'s Makefile.PL" if $code;
      $mmod = -M 'Makefile';
      if ($mmod > $dmod)
       {
        warn "Makefile $mmod > $dmod ($dep)\n";
       }
     }  
    if ($targ)
     {
      print "Making $targ in $dir\n$make $targ\n";
      $code = system("$make $targ");
      die "Unsuccessful make($dir): code=$code" if $code!=0;
     }
    else
     {
      print "Making $dir\n$make\n";
      $code = system($make);
      die "Unsuccessful make($dir): code=$code" if $code!=0;
     }
    chdir($here) || die "Cannot cd to $here:$!";
   }
  else
   {
    warn "Cannot cd to $ext\\$dir:$!";
   }
 }

