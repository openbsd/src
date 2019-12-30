################################################################################
#
#  Makefile.PL -- generate Makefile
#
################################################################################
#
#  Version 3.x, Copyright (C) 2004-2013, Marcus Holland-Moritz.
#  Version 2.x, Copyright (C) 2001, Paul Marquess.
#  Version 1.x, Copyright (C) 1999, Kenneth Albanowski.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the same terms as Perl itself.
#
################################################################################

require 5.003;

use strict;
use ExtUtils::MakeMaker;

use vars '%opt';  # needs to be global, and we can't use 'our'

unless ($ENV{'PERL_CORE'}) {
  $ENV{'PERL_CORE'} = 1 if grep { $_ eq 'PERL_CORE=1' } @ARGV;
}

@ARGV = map { /^--with-(apicheck)$/ && ++$opt{$1} ? () : $_ } @ARGV;

my %mf = (
  NAME          => 'Devel::PPPort',
  VERSION_FROM  => 'PPPort_pm.PL',
  PM            => { 'PPPort.pm' => '$(INST_LIBDIR)/PPPort.pm' },
  H             => [ qw(ppport.h) ],
  OBJECT        => 'RealPPPort$(OBJ_EXT) $(O_FILES)',
  XSPROTOARG    => '-noprototypes',
  CONFIGURE     => \&configure,
);
delete $mf{META_MERGE} unless eval { ExtUtils::MakeMaker->VERSION (6.46) };
WriteMakefile(%mf);

sub configure
{
  my @clean    = qw{ $(H_FILES) RealPPPort.xs RealPPPort.c PPPort.pm };
  my %depend   = ('$(OBJECT)' => '$(H_FILES)');
  my @C_FILES  = qw{ module2.c module3.c },
  my %PL_FILES = (
    'ppport_h.PL'  => 'ppport.h',
    'PPPort_pm.PL' => 'PPPort.pm',
    'PPPort_xs.PL' => 'RealPPPort.xs',
  );
  my @moreopts;

  if (eval $ExtUtils::MakeMaker::VERSION >= 6) {
    push @moreopts, AUTHOR => 'Marcus Holland-Moritz <mhx@cpan.org>';
    push @moreopts, ABSTRACT_FROM => 'PPPort_pm.PL';
  }

  if (eval $ExtUtils::MakeMaker::VERSION >= 6.30_01) {
    print "Setting license tag...\n";
    push @moreopts, LICENSE => 'perl';
  }

  if (not $ENV{'PERL_CORE'}) {
    # Devel::PPPort is in the core since 5.7.3
    # 5.11.0+ has site before perl
    push @moreopts, INSTALLDIRS => (
      ("$]" >= 5.007003 and "$]" < 5.011)
        ? 'perl'
        : 'site'
    );
  }

  if ($opt{'apicheck'}) {
    $PL_FILES{'apicheck_c.PL'} = 'apicheck.c';
    push @C_FILES, qw{ apicheck.c };
    push @clean,   qw{ apicheck.c apicheck.i };
    $depend{'apicheck.i'} = 'ppport.h';
  }

  open my $fh, '<', 'PPPort_pm.PL' or die "cannot open PPPort_pm.PL for reading: $!";
  my $version;
  while (my $line = <$fh>) {
    ($version) = $line =~ /^\$VERSION = '([\d.]+)';$/ and last;
  };
  die 'failed to extract $VERSION from PPPort_pm.PL' if not $version;
  close $fh;

  return {
    C        => \@C_FILES,
    XS       => { 'RealPPPort.xs' => 'RealPPPort.c' },
    PL_FILES => \%PL_FILES,
    depend   => \%depend,
    clean    => { FILES => "@clean" },
    META_MERGE    => {
      'meta-spec' => { version => 2 },
      provides => {
        'Devel::PPPort' => {
          file    => 'PPPort_pm.PL',
          version => $version,
        },
      },
      resources => {
        bugtracker => {
          web => 'https://rt.perl.org/rt3/',
        },
        repository => {
          type => 'git',
          url  => 'git://perl5.git.perl.org/perl.git',
          web  => 'https://perl5.git.perl.org/perl.git',
        },
      },
    },
    @moreopts,
  };
}

sub MY::postamble
{
  package MY;
  my $post = shift->SUPER::postamble(@_);
  $post .= <<'POSTAMBLE';

purge_all: realclean
	@$(RM_F) PPPort.pm t/*.t

regen_pm:
	$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) PPPort_pm.PL

regen_xs:
	$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) PPPort_xs.PL

regen_tests:
	$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) mktests.PL

regen_h:
	$(PERL) -I$(INST_ARCHLIB) -I$(INST_LIB) -I$(PERL_ARCHLIB) -I$(PERL_LIB) ppport_h.PL

regen: regen_pm regen_xs regen_tests regen_h

POSTAMBLE
  return $post;
}

sub MY::dist_core
{
  package MY;
  my $dist = shift->SUPER::dist_core(@_);

  my $updated = '';
  my @rules = split( m{^\s*$}m, $dist );
  foreach my $rule ( @rules ) {
    if ( $rule =~ m{^\s*^dist\s+:}m ) {
        $rule .= qq[\t].q[$(NOECHO) $(ECHO) "Warning: Please check '__MAX_PERL__' value in PPPort_pm.PL"].qq[\n];
    }
    $updated .= $rule;
  }

  return $updated;
}


sub MY::c_o
{
  package MY;
  my $co = shift->SUPER::c_o(@_);

  if ($::opt{'apicheck'} && $co !~ /^\.c\.i:/m) {
    print "Adding custom rule for preprocessed apicheck file...\n";

    $co .= <<'CO'

.SUFFIXES: .i

.c.i:
	$(CCCMD) -E -I$(PERL_INC) $(DEFINE) $*.c > $*.i
CO
  }

  return $co;
}
