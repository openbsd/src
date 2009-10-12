#!/usr/bin/perl -w

use strict;

use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 5;

use_ok 'Module::Build';
ensure_blib('Module::Build');

#########################

my $tmp = MBTest->tmpdir;

# Create test distribution; set requires and build_requires
use DistGen;
my $dist = DistGen->new( dir => $tmp );

$dist->regen;

$dist->chdir_in;

#########################

my $mb; stdout_of(sub{ $mb = Module::Build->new_from_context});

use Module::Build::Compat;

$dist->regen;

Module::Build::Compat->create_makefile_pl('passthrough', $mb);

# as silly as all of this exit(0) business is, that is what the cpan
# testers have instructed everybody to do so...
$dist->change_file('Build.PL' =>
  "warn qq(you have no libthbbt\n); exit;\n" . $dist->get_file('Build.PL')
);

$dist->regen;

stdout_of(sub{ $mb->ACTION_realclean });

my $result;
my ($stdout, $stderr ) = stdout_stderr_of (sub {
  $result = $mb->run_perl_script('Makefile.PL');
});
ok $result, "Makefile.PL exit";
like $stdout, qr/running Build\.PL/;
like $stderr, qr/you have no libthbbt$/;
#warn "out: $stdout"; warn "err: $stderr";

# vim:ts=2:sw=2:et:sta
