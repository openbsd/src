=head1 NAME

buildext.pl - build extensions

=head1 SYNOPSIS

    buildext.pl make [-make_opts] dep directory [target] !ext1 !ext2

E.g.

    buildext.pl nmake -nologo perldll.def ..\ext

    buildext.pl nmake -nologo perldll.def ..\ext clean

    buildext.pl dmake perldll.def ..\ext

    buildext.pl dmake perldll.def ..\ext clean

Will skip building extensions which are marked with an '!' char.
Mostly because they still not ported to specified platform.

=cut

use File::Basename;
use Cwd;
use FindExt;

# @ARGV with '!' at first position are exclusions
my %excl = map {$_=>1} map {/^!(.*)$/} @ARGV;
@ARGV = grep {!/^!/} @ARGV;

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

my @ext = FindExt::extensions();

foreach my $dir (sort @ext)
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
                  'INSTALLDIRS=perl', 'PERL_CORE=1');
      if (defined $::Cross::platform) {
	@perl = (@perl[0,1],"-MCross=$::Cross::platform",@perl[2..$#perl]);
      }
      print join(' ', @perl), "\n";
      $code = system(@perl);
      warn "$code from $dir's Makefile.PL" if $code;
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

