#!/usr/bin/perl -w

#use strict vars;

#use Term::ReadKey qw( ReadMode ReadKey );
#my $x;
#ReadMode 3;
#print "Read 1\n";
#$x = ReadKey(0);
#print "X=$x\n";
#print "Read 2\n";
#$x = ReadKey(0);
#print "X=$x\n";
#ReadMode 0;
#__END__;

my $interactive = ( @ARGV && $ARGV[0] =~ /interactive/ );

BEGIN { print "1..8\n"; }
END { print "not ok 1\n" unless $loaded }
use Term::ReadKey;

$loaded = 1;
print "ok 1\n";

use Fcntl;

if ( not exists $ENV{COLUMNS} )
{
    $ENV{COLUMNS} = 80;
    $ENV{LINES}   = 24;
}

if ( $^O =~ /Win32/i )
{
    sysopen( IN,  'CONIN$',  O_RDWR ) or die "Unable to open console input:$!";
    sysopen( OUT, 'CONOUT$', O_RDWR ) or die "Unable to open console output:$!";
}
else
{

    if ( open( IN, "</dev/tty" ) )
    {
        *OUT = *IN;
        die "Foo" unless -t OUT;
    }
    else
    {

        # Okay we are going to cheat a skip
        foreach my $skip ( 2 .. 8 )
        {
            print "ok $skip  # skip /dev/tty is absent\n";
        }
        exit;
    }
}

*IN = *IN;    # Make single-use warning go away
$|  = 1;

# Bad filehandle: IN at ../lib/Term/ReadKey.pm line 377 with \IN and harness
my $size1 = join( ",", GetTerminalSize( -t \IN ? \IN : "IN" ) );
my $size2 = join( ",", GetTerminalSize("IN") );
my $size3 = join( ",", GetTerminalSize(*IN) );
my $size4 = join( ",", GetTerminalSize( \*IN ) );

if ( ( $size1 eq $size2 ) && ( $size2 eq $size3 ) && ( $size3 eq $size4 ) )
{
    print "ok 2\n";
}
else
{
    print "not ok 2\n";
}

sub makenicelist
{
    my (@list) = @_;
    my ( $i, $result );
    $result = "";
    for ( $i = 0 ; $i < @list ; $i++ )
    {
        $result .= ", " if $i > 0;
        $result .= "and " if $i == @list - 1 and @list > 1;
        $result .= $list[$i];
    }
    $result;
}

sub makenice
{
    my ($char) = $_[0];
    if ( ord($char) < 32 ) { $char = "^" . pack( "c", ord($char) + 64 ) }
    elsif ( ord($char) > 126 ) { $char = ord($char) }
    $char;
}

sub makeunnice
{
    my ($char) = $_[0];
    $char =~ s/^\^(.)$/pack("c",ord($1)-64)/eg;
    $char =~ s/(\d{1,3})/pack("c",$1+0)/eg;
    $char;
}

my $response;

eval {

    if ( &Term::ReadKey::termoptions() == 1 )
    {
        $response =
          "Term::ReadKey is using TERMIOS, as opposed to TERMIO or SGTTY.\n";
    }
    elsif ( &Term::ReadKey::termoptions() == 2 )
    {
        $response =
          "Term::ReadKey is using TERMIO, as opposed to TERMIOS or SGTTY.\n";
    }
    elsif ( &Term::ReadKey::termoptions() == 3 )
    {
        $response =
          "Term::ReadKey is using SGTTY, as opposed to TERMIOS or TERMIO.\n";
    }
    elsif ( &Term::ReadKey::termoptions() == 4 )
    {
        $response =
"Term::ReadKey is trying to make do with stty; facilites may be limited.\n";
    }
    elsif ( &Term::ReadKey::termoptions() == 5 )
    {
        $response = "Term::ReadKey is using Win32 functions.\n";
    }
    else
    {
        $response =
          "Term::ReadKey could not find any way to manipulate the terminal.\n";
    }

    print "ok 3\n";
};

print "not ok 3\n" if $@;

print $response if $interactive;

eval {
    push( @modes, "O_NODELAY" ) if &Term::ReadKey::blockoptions() & 1;
    push( @modes, "poll()" )    if &Term::ReadKey::blockoptions() & 2;
    push( @modes, "select()" )  if &Term::ReadKey::blockoptions() & 4;
    push( @modes, "Win32" )     if &Term::ReadKey::blockoptions() & 8;

    print "ok 4\n";
};

print "not ok 4\n" if $@;

if ($interactive)
{
    if ( &Term::ReadKey::blockoptions() == 0 )
    {
        print "No methods found to implement non-blocking reads.\n";
        print
" (If your computer supports poll(), you might like to read through ReadKey.xs)\n";
    }
    else
    {
        print "Non-blocking reads possible via ", makenicelist(@modes), ".\n";
        print $modes[0] . " will be used. " if @modes > 0;
        print $modes[1] . " will be used for timed reads."
          if @modes > 1
          and $modes[0] eq "O_NODELAY";
        print "\n";
    }
}

