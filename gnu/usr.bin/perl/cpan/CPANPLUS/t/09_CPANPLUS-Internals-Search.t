### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';
use Data::Dumper;
use CPANPLUS::Backend;
use CPANPLUS::Internals::Constants;

my $Conf    = gimme_conf();
my $CB      = CPANPLUS::Backend->new($Conf);
my $ModName = TEST_CONF_MODULE;
my $Mod     = $CB->module_tree( $ModName );


### search for modules ###
for my $type ( CPANPLUS::Module->accessors() ) {

    ### don't muck around with references/objects
    ### or private identifiers
    next if ref $Mod->$type() or $type =~/^_/;

    my @aref = $CB->search(
                    type    => $type,
                    allow   => [$Mod->$type()],
                );

    ok( scalar @aref,       "Module found by '$type'" );
    for( @aref ) {
        ok( IS_MODOBJ->($_),"   Module isa module object" );
    }
}

### search for authors ###
my $auth = $Mod->author;
for my $type ( CPANPLUS::Module::Author->accessors() ) {
    
    ### don't muck around with references/objects
    ### or private identifiers
    next if ref $auth->$type() or $type =~/^_/;

    my @aref = $CB->search(
                    type    => $type,
                    allow   => [$auth->$type()],
                );

    ok( @aref,                  "Author found by '$type'" );
    for( @aref ) {
        ok( IS_AUTHOBJ->($_),   "   Author isa author object" );
    }
}


{   my $warning = '';
    local $SIG{__WARN__} = sub { $warning .= "@_"; };

    {   ### try search that will yield nothing ###
        ### XXX SOURCEFILES FIX
        my @list = $CB->search( type    => 'module',
                                allow   => [$ModName.$$] );

        is( scalar(@list), 0,   "Valid search yields no results" );
        is( $warning, '',       "   No warnings issued" );
    }

    {   ### try bogus arguments ###
        my @list = $CB->search( type => '', allow => ['foo'] );

        is( scalar(@list), 0,   "Broken search yields no results" );
        like( $warning, qr/^Key 'type'.* is of invalid type for/,
                                "   Got a warning for wrong arguments" );
    }
}

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
