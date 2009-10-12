BEGIN {
    if ($ENV{PERL_CORE}) {
	chdir 't' if -d 't';
	@INC = ("../lib", "lib/compress");
    }
}

use lib qw(t t/compress);
use strict;
use warnings;
use bytes;

use Test::More ; 
use CompTestUtils;

BEGIN {
    # use Test::NoWarnings, if available
    my $extra = 0 ;
    $extra = 1
        if eval { require Test::NoWarnings ;  import Test::NoWarnings; 1 };

    plan tests => 88 + $extra ;

    use_ok('Scalar::Util');
    use_ok('IO::Compress::Base::Common');
}


ok gotScalarUtilXS(), "Got XS Version of Scalar::Util"
    or diag <<EOM;
You don't have the XS version of Scalar::Util
EOM

# Compress::Zlib::Common;

sub My::testParseParameters()
{
    eval { ParseParameters(1, {}, 1) ; };
    like $@, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    eval { ParseParameters(1, {}, undef) ; };
    like $@, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    eval { ParseParameters(1, {}, []) ; };
    like $@, mkErr(': Expected even number of parameters, got 1'), 
            "Trap odd number of params";

    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_boolean, 0]}, Fred => 'joe') ; };
    like $@, mkErr("Parameter 'Fred' must be an int, got 'joe'"), 
            "wanted unsigned, got undef";

    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_unsigned, 0]}, Fred => undef) ; };
    like $@, mkErr("Parameter 'Fred' must be an unsigned int, got 'undef'"), 
            "wanted unsigned, got undef";

    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_signed, 0]}, Fred => undef) ; };
    like $@, mkErr("Parameter 'Fred' must be a signed int, got 'undef'"), 
            "wanted signed, got undef";

    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_signed, 0]}, Fred => 'abc') ; };
    like $@, mkErr("Parameter 'Fred' must be a signed int, got 'abc'"), 
            "wanted signed, got 'abc'";


    SKIP:
    {
        use Config;

        skip 'readonly + threads', 1
            if $Config{useithreads};

        eval { ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, 0]}, Fred => 'abc') ; };
        like $@, mkErr("Parameter 'Fred' not writable"), 
                "wanted writable, got readonly";
    }

    my @xx;
    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, 0]}, Fred => \@xx) ; };
    like $@, mkErr("Parameter 'Fred' not a scalar reference"), 
            "wanted scalar reference";

    local *ABC;
    eval { ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, 0]}, Fred => *ABC) ; };
    like $@, mkErr("Parameter 'Fred' not a scalar"), 
            "wanted scalar";

    #eval { ParseParameters(1, {'Fred' => [1, 1, Parse_any|Parse_multiple, 0]}, Fred => 1, Fred => 2) ; };
    #like $@, mkErr("Muliple instances of 'Fred' found"),
        #"wanted scalar";

    ok 1;

    my $got = ParseParameters(1, {'Fred' => [1, 1, 0x1000000, 0]}, Fred => 'abc') ;
    is $got->value('Fred'), "abc", "other" ;

    $got = ParseParameters(1, {'Fred' => [0, 1, Parse_any, undef]}, Fred => undef) ;
    ok $got->parsed('Fred'), "undef" ;
    ok ! defined $got->value('Fred'), "undef" ;

    $got = ParseParameters(1, {'Fred' => [0, 1, Parse_string, undef]}, Fred => undef) ;
    ok $got->parsed('Fred'), "undef" ;
    is $got->value('Fred'), "", "empty string" ;

    my $xx;
    $got = ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, undef]}, Fred => $xx) ;

    ok $got->parsed('Fred'), "parsed" ;
    my $xx_ref = $got->value('Fred');
    $$xx_ref = 77 ;
    is $xx, 77;

    $got = ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, undef]}, Fred => \$xx) ;

    ok $got->parsed('Fred'), "parsed" ;
    $xx_ref = $got->value('Fred');

    $$xx_ref = 666 ;
    is $xx, 666;

    {
        my $got1 = ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, undef]}, $got) ;
        is $got1, $got, "Same object";
    
        ok $got1->parsed('Fred'), "parsed" ;
        $xx_ref = $got1->value('Fred');
        
        $$xx_ref = 777 ;
        is $xx, 777;
    }
    
    my $got2 = ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, undef]}, '__xxx__' => $got) ;
    isnt $got2, $got, "not the Same object";

    ok $got2->parsed('Fred'), "parsed" ;
    $xx_ref = $got2->value('Fred');
    $$xx_ref = 888 ;
    is $xx, 888;  
      
    my $other;
    my $got3 = ParseParameters(1, {'Fred' => [1, 1, Parse_writable_scalar, undef]}, '__xxx__' => $got, Fred => \$other) ;
    isnt $got3, $got, "not the Same object";

    ok $got3->parsed('Fred'), "parsed" ;
    $xx_ref = $got3->value('Fred');
    $$xx_ref = 999 ;
    is $other, 999;  
    is $xx, 888;  
}


