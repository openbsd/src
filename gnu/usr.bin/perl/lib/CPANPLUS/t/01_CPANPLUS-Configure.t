### make sure we can find our conf.pl file
BEGIN { 
    use FindBin; 
    require "$FindBin::Bin/inc/conf.pl";
}

use Test::More 'no_plan';
use Data::Dumper;
use strict;
use CPANPLUS::Internals::Constants;

my $Config_pm   = 'CPANPLUS/Config.pm';

### DO NOT FLUSH TILL THE END!!! we depend on all warnings being logged..

for my $mod (qw[CPANPLUS::Configure]) {
    use_ok($mod) or diag qq[Can't load $mod];
}    

my $c = CPANPLUS::Configure->new();
isa_ok($c, 'CPANPLUS::Configure');

my $r = $c->conf;
isa_ok( $r, 'CPANPLUS::Config' );


### EU::AI compatibility test ###
{   my $base = $c->_get_build('base');
    ok( defined($base),                 "Base retrieved by old compat API");
    is( $base, $c->get_conf('base'),    "   Value as expected" );
}

for my $cat ( $r->ls_accessors ) {

    ### what field can they take? ###
    my @options = $c->options( type => $cat );

    ### copy for use on the config object itself
    my $accessor    = $cat;
    my $prepend     = ($cat =~ s/^_//) ? '_' : '';
    
    my $getmeth     = $prepend . 'get_'. $cat;
    my $setmeth     = $prepend . 'set_'. $cat;
    my $addmeth     = $prepend . 'add_'. $cat;
    
    ok( scalar(@options),               "Possible options obtained" );
    
    ### test adding keys too ###
    {   my $add_key = 'test_key';
        my $add_val = [1..3];
    
        my $found = grep { $add_key eq $_ } @options;
        ok( !$found,                    "Key '$add_key' not yet defined" );
        ok( $c->$addmeth( $add_key => $add_val ),
                                        "   $addmeth('$add_key' => VAL)" ); 

        ### this one now also exists ###
        push @options, $add_key
    }

    ### poke in the object, get the actual hashref out ### 
    my %hash = map {
        $_ => $r->$accessor->$_     
    } $r->$accessor->ls_accessors;
    
    while( my ($key,$val) = each %hash ) {
        my $is = $c->$getmeth($key); 
        is_deeply( $val, $is,           "deep check for '$key'" );
        ok( $c->$setmeth($key => 1 ),   "   $setmeth('$key' => 1)" );
        is( $c->$getmeth($key), 1,      "   $getmeth('$key')" );
        ok( $c->$setmeth($key => $val), "   $setmeth('$key' => ORGVAL)" );
    }

    ### now check if we found all the keys with options or not ###
    delete $hash{$_} for @options;
    ok( !(scalar keys %hash),          "All possible keys found" );
    
}    


### see if we can save the config ###
{   my $dir     = File::Spec->rel2abs('dummy-cpanplus');
    my $pm      = 'CPANPLUS::Config::Test' . $$;
    my $file    = $c->save( $pm, $dir );
    
    ok( $file,                  "Config $pm saved" );
    ok( -e $file,               "   File exists" );
    ok( -s $file,               "   File has size" );

    ### include our dummy dir when re-scanning
    {   local @INC = ( $dir, @INC );
        ok( $c->init( rescan => 1 ),
                                "Reran ->init()" );
    }
    
    ### make sure this file is now loaded
    ### XXX can't trust bloody dir seperators on Win32 in %INC,
    ### so rather than an exact match, do a grep...
    my ($found) = grep /\bTest$$/, values %INC; 
    ok( $found,                 "   Found $file in \%INC" );
    ok( -e $file,               "   File exists" );
    1 while unlink $file;
    ok(!-e $file,               "       File removed" );
    
}

{   my $env             = ENV_CPANPLUS_CONFIG;
    local $ENV{$env}    = $$;
    my $ok              = $c->init;
    my $stack           = CPANPLUS::Error->stack_as_string;
        
    ok( $ok,                    "Reran init again" );
    like( $stack, qr/Specifying a config file in your environment/,
                                "   Warning logged" );
}


{   CPANPLUS::Error->flush;
    
    {   ### try a bogus method call 
        my $x   = $c->flubber('foo');
        my $err = CPANPLUS::Error->stack_as_string;
        is  ($x, undef,         "Bogus method call returns undef");
        like($err, "/flubber/", "   Bogus method call recognized");
    }
    
    CPANPLUS::Error->flush;
}    


# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
