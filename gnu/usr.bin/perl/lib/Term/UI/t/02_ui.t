### Term::UI test suite ###

use strict;
use lib qw[../lib lib];
use Test::More tests => 19;
use Term::ReadLine;

use_ok( 'Term::UI' );

### make sure we can do this automatically ###
$Term::UI::AUTOREPLY    = $Term::UI::AUTOREPLY  = 1;
$Term::UI::VERBOSE      = $Term::UI::VERBOSE    = 0;

### enable warnings
$^W = 1;

### perl core gets upset if we print stuff to STDOUT...
if( $ENV{PERL_CORE} ) {
    *STDOUT_SAVE = *STDOUT_SAVE = *STDOUT;
    close *STDOUT;
    open *STDOUT, ">termui.$$" or diag("Could not open tempfile");
}
END { close *STDOUT && unlink "termui.$$" if $ENV{PERL_CORE} }


### so T::RL doesn't go nuts over no console
BEGIN{ $ENV{LINES}=25; $ENV{COLUMNS}=80; }
my $term = Term::ReadLine->new('test')
                or diag "Could not create a new term. Dying", die;

my $tmpl = {
        prompt  => "What is your favourite colour?",
        choices => [qw|blue red green|],
        default => 'blue',
    };

{
    my $args = \%{ $tmpl };

    is( $term->get_reply( %$args ), 'blue', q[Checking reply with defaults and choices] );
}

{
    my $args = \%{ $tmpl };
    delete $args->{choices};

    is( $term->get_reply( %$args ), 'blue', q[Checking reply with defaults] );
}

{
    my $args = {
        prompt  => 'Do you like cookies?',
        default => 'y',
    };

    is( $term->ask_yn( %$args ), 1, q[Asking yes/no with 'yes' as default] );
}

{
    my $args = {
        prompt  => 'Do you like Python?',
        default => 'n',
    };

    is( $term->ask_yn( %$args ), 0, q[Asking yes/no with 'no' as default] );
}


# used to print: Use of uninitialized value in length at Term/UI.pm line 141.
# [#13412]
{   my $args = {
        prompt  => 'Uninit warning on empty default',
    };
    
    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= "@_" };
    
    my $res = $term->get_reply( %$args );

    ok( !$res,                  "Empty result on autoreply without default" );
    is( $warnings, '',          "   No warnings with empty default" );
    unlike( $warnings, qr|Term.UI|,
                                "   No warnings from Term::UI" );

}
 
# used to print: Use of uninitialized value in string at Params/Check.pm
# [#13412]
{   my $args = {
        prompt  => 'Undef warning on failing allow',
        allow   => sub { 0 },
    };
    
    my $warnings = '';
    local $SIG{__WARN__} = sub { $warnings .= "@_" };
    
    my $res = $term->get_reply( %$args );

    ok( !$res,                  "Empty result on autoreply without default" );
    is( $warnings, '',          "   No warnings with failing allow" );
    unlike( $warnings, qr|Params.Check|,
                                "   No warnings from Params::Check" );

}

#### test parse_options   
{
    my $str =   q[command --no-foo --baz --bar=0 --quux=bleh ] .
                q[--option="some'thing" -one-dash -single=blah' foo bar-zot];

    my $munged = 'command foo bar-zot';
    my $expected = {
            foo         => 0,
            baz         => 1,
            bar         => 0,
            quux        => 'bleh',
            option      => q[some'thing],
            'one-dash'  => 1,
            single      => q[blah'],
    };

    my ($href,$rest) = $term->parse_options( $str );

    is_deeply($href, $expected, qq[Parsing options] );
    is($rest, $munged,          qq[Remaining unparsed string '$munged'] );
}

### more parse_options tests
{   my @map = (
        [ 'x --update_source'   => 'x', { update_source => 1 } ],
        [ '--update_source'     => '',  { update_source => 1 } ],
    );
    
    for my $aref ( @map ) {
        my( $input, $munged, $expect ) = @$aref;
        
        my($href,$rest) = $term->parse_options( $input );
        
        ok( $href,              "Parsed '$input'" );
        is_deeply( $href, $expect,
                                "   Options parsed correctly" );
        is( $rest, $munged,     "   Command parsed correctly" );
    }
}
