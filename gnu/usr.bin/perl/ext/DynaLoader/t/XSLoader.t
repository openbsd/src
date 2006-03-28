#!/usr/bin/perl -wT

BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir 't';
        @INC = '../lib';
    }
}

use strict;
use Config;
use Test::More;
my %modules;
BEGIN {
    %modules = (
      # ModuleName  => q|code to check that it was loaded|,
       'Cwd'        => q| ::is( ref Cwd->can('fastcwd'),'CODE' ) |,         # 5.7 ?
       'File::Glob' => q| ::is( ref File::Glob->can('doglob'),'CODE' ) |,   # 5.6
       'SDBM_File'  => q| ::is( ref SDBM_File->can('TIEHASH'), 'CODE' ) |,  # 5.0
       'Socket'     => q| ::is( ref Socket->can('inet_aton'),'CODE' ) |,    # 5.0
       'Time::HiRes'=> q| ::is( ref Time::HiRes->can('usleep'),'CODE' ) |,  # 5.7.3
    );
    plan tests => keys(%modules) * 2 + 3
}


BEGIN {
    use_ok( 'XSLoader' );
}

# Check functions
can_ok( 'XSLoader' => 'load' );
#can_ok( 'XSLoader' => 'bootstrap_inherit' );  # doesn't work

# Check error messages
eval { XSLoader::load() };
like( $@, '/^XSLoader::load\(\'Your::Module\', \$Your::Module::VERSION\)/',
        "calling XSLoader::load() with no argument" );

# Now try to load well known XS modules
my $extensions = $Config{'extensions'};
$extensions =~ s|/|::|g;

for my $module (sort keys %modules) {
    SKIP: {
        skip "$module not available", 2 if $extensions !~ /\b$module\b/;
        eval qq| package $module; XSLoader::load('$module'); | . $modules{$module};
        is( $@, '',  "XSLoader::load($module)");
    }
}

