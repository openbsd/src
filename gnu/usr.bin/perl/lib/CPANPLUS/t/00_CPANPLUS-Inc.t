### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use strict;
use Test::More 'no_plan';

my $Class = 'CPANPLUS::inc';
use_ok( $Class );
can_ok( $Class, qw[original_perl5opt original_perl5lib original_inc] );

__END__

# XXX CPANPLUS::inc functionality is obsolete, so it is removed

my $Module = 'Params::Check';
my $File   = File::Spec->catfile(qw|Params Check.pm|);
my $Ufile  = 'Params/Check.pm';
my $Boring = 'IO::File';
my $Bfile  = 'IO/File.pm';



### now, first element should be a coderef ###
my $code = $INC[0];
is( ref $code, 'CODE',          'Coderef loaded in @INC' );

### check interesting modules ###
{   my $mods = CPANPLUS::inc->interesting_modules();
    ok( $mods,                  "Retrieved interesting modules list" );
    is( ref $mods, 'HASH',      "   It's a hashref" );
    ok( scalar(keys %$mods),    "   With some keys in it" );
    ok( $mods->{$Module},       "   Found a module we care about" );
}

### checking include path ###
SKIP: {   
    my $path = CPANPLUS::inc->inc_path();
    ok( $path,                  "Retrieved include path" );
    ok( -d $path,               "   Include path is an actual directory" );

    ### XXX no more files are bundled this way, it's obsolete    
    #skip "No files actually bundled in perl core", 1 if $ENV{PERL_CORE};
    #ok( -s File::Spec->catfile( $path, $File ),
    #                            "   Found '$File' in include path" );

    ### we don't do this anymore
    #my $out = join '', `$^X -V`; my $qm_path = quotemeta $path;
    #like( $out, qr/$qm_path/s,  "   Path found in perl -V output" );
}

### back to the coderef ###
### try a boring module ###
{   local $CPANPLUS::inc::DEBUG = 1;
    my $warnings; local $SIG{__WARN__} = sub { $warnings .= "@_" };

    my $rv = $code->($code, $Bfile);
    ok( !$rv,                   "Ignoring boring module" );
    ok( !$INC{$Bfile},          "   Boring file not loaded" );
    like( $warnings, qr/CPANPLUS::inc: Not interested in '$Boring'/s,
                                "   Warned about boringness" );
}

### try to load a module with windows paths in it (bug [#11177])
{   local $CPANPLUS::inc::DEBUG = 1;
    my $warnings; local $SIG{__WARN__} = sub { $warnings .= "@_" };

    my $wfile   = 'IO\File.pm';
    my $wmod    = 'IO::File';

    my $rv = $code->($code, $wfile);
    ok( !$rv,                   "Ignoring boring win32 module" );
    ok( !$INC{$wfile},          "   Boring win32 file not loaded" );
    like( $warnings, qr/CPANPLUS::inc: Not interested in '$wmod'/s,
                                "   Warned about boringness" );
}

### try an interesting module ###
{   local $CPANPLUS::inc::DEBUG = 1;
    my $warnings; local $SIG{__WARN__} = sub { $warnings .= "@_" };

    my $rv = $code->($code, $Ufile);
    ok( $rv,                    "Found interesting module" );
    ok( !$INC{$Ufile},          "   Interesting file not loaded" );
    like( $warnings, qr/CPANPLUS::inc: Found match for '$Module'/,
                                "   Match noted in warnings" );
    like( $warnings, qr/CPANPLUS::inc: Best match for '$Module'/,
                                "   Best match noted in warnings" );

    my $contents = do { local $/; <$rv> };
    ok( $contents,              "   Read contents from filehandle" );
    like( $contents, qr/$Module/s,
                                "   Contents is from '$Module'" );
}

### now do some real loading ###
{   use_ok($Module);
    ok( $INC{$Ufile},           "   Regular use of '$Module'" );

    use_ok($Boring);
    ok( $INC{$Bfile},           "   Regular use of '$Boring'" );
}

### check we didn't load our coderef anymore than needed ###
{   my $amount = 5;
    for( 0..$amount ) { CPANPLUS::inc->import; };

    my $flag;
    map { $flag++ if ref $_ eq 'CODE' } @INC[0..$amount];

    my $ok = $amount + 1 == $flag ? 0 : 1;
    ok( $ok,                    "Only loaded coderef into \@INC $flag times");
}

### check limit funcionality
{   local $CPANPLUS::inc::DEBUG = 1;
    my $warnings; local $SIG{__WARN__} = sub { $warnings .= "@_" };

    ### so we can reload it
    delete $INC{$Ufile};
    delete $INC{$Bfile};

    ### limit to the loading of $Boring;
    CPANPLUS::inc->import( $Boring );

    ok( $CPANPLUS::inc::LIMIT{$Boring},
                                "Limit to '$Boring' recorded" );

    ### try a boring file first
    {   my $rv = $code->($code, $Bfile);
        ok( !$rv,               "   '$Boring' still not being loaded" );
        ok( !$INC{$Bfile},      '   Is not in %INC either' );
    }

    ### try an interesting one now
    {   my $rv = $code->( $code, $Ufile );
        ok( !$rv,               "   '$Module' is not being loaded" );
        ok( !$INC{$Ufile},      '   Is not in %INC either' );
        like( $warnings, qr/CPANPLUS::inc: Limits active, '$Module'/s,
                                "   Warned about limits" );
    }

    ### reset limits, try again
    {   local %CPANPLUS::inc::LIMIT;
        ok( !keys(%CPANPLUS::inc::LIMIT),
                                "   Limits removed" );


        my $rv = $code->( $code, $Ufile );
        ok( $rv,                "   '$Module' is being loaded" );

        use_ok( $Module );
        ok( $INC{$Ufile},       '   Present in %INC' );
    }
}

### test limited perl5opt, to include just a few modules
{   my $dash_m  = quotemeta '-MCPANPLUS::inc';
    my $dash_i  = quotemeta '-I' . CPANPLUS::inc->my_path;
    my $orgopt  = quotemeta CPANPLUS::inc->original_perl5opt;

    ### first try an empty string;
    {   my $str = CPANPLUS::inc->limited_perl5opt;
        ok( !$str,              "limited_perl5opt without args is empty" );
    }

    ### try with one 'modules'
    {   my $str = CPANPLUS::inc->limited_perl5opt(qw[A]);
        ok( $str,               "limted perl5opt set for 1 module" );
        like( $str, qr/$dash_m=A\b/,
                                "   -M set properly" );
        like( $str, qr/$dash_i/,"   -I set properly" );
        like( $str, qr/$orgopt/,"   Original opts preserved" );
    }

    ### try with more 'modules'
    {   my $str = CPANPLUS::inc->limited_perl5opt(qw[A B C]);
        ok( $str,               "limted perl5opt set for 3 modules" );
        like( $str, qr/$dash_m=A,B,C\b/,
                                "   -M set properly" );
        like( $str, qr/$dash_i/,"   -I set properly" );
        like( $str, qr/$orgopt/,"   Original opts preserved" );
    }
}