My::testParseParameters();


{
    title "isaFilename" ;
    ok   isaFilename("abc"), "'abc' isaFilename";

    ok ! isaFilename(undef), "undef ! isaFilename";
    ok ! isaFilename([]),    "[] ! isaFilename";
    $main::X = 1; $main::X = $main::X ;
    ok ! isaFilename(*X),    "glob ! isaFilename";
}

{
    title "whatIsInput" ;

    my $lex = new LexFile my $out_file ;
    open FH, ">$out_file" ;
    is whatIsInput(*FH), 'handle', "Match filehandle" ;
    close FH ;

    my $stdin = '-';
    is whatIsInput($stdin),       'handle',   "Match '-' as stdin";
    #is $stdin,                    \*STDIN,    "'-' changed to *STDIN";
    #isa_ok $stdin,                'IO::File',    "'-' changed to IO::File";
    is whatIsInput("abc"),        'filename', "Match filename";
    is whatIsInput(\"abc"),       'buffer',   "Match buffer";
    is whatIsInput(sub { 1 }, 1), 'code',     "Match code";
    is whatIsInput(sub { 1 }),    ''   ,      "Don't match code";

}

{
    title "whatIsOutput" ;

    my $lex = new LexFile my $out_file ;
    open FH, ">$out_file" ;
    is whatIsOutput(*FH), 'handle', "Match filehandle" ;
    close FH ;

    my $stdout = '-';
    is whatIsOutput($stdout),     'handle',   "Match '-' as stdout";
    #is $stdout,                   \*STDOUT,   "'-' changed to *STDOUT";
    #isa_ok $stdout,               'IO::File',    "'-' changed to IO::File";
    is whatIsOutput("abc"),        'filename', "Match filename";
    is whatIsOutput(\"abc"),       'buffer',   "Match buffer";
    is whatIsOutput(sub { 1 }, 1), 'code',     "Match code";
    is whatIsOutput(sub { 1 }),    ''   ,      "Don't match code";

}

# U64

{
    title "U64" ;

    my $x = new U64();
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0, "  getLow is 0";

    $x = new U64(1,2);
    $x = new U64(1,2);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 2, "  getLow is 2";

    $x = new U64(0xFFFFFFFF,2);
    is $x->getHigh, 0xFFFFFFFF, "  getHigh is 0xFFFFFFFF";
    is $x->getLow, 2, "  getLow is 2";

    $x = new U64(7, 0xFFFFFFFF);
    is $x->getHigh, 7, "  getHigh is 7";
    is $x->getLow, 0xFFFFFFFF, "  getLow is 0xFFFFFFFF";

    $x = new U64(666);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 666, "  getLow is 666";

    title "U64 - add" ;

    $x = new U64(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    $x->add(1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 2, "  getLow is 2";

    $x = new U64(0, 0xFFFFFFFE);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0xFFFFFFFE, "  getLow is 0xFFFFFFFE";

    $x->add(1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 0xFFFFFFFF, "  getLow is 0xFFFFFFFF";

    $x->add(1);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 0, "  getLow is 0";

    $x->add(1);
    is $x->getHigh, 1, "  getHigh is 1";
    is $x->getLow, 1, "  getLow is 1";

    $x = new U64(1, 0xFFFFFFFE);
    my $y = new U64(2, 3);

    $x->add($y);
    is $x->getHigh, 4, "  getHigh is 4";
    is $x->getLow, 1, "  getLow is 1";

    title "U64 - equal" ;

    $x = new U64(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    $y = new U64(0, 1);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    my $z = new U64(0, 2);
    is $x->getHigh, 0, "  getHigh is 0";
    is $x->getLow, 1, "  getLow is 1";

    ok $x->equal($y), "  equal";
    ok !$x->equal($z), "  ! equal";

    title "U64 - pack_V" ;
}
