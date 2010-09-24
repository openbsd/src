#!perl -T

use strict;
use Config;

my $db_file;
BEGIN {
    eval "use Test::More";
    if ($@) {
        print "1..0 # Skip: Test::More not available\n";
        die "Test::More not available\n";
    }

    use Config;
    foreach (qw/SDBM_File GDBM_File ODBM_File NDBM_File DB_File/) {
        if ($Config{extensions} =~ /\b$_\b/) {
            $db_file = $_;
            last;
        }
    }
}


my %modules = (
    # ModuleName  => q|code to check that it was loaded|,
    'Cwd'        => q| ::can_ok( 'Cwd' => 'fastcwd'         ) |,  # 5.7 ?
    'File::Glob' => q| ::can_ok( 'File::Glob' => 'doglob'   ) |,  # 5.6
    $db_file     => q| ::can_ok( $db_file => 'TIEHASH'      ) |,  # 5.0
    'Socket'     => q| ::can_ok( 'Socket' => 'inet_aton'    ) |,  # 5.0
    'Time::HiRes'=> q| ::can_ok( 'Time::HiRes' => 'usleep'  ) |,  # 5.7.3
);

plan tests => keys(%modules) * 3 + 5;

# Try to load the module
use_ok( 'XSLoader' );

# Check functions
can_ok( 'XSLoader' => 'load' );
can_ok( 'XSLoader' => 'bootstrap_inherit' );

# Check error messages
eval { XSLoader::load() };
like( $@, '/^XSLoader::load\(\'Your::Module\', \$Your::Module::VERSION\)/',
        "calling XSLoader::load() with no argument" );

eval q{ package Thwack; XSLoader::load('Thwack'); };
if ($Config{usedl}) {
    like( $@, q{/^Can't locate loadable object for module Thwack in @INC/},
        "calling XSLoader::load() under a package with no XS part" );
}
else {
    like( $@, q{/^Can't load module Thwack, dynamic loading not available in this perl./},
        "calling XSLoader::load() under a package with no XS part" );
}

# Now try to load well known XS modules
my $extensions = $Config{'extensions'};
$extensions =~ s|/|::|g;

for my $module (sort keys %modules) {
    my $warnings = "";
    local $SIG{__WARN__} = sub { $warnings = $_[0] };

    SKIP: {
        skip "$module not available", 4 if $extensions !~ /\b$module\b/;

        eval qq{ package $module; XSLoader::load('$module', "12345678"); };
        like( $@, "/^$module object version \\S+ does not match bootstrap parameter (?:12345678|0)/",
                "calling XSLoader::load() with a XS module and an incorrect version" );

        eval qq{ package $module; XSLoader::load('$module'); };
        is( $@, '',  "XSLoader::load($module)");

        eval qq{ package $module; $modules{$module}; };
    }
}

