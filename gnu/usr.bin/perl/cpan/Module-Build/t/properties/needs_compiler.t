# sample.t -- a sample test file for Module::Build

use strict;
use lib 't/lib';
use MBTest;
use DistGen;

plan tests => 19;

# Ensure any Module::Build modules are loaded from correct directory
blib_load('Module::Build');

my $dist = DistGen->new->regen->chdir_in;

# get a Module::Build object and test with it
my $mb;
stderr_of(sub {
    ok( $mb = $dist->new_from_context, "Default Build.PL" );
});

ok( ! $mb->needs_compiler, "needs_compiler is false" );
ok( ! exists $mb->{properties}{build_requires}{'ExtUtils::CBuilder'},
  "ExtUtils::CBuilder is not in build_requires"
);

#--------------------------------------------------------------------------#
# try with c_source
#--------------------------------------------------------------------------#
$dist->change_build_pl({
    module_name => $dist->name,
    license => 'perl',
    c_source => 'src',
});
$dist->regen;
stderr_of(sub {
  ok( $mb = $dist->new_from_context,
    "Build.PL with c_source"
  );
});
is( $mb->c_source, 'src', "c_source is set" );
ok( $mb->needs_compiler, "needs_compiler is true" );
ok( exists $mb->{properties}{build_requires}{'ExtUtils::CBuilder'},
  "ExtUtils::CBuilder was added to build_requires"
);

#--------------------------------------------------------------------------#
# try with xs files
#--------------------------------------------------------------------------#
$dist = DistGen->new(dir => 'MBTest', xs => 1);
$dist->regen;
$dist->chdir_in;

stderr_of(sub {
  ok( $mb = $dist->new_from_context,
    "Build.PL with xs files"
  );
});
ok( $mb->needs_compiler, "needs_compiler is true" );
ok( exists $mb->{properties}{build_requires}{'ExtUtils::CBuilder'},
  "ExtUtils::CBuilder was added to build_requires"
);

#--------------------------------------------------------------------------#
# force needs_compiler off, despite xs modules
#--------------------------------------------------------------------------#

$dist->change_build_pl({
    module_name => $dist->name,
    license => 'perl',
    needs_compiler => 0,
});
$dist->regen;

stderr_of(sub {
  ok( $mb = $dist->new_from_context ,
    "Build.PL with xs files, but needs_compiler => 0"
  );
});
is( $mb->needs_compiler, 0, "needs_compiler is false" );
ok( ! exists $mb->{properties}{build_requires}{'ExtUtils::CBuilder'},
  "ExtUtils::CBuilder is not in build_requires"
);

#--------------------------------------------------------------------------#
# don't override specific EU::CBuilder build_requires
#--------------------------------------------------------------------------#

$dist->change_build_pl({
    module_name => $dist->name,
    license => 'perl',
    build_requires => { 'ExtUtils::CBuilder' => 0.2 },
});
$dist->regen;

stderr_of(sub {
  ok( $mb = $dist->new_from_context ,
    "Build.PL with xs files, build_requires EU::CB 0.2"
  );
});
ok( $mb->needs_compiler, "needs_compiler is true" );
is( $mb->build_requires->{'ExtUtils::CBuilder'}, 0.2,
  "build_requires for ExtUtils::CBuilder is correct version"
);

#--------------------------------------------------------------------------#
# falsify compiler and test error handling
#--------------------------------------------------------------------------#

# clear $ENV{CC} so we are sure to fail to find our fake compiler :-)
local $ENV{CC};

my $err = stderr_of( sub {
    $mb = $dist->new_from_context( config => { cc => "adfasdfadjdjk" } )
});
ok( $mb, "Build.PL while hiding compiler" );
like( $err, qr/no compiler detected/,
  "hidden compiler resulted in warning message during Build.PL"
);
eval { $mb->dispatch('build') };
like( $@, qr/no compiler detected/,
  "hidden compiler resulted in fatal message during Build"
);


# vim:ts=2:sw=2:et:sta:sts=2
