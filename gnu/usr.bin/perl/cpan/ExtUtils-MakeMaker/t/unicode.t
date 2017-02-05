# Test problems in Makefile.PL's and hint files.

BEGIN {
    unshift @INC, 't/lib';
}
chdir 't';

use strict;
use Test::More;
use Config;
BEGIN {
  plan skip_all => 'Need perlio and perl 5.8+.'
    if $] < 5.008 or !$Config{useperlio};
  plan tests => 9;
}
use ExtUtils::MM;
use MakeMaker::Test::Setup::Unicode;
use MakeMaker::Test::Utils qw(makefile_name make_run run);
use TieOut;

my $MM = bless { DIR => ['.'] }, 'MM';

ok( setup_recurs(), 'setup' );
END {
    ok( chdir File::Spec->updir, 'chdir updir' );
    ok( teardown_recurs(), 'teardown' );
}

ok( chdir 'Problem-Module', "chdir'd to Problem-Module" ) ||
  diag("chdir failed: $!");

if ($] >= 5.008) {
  eval { require ExtUtils::MakeMaker::Locale; };
  note "ExtUtils::MakeMaker::Locale vars: $ExtUtils::MakeMaker::Locale::ENCODING_LOCALE;$ExtUtils::MakeMaker::Locale::ENCODING_LOCALE_FS;$ExtUtils::MakeMaker::Locale::ENCODING_CONSOLE_IN;$ExtUtils::MakeMaker::Locale::ENCODING_CONSOLE_OUT\n" unless $@;
  note "Locale env vars: " . join(';', map {
    "$_=$ENV{$_}"
  } grep { /LANG|LC/ } keys %ENV) . "\n";
}

# Make sure when Makefile.PL's break, they issue a warning.
# Also make sure Makefile.PL's in subdirs still have '.' in @INC.
{
    my $stdout = tie *STDOUT, 'TieOut' or die;

    my $warning = '';
    local $SIG{__WARN__} = sub { $warning .= join '', @_ };
    $MM->eval_in_subdirs;
    my $warnlines = grep { !/does not map to/ } split "\n", $warning;
    is $warnlines, 0, 'no warning' or diag $warning;

    open my $json_fh, '<:utf8', 'MYMETA.json' or die $!;
    my $json = do { local $/; <$json_fh> };
    close $json_fh;

    require Encode;
    my $str = Encode::decode( 'utf8', "Danijel Tašov's" );
    like( $json, qr/$str/, 'utf8 abstract' );

    untie *STDOUT;
}

my $make = make_run();
my $make_out = run("$make");
is $? >> 8, 0, 'Exit code of make == 0';

my $manfile = File::Spec->catfile(qw(blib man1 probscript.1));
SKIP: {
  skip 'Manpage not generated', 1 unless -f $manfile;
  skip 'Pod::Man >= 2.17 needed', 1 unless do {
    require Pod::Man; $Pod::Man::VERSION >= 2.17;
  };
  open my $man_fh, '<:utf8', $manfile or die "open $manfile: $!";
  my $man = do { local $/; <$man_fh> };
  close $man_fh;

  require Encode;
  my $str = Encode::decode( 'utf8', "文档" );
  like( $man, qr/$str/, 'utf8 man-snippet' );
}

$make_out = run("$make realclean");
is $? >> 8, 0, 'Exit code of make == 0';

sub makefile_content {
  open my $fh, '<', makefile_name or die;
  return <$fh>;
}