eval {
    @size = GetTerminalSize(OUT);
    print "ok 5\n";
};

print "not ok 5\n" if $@;

if ($interactive)
{
    if ( !@size )
    {
        print
          "GetTerminalSize was incapable of finding the size of your terminal.";
    }
    else
    {
        print "Using GetTerminalSize, it appears that your terminal is\n";
        print "$size[0] characters wide by $size[1] high.\n\n";
    }

}

eval {
    @speeds = GetSpeed();
    print "ok 6\n";
};

print "not ok 6\n" if $@;

if ($interactive)
{
    if (@speeds)
    {
        print "Apparently, you are connected at ", join( "/", @speeds ),
          " baud.\n";
    }
    else
    {
        print "GetSpeed couldn't tell your connection baud rate.\n\n";
    }
    print "\n";
}

eval {
    %chars = GetControlChars(IN);
    print "ok 7\n";
};

print "not ok 7\n" if $@;

%origchars = %chars;

if ($interactive)
{
    for $c ( keys %chars ) { $chars{$c} = makenice( $chars{$c} ) }

    print "Control chars = (",
      join( ', ', map( "$_ => $chars{$_}", keys %chars ) ), ")\n";
}

eval {
    SetControlChars( %origchars, IN );
    print "ok 8\n";
};

print "not ok 8\n" if $@;

#SetControlChars("FOOFOO"=>"Q");
#SetControlChars("INTERRUPT"=>"\x5");

END { ReadMode 0, IN; }    # Just if something goes weird

exit(0) unless $interactive;

print "\nAnd now for the interactive tests.\n";

print
  "\nThis is ReadMode 1. It's guarranteed to give you cooked input. All the\n";
print "signals and editing characters may be used as usual.\n";

ReadMode 1, IN;

print "\nYou may enter some text here: ";

$t = ReadLine 0, IN;

chop $t;

print "\nYou entered `$t'.\n";

ReadMode 2, IN;

print
  "\nThis is ReadMode 2. It's just like #1, but echo is turned off. Great\n";
print "for passwords.\n";

print "\nYou may enter some invisible text here: ";

$t = ReadLine 0, IN;

chop $t;

print "\nYou entered `$t'.\n";

ReadMode 3, IN;

print
  "\nI won't demonstrate ReadMode 3 here. It's your standard cbreak mode,\n";
print
  "with editing characters disabled, single character at a time input, but\n";
print "with the control characters still enabled.\n";

print "\n";

print
"I'm now putting the terminal into ReadMode 4 and using non-blocking reads.\n";
print
  "All signals should be disabled, including xon-xoff. You should only be\n";
print "able to exit this loop via 'q'.\n";

ReadMode 4, IN;
$k = "";

#$in = *STDIN;
$in = \*IN;    # or *IN or "IN"
while ( $k ne "q" )
{
    print "Press a key, or \"q\" to stop: ";
    $count = 0;

    #print "IN = $in\n";
    $count++ while !defined( $k = ReadKey( -1, $in ) );

    #print "IN2 = $in\n";
    print "\nYou pressed `", makenice($k),
      "' after the loop rolled over $count times\n";
}
ReadMode 0, IN;

print "\nHere is a similar loop which times out after two seconds:\n";

ReadMode 4, IN;
$k = "";

#$in = *STDIN;
$in = \*IN;    # or *IN or "IN"
while ( $k ne "q" )
{
    print "Press a key, or \"q\" to stop: ";
    $count = 0;

    #print "IN = $in\n";
    print "Timeout! " while !defined( $k = ReadKey( 2, $in ) );

    #print "IN2 = $in\n";
    print "\nYou pressed `", makenice($k), "'\n";
}

print
  "\nLastly, ReadMode 5, which also affects output (except under Win32).\n\n";

ReadMode 5, IN;

print
"This should be a diagonal line (except under Win32): *\n*\n*\n\*\n*\n*\r\n\r\n";
print "And this should be a moving spot:\r\n\r\n";

$width = ( GetTerminalSize(OUT) )[0];
$width /= 2;
$width--;
if ( $width < 10 ) { $width = 10; }

for ( $i = 0 ; $i < 20 ; $i += .15 )
{
    print "\r";
    print( " " x ( ( cos($i) + 1 ) * $width ) );
    print "*";
    select( undef, undef, undef, 0.01 );
    print "\r";
    print( " " x ( ( cos($i) + 1 ) * $width ) );
    print " ";
}
print "\r                                           ";

print "\n\r\n";

ReadMode 0, IN;

print "That's all, folks!\n";

