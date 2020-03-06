use strict;
use warnings;


use Test::More ;

if ( -t STDIN ) {
   plan tests => 7;
}
else {
   plan skip_all => "Need a terminal to test";
}

use Term::ReadKey;
use Fcntl;

$| = 1;

if ( not exists $ENV{COLUMNS} ){
    $ENV{COLUMNS} = 80;
    $ENV{LINES}   = 24;
}

SKIP:
{
    eval {
        if ( $^O =~ /Win32/i ){
            sysopen( IN,  'CONIN$',  O_RDWR ) or die "Unable to open console input:$!";
            sysopen( OUT, 'CONOUT$', O_RDWR ) or die "Unable to open console output:$!";
        }
        else{
            if ( open( IN, "</dev/tty" ) ){
                *OUT = *IN;
                die "Foo" unless -t OUT;
            }
            else{
                die "/dev/tty is absent\n";
            }
        }
    };
    skip( 'Because Term::ReadKey need at least a tty to be useful', 7 ) if $@;
    *IN = *IN;    # Make single-use warning go away
    $|  = 1;
    no strict "subs";
    my $size2 = join( ",", GetTerminalSize("IN") );
    my $size3 = join( ",", GetTerminalSize(*IN) );
    my $size4 = join( ",", GetTerminalSize( \*IN ) );

    my $size_result=0;
    if ( ( $size2 eq $size3 ) && ( $size3 eq $size4 ) ){
        $size_result = 1;
    }
    is($size_result, 1, "Comparing TerminalSize IN");

    my $usable_terminal=0;
    for (my $i = 1; $i < 6; $i++){
        if ( Term::ReadKey::termoptions() == $i ){
            $usable_terminal = 1;
            last;
        }
    }
    is($usable_terminal, 1, "Manipulating the terminal.");

    my @modes;
    eval {
        push( @modes, "O_NODELAY" ) if Term::ReadKey::blockoptions() & 1;
        push( @modes, "poll()" )    if Term::ReadKey::blockoptions() & 2;
        push( @modes, "select()" )  if Term::ReadKey::blockoptions() & 4;
        push( @modes, "Win32" )     if Term::ReadKey::blockoptions() & 8;
    };
    is($@, '', "Check non-blocking read");

    eval {
        my @size = GetTerminalSize(OUT);
    };
    is($@, '', "Check TerminalSize OUT");

    eval {
        my @speeds = GetSpeed();
    };
    is($@, '', "Check Terminal communication speed");

    my %chars;
    eval {
        %chars = GetControlChars(IN);
    };
    is($@, '', "Validate GetControlChars function");

    my %origchars = %chars;
    eval {
        SetControlChars( %origchars, IN );
    };
    is($@, '', "Validate SetControlChars function");
}
