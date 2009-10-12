#!/usr/bin/perl

use strict;
use warnings;

use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest 'no_plan';

use_ok 'Module::Build';
ensure_blib 'Module::Build';

{
    chdir MBTest->tmpdir();

    my $build = Module::Build->new(
        dist_name       => "Foo-Bar",
        dist_version    => '1.23',
    );

    my $skip = "mskip.txt";  # for compatibility
    $build->_write_default_maniskip($skip);

    ok -r $skip, "Default maniskip written";
    my $have = slurp($skip);

    my $head;
    if( $build->_eumanifest_has_include ) {
        $head = "#!include_default\n";
    }
    else {
        $head = slurp($build->_default_maniskip);
    }

    like $have, qr/^\Q$head\E/, "default MANIFEST.SKIP used";
    like $have, qr/^# Avoid Module::Build generated /ms, "Module::Build specific entries";
    like $have, qr/Foo-Bar-/, "distribution tarball entry";
}
