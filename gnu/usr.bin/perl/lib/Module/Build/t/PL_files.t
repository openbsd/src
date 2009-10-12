#!/usr/bin/perl -w

use strict;
use lib $ENV{PERL_CORE} ? '../lib/Module/Build/t/lib' : 't/lib';
use MBTest tests => 8;
use DistGen;
use Module::Build;

my $dist;

# Test that PL files don't get installed even in bin or lib
{
    $dist = DistGen->new( dir => MBTest->tmpdir );
    $dist->regen;
    $dist->chdir_in;

    my $distname = $dist->name;
    $dist->change_build_pl({
        module_name         => $distname,
        PL_files            => {
            'bin/foo.PL'        => 'bin/foo',
            'lib/Bar.pm.PL'     => 'lib/Bar.pm',
        },
    });

    $dist->add_file("bin/foo.PL", <<'END');
open my $fh, ">", $ARGV[0] or die $!;
print $fh "foo\n";
END

    $dist->add_file("lib/Bar.pm.PL", <<'END');
open my $fh, ">", $ARGV[0] or die $!;
print $fh "bar\n";
END

    $dist->regen;

    my $mb = Module::Build->new_from_context( install_base => "test_install" );
    $mb->dispatch("install");

    ok -e "test_install/bin/foo",               "Generated PL_files installed from bin";
    ok -e "test_install/lib/perl5/Bar.pm",      "  and from lib";

    ok !-e "test_install/bin/foo.PL",           "PL_files not installed from bin";
    ok !-e "test_install/lib/perl5/Bar.pm.PL",  "  nor from lib";

    is slurp("test_install/bin/foo"), "foo\n",          "Generated bin contains correct content";
    is slurp("test_install/lib/perl5/Bar.pm"), "bar\n", "  so does the lib";

    $dist->chdir_original if $dist->did_chdir;
}

# Test an empty PL target list runs the PL but doesn't
# add it to MANIFEST or cleanup
{
    $dist = DistGen->new( dir => MBTest->tmpdir );
    $dist->regen;
    $dist->chdir_in;

    my $distname = $dist->name;
    $dist->change_build_pl({
        module_name         => $distname,
        PL_files            => {
            'Special.PL'     => [],
        },
    });

    $dist->add_file("Special.PL", <<'END');
open my $fh, ">", "foo" or die $!;
print $fh "foo\n";
END

    $dist->regen;

    my $mb = Module::Build->new_from_context();
    $mb->dispatch("code");

    ok( -f "foo", "special PL file ran" );

    my $cleanup = $mb->cleanup;

    my %cleanup = map { $_ => 1 } $mb->cleanup;
    is($cleanup{foo}, undef, "generated special file not added to cleanup");


}
