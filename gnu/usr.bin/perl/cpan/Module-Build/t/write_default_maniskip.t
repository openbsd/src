#!/usr/bin/perl

use strict;
use warnings;

use lib 't/lib';
use MBTest 'no_plan';
use DistGen;
use Cwd;

blib_load('Module::Build');

{
    my $cwd = Cwd::cwd;
    chdir MBTest->tmpdir();

    my $build = Module::Build->new(
        module_name     => "Foo::Bar",
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

    DistGen::chdir_all($cwd);
}
