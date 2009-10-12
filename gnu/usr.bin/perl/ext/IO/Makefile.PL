# This -*- perl -*- script makes the Makefile

BEGIN { require 5.006_001 }
use ExtUtils::MakeMaker;
use Config qw(%Config);
my $PERL_CORE = grep { $_ eq 'PERL_CORE=1' } @ARGV;

#--- Attempt to find <poll.h>

my $define = "";

unless ($PERL_CORE or exists $Config{'i_poll'}) {
  my @inc = split(/\s+/, join(" ", $Config{'usrinc'}, $Config{'incpth'}, $Config{'locincpth'}));
  foreach $path (@inc) {
    if (-f $path . "/poll.h") {
      $define .= "-DI_POLL ";
      last;
    }
  }
}

if ($] < 5.008 and !$PERL_CORE) {
  open(FH,">typemap");
  print FH "const char *            T_PV\n";
  close(FH);
}

#--- Write the Makefile

WriteMakefile(
  VERSION_FROM => "IO.pm",
  NAME         => "IO",
  OBJECT       => '$(O_FILES)',
  ABSTRACT     => 'Perl core IO modules',
  AUTHOR       => 'Graham Barr <gbarr@cpan.org>',
  ( $PERL_CORE
    ? ()
    : (
      INSTALLDIRS => 'perl',
      clean       => {FILES => 'typemap'},
    )
  ),
  ($define                                    ? (DEFINE    => $define) : ()),
  ((ExtUtils::MakeMaker->VERSION() gt '6.30') ? ('LICENSE' => 'perl')  : ()),
);
