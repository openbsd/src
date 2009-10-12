# -*- perl -*-
BEGIN { require 5.006; } # allow CPAN testers to get the point
use strict;
use warnings;
use Config;
use File::Spec;
use ExtUtils::MakeMaker;
my $PERL_CORE = grep { $_ eq 'PERL_CORE=1' } @ARGV;

my $do_xs = $PERL_CORE || can_cc();

for (@ARGV) {
  /^-pm/ and $do_xs = 0;
  /^-xs/ and $do_xs = 1;
}

WriteMakefile(
  NAME         => q[List::Util],
  ABSTRACT     => q[Common Scalar and List utility subroutines],
  AUTHOR       => q[Graham Barr <gbarr@cpan.org>],
  DEFINE       => q[-DPERL_EXT],
  DISTNAME     => q[Scalar-List-Utils],
  VERSION_FROM => 'lib/List/Util.pm',

  # We go through the ListUtil.xs trickery to foil platforms
  # that have the feature combination of
  # (1) static builds
  # (2) allowing only one object by the same name in the static library
  # (3) the object name matching being case-blind
  # This means that we can't have the top-level util.o
  # and the extension-level Util.o in the same build.
  # One such platform is the POSIX-BC BS2000 EBCDIC mainframe platform.
  XS     => {'ListUtil.xs' => 'ListUtil.c'},
  OBJECT => 'ListUtil$(OBJ_EXT)',
  ( $PERL_CORE
    ? ()
    : (
      INSTALLDIRS => q[perl],
      PREREQ_PM   => {'Test::More' => 0,},
      (eval { ExtUtils::MakeMaker->VERSION(6.31) } ? (LICENSE => 'perl') : ()),
      ($do_xs ? () : (XS => {}, C => [], OBJECT => '')),
      ( eval { ExtUtils::MakeMaker->VERSION(6.46) } ? (
          META_MERGE => {
            resources => {    ##
              repository => 'http://github.com/gbarr/Scalar-List-Utils',
            },
          }
          )
        : ()
      ),
    )
  ),
);


sub can_cc {

    foreach my $cmd (split(/ /, $Config::Config{cc})) {
        my $_cmd = $cmd;
        return $_cmd if (-x $_cmd or $_cmd = MM->maybe_command($_cmd));

        for my $dir ((split /$Config::Config{path_sep}/, $ENV{PATH}), '.') {
            my $abs = File::Spec->catfile($dir, $_[1]);
            return $abs if (-x $abs or $abs = MM->maybe_command($abs));
        }
    }

    return;
}

package MY;

sub init_PM  {
  my $self = shift;

  $self->SUPER::init_PM(@_);

  return if $do_xs;

  my $pm = $self->{PM};
  my $pm_file = File::Spec->catfile(qw(lib List Util XS.pm));

  # When installing pure perl, install XS.pp as XS.pm
  $self->{PM}{'XS.pp'} = delete $self->{PM}{$pm_file};
}

