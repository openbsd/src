BEGIN {
    if( $ENV{PERL_CORE} ) {
        chdir '../lib/Archive/Tar' if -d '../lib/Archive/Tar';
    }       
    use lib '../../..';
}

BEGIN { chdir 't' if -d 't' }

use Test::More 'no_plan';
use strict;
use lib '../lib';

my $Class   = 'Archive::Tar';
my $FClass  = 'Archive::Tar::File';
my $File    = 'src/long/bar.tar';
my @Expect = (
    qr|^c$|,
    qr|^d$|,
    qr|^directory/$|,
    qr|^directory/really.*name/$|,
    qr|^directory/.*/myfile$|,
);

use_ok( $Class );

### crazy ref to special case 'all'
for my $index ( \0, 0 .. $#Expect ) {   

    my %opts    = ();
    my @expect  = ();
    
    ### do a full test vs individual filters
    if( not ref $index ) {
        my $regex       = $Expect[$index];
        $opts{'filter'} = $regex;
        @expect         = ($regex);
    } else {
        @expect         = @Expect;
    }        

    my $next = $Class->iter( $File, 0, \%opts );
    
    my $pp_opts = join " => ", %opts;
    ok( $next,                  "Iterator created from $File ($pp_opts)" );
    isa_ok( $next, "CODE",      "   Iterator" );

    my @names;
    while( my $f = $next->() ) {
        ok( $f,                 "       File object retrieved" );
        isa_ok( $f, $FClass,    "           Object" );

        push @names, $f->name;
    }
    
    is( scalar(@names), scalar(@expect),
                                "   Found correct number of files" );
    
    my $i = 0;
    for my $name ( @names ) {
        ok( 1,                  "   Inspecting '$name' " );
        like($name, $expect[$i],"       Matches $Expect[$i]" );
        $i++;
    }        
}
