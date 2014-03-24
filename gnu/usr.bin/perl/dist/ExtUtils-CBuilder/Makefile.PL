use strict;
use warnings;

use ExtUtils::MakeMaker 6.30;

my %WriteMakefileArgs = (
  "NAME" => "ExtUtils::CBuilder",
  "VERSION_FROM" => "lib/ExtUtils/CBuilder.pm",
  "ABSTRACT_FROM" => "lib/ExtUtils/CBuilder.pm",
  "AUTHOR" => "Ken Williams <kwilliams\@cpan.org>, The Perl 5 Porters",
  "BUILD_REQUIRES" => {
    "Test::More" => "0.47"
  },
  "CONFIGURE_REQUIRES" => {
    "ExtUtils::MakeMaker" => "6.30"
  },
  "EXE_FILES" => [],
  "INSTALLDIRS" => ($] < 5.011 ? 'perl' : 'site'),
  "LICENSE" => "perl",
  "PREREQ_PM" => {
    "Cwd" => 0,
    "File::Basename" => 0,
    "File::Spec" => "3.13",
    "File::Temp" => 0,
    "IO::File" => 0,
    "IPC::Cmd" => 0,
    "Perl::OSType" => 0,
    "Text::ParseWords" => 0
  },
);

unless ( eval { ExtUtils::MakeMaker->VERSION(6.56) } ) {
  my $br = delete $WriteMakefileArgs{BUILD_REQUIRES};
  my $pp = $WriteMakefileArgs{PREREQ_PM};
  for my $mod ( keys %$br ) {
    if ( exists $pp->{$mod} ) {
      $pp->{$mod} = $br->{$mod} if $br->{$mod} > $pp->{$mod};
    }
    else {
      $pp->{$mod} = $br->{$mod};
    }
  }
}

delete $WriteMakefileArgs{CONFIGURE_REQUIRES}
  unless eval { ExtUtils::MakeMaker->VERSION(6.52) };

$WriteMakefileArgs{INSTALLDIRS} = 'perl'
    if $] >= 5.009005 && $] <= 5.011000;

WriteMakefile(%WriteMakefileArgs);

