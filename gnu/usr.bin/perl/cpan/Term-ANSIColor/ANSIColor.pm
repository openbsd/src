# Term::ANSIColor -- Color screen output using ANSI escape sequences.
#
# Copyright 1996, 1997, 1998, 2000, 2001, 2002, 2005, 2006, 2008, 2009, 2010,
#     2011 Russ Allbery <rra@stanford.edu> and Zenin
# PUSH/POP support submitted 2007 by openmethods.com voice solutions
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Ah, September, when the sysadmins turn colors and fall off the trees....
#                               -- Dave Van Domelen

##############################################################################
# Modules and declarations
##############################################################################

package Term::ANSIColor;
require 5.001;

$VERSION = '3.01';

use strict;
use vars qw($AUTOLOAD $AUTOLOCAL $AUTORESET @COLORLIST @COLORSTACK $EACHLINE
            @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $VERSION %ATTRIBUTES
            %ATTRIBUTES_R);

use Exporter ();
BEGIN {
    @COLORLIST = qw(
        CLEAR           RESET             BOLD            DARK
        FAINT           UNDERLINE         UNDERSCORE      BLINK
        REVERSE         CONCEALED

        BLACK           RED               GREEN           YELLOW
        BLUE            MAGENTA           CYAN            WHITE
        ON_BLACK        ON_RED            ON_GREEN        ON_YELLOW
        ON_BLUE         ON_MAGENTA        ON_CYAN         ON_WHITE

        BRIGHT_BLACK    BRIGHT_RED        BRIGHT_GREEN    BRIGHT_YELLOW
        BRIGHT_BLUE     BRIGHT_MAGENTA    BRIGHT_CYAN     BRIGHT_WHITE
        ON_BRIGHT_BLACK ON_BRIGHT_RED     ON_BRIGHT_GREEN ON_BRIGHT_YELLOW
        ON_BRIGHT_BLUE  ON_BRIGHT_MAGENTA ON_BRIGHT_CYAN  ON_BRIGHT_WHITE
    );
    @ISA         = qw(Exporter);
    @EXPORT      = qw(color colored);
    @EXPORT_OK   = qw(uncolor colorstrip colorvalid);
    %EXPORT_TAGS = (constants => \@COLORLIST,
                    pushpop   => [ @COLORLIST,
                                   qw(PUSHCOLOR POPCOLOR LOCALCOLOR) ]);
    Exporter::export_ok_tags ('pushpop');
}

##############################################################################
# Internal data structures
##############################################################################

%ATTRIBUTES = ('clear'          => 0,
               'reset'          => 0,
               'bold'           => 1,
               'dark'           => 2,
               'faint'          => 2,
               'underline'      => 4,
               'underscore'     => 4,
               'blink'          => 5,
               'reverse'        => 7,
               'concealed'      => 8,

               'black'          => 30,   'on_black'          => 40,
               'red'            => 31,   'on_red'            => 41,
               'green'          => 32,   'on_green'          => 42,
               'yellow'         => 33,   'on_yellow'         => 43,
               'blue'           => 34,   'on_blue'           => 44,
               'magenta'        => 35,   'on_magenta'        => 45,
               'cyan'           => 36,   'on_cyan'           => 46,
               'white'          => 37,   'on_white'          => 47,

               'bright_black'   => 90,   'on_bright_black'   => 100,
               'bright_red'     => 91,   'on_bright_red'     => 101,
               'bright_green'   => 92,   'on_bright_green'   => 102,
               'bright_yellow'  => 93,   'on_bright_yellow'  => 103,
               'bright_blue'    => 94,   'on_bright_blue'    => 104,
               'bright_magenta' => 95,   'on_bright_magenta' => 105,
               'bright_cyan'    => 96,   'on_bright_cyan'    => 106,
               'bright_white'   => 97,   'on_bright_white'   => 107,
               );

# Reverse lookup.  Alphabetically first name for a sequence is preferred.
for (reverse sort keys %ATTRIBUTES) {
    $ATTRIBUTES_R{$ATTRIBUTES{$_}} = $_;
}

##############################################################################
# Implementation (constant form)
##############################################################################

# Time to have fun!  We now want to define the constant subs, which are named
# the same as the attributes above but in all caps.  Each constant sub needs
# to act differently depending on whether $AUTORESET is set.  Without
# autoreset:
#
#     BLUE "text\n"  ==>  "\e[34mtext\n"
#
# If $AUTORESET is set, we should instead get:
#
#     BLUE "text\n"  ==>  "\e[34mtext\n\e[0m"
#
# The sub also needs to handle the case where it has no arguments correctly.
# Maintaining all of this as separate subs would be a major nightmare, as well
# as duplicate the %ATTRIBUTES hash, so instead we define an AUTOLOAD sub to
# define the constant subs on demand.  To do that, we check the name of the
# called sub against the list of attributes, and if it's an all-caps version
# of one of them, we define the sub on the fly and then run it.
#
# If the environment variable ANSI_COLORS_DISABLED is set, just return the
# arguments without adding any escape sequences.  This is to make it easier to
# write scripts that also work on systems without any ANSI support, like
# Windows consoles.
sub AUTOLOAD {
    if (defined $ENV{ANSI_COLORS_DISABLED}) {
        return join ('', @_);
    }
    if ($AUTOLOAD =~ /^([\w:]*::([A-Z_]+))$/ and defined $ATTRIBUTES{lc $2}) {
        $AUTOLOAD = $1;
        my $attr = "\e[" . $ATTRIBUTES{lc $2} . 'm';
        eval qq {
            sub $AUTOLOAD {
                if (\$AUTORESET && \@_) {
                    return '$attr' . join ('', \@_) . "\e[0m";
                } elsif (\$AUTOLOCAL && \@_) {
                    return PUSHCOLOR ('$attr') . join ('', \@_) . POPCOLOR;
                } else {
                    return '$attr' . join ('', \@_);
                }
            }
        };
        goto &$AUTOLOAD;
    } else {
        require Carp;
        Carp::croak ("undefined subroutine &$AUTOLOAD called");
    }
}

# Append a new color to the top of the color stack and return the top of
# the stack.
sub PUSHCOLOR {
    my ($text) = @_;
    my ($color) = ($text =~ m/^((?:\e\[[\d;]+m)+)/);
    if (@COLORSTACK) {
        $color = $COLORSTACK[-1] . $color;
    }
    push (@COLORSTACK, $color);
    return $text;
}

# Pop the color stack and return the new top of the stack (or reset, if
# the stack is empty).
sub POPCOLOR {
    pop @COLORSTACK;
    if (@COLORSTACK) {
        return $COLORSTACK[-1] . join ('', @_);
    } else {
        return RESET (@_);
    }
}

# Surround arguments with a push and a pop.
sub LOCALCOLOR {
    return PUSHCOLOR (join ('', @_)) . POPCOLOR ();
}

##############################################################################
# Implementation (attribute string form)
##############################################################################

# Return the escape code for a given set of color attributes.
sub color {
    return '' if defined $ENV{ANSI_COLORS_DISABLED};
    my @codes = map { split } @_;
    my $attribute = '';
    foreach (@codes) {
        $_ = lc $_;
        unless (defined $ATTRIBUTES{$_}) {
            require Carp;
            Carp::croak ("Invalid attribute name $_");
        }
        $attribute .= $ATTRIBUTES{$_} . ';';
    }
    chop $attribute;
    return ($attribute ne '') ? "\e[${attribute}m" : undef;
}

# Return a list of named color attributes for a given set of escape codes.
# Escape sequences can be given with or without enclosing "\e[" and "m".  The
# empty escape sequence '' or "\e[m" gives an empty list of attrs.
sub uncolor {
    my (@nums, @result);
    for (@_) {
        my $escape = $_;
        $escape =~ s/^\e\[//;
        $escape =~ s/m$//;
        unless ($escape =~ /^((?:\d+;)*\d*)$/) {
            require Carp;
            Carp::croak ("Bad escape sequence $escape");
        }
        push (@nums, split (/;/, $1));
    }
    for (@nums) {
        $_ += 0; # Strip leading zeroes
        my $name = $ATTRIBUTES_R{$_};
        if (!defined $name) {
            require Carp;
            Carp::croak ("No name for escape sequence $_" );
        }
        push (@result, $name);
    }
    return @result;
}

# Given a string and a set of attributes, returns the string surrounded by
# escape codes to set those attributes and then clear them at the end of the
# string.  The attributes can be given either as an array ref as the first
# argument or as a list as the second and subsequent arguments.  If $EACHLINE
# is set, insert a reset before each occurrence of the string $EACHLINE and
# the starting attribute code after the string $EACHLINE, so that no attribute
# crosses line delimiters (this is often desirable if the output is to be
# piped to a pager or some other program).
sub colored {
    my ($string, @codes);
    if (ref ($_[0]) && ref ($_[0]) eq 'ARRAY') {
        @codes = @{+shift};
        $string = join ('', @_);
    } else {
        $string = shift;
        @codes = @_;
    }
    return $string if defined $ENV{ANSI_COLORS_DISABLED};
    if (defined $EACHLINE) {
        my $attr = color (@codes);
        return join '',
            map { ($_ ne $EACHLINE) ? $attr . $_ . "\e[0m" : $_ }
                grep { length ($_) > 0 }
                    split (/(\Q$EACHLINE\E)/, $string);
    } else {
        return color (@codes) . $string . "\e[0m";
    }
}

# Given a string, strip the ANSI color codes out of that string and return the
# result.  This removes only ANSI color codes, not movement codes and other
# escape sequences.
sub colorstrip {
    my (@string) = @_;
    for my $string (@string) {
        $string =~ s/\e\[[\d;]*m//g;
    }
    return wantarray ? @string : join ('', @string);
}

# Given a list of color attributes (arguments for color, for instance), return
# true if they're all valid or false if any of them are invalid.
sub colorvalid {
    my @codes = map { split } @_;
    for (@codes) {
        unless (defined $ATTRIBUTES{lc $_}) {
            return;
        }
    }
    return 1;
}

##############################################################################
# Module return value and documentation
##############################################################################

# Ensure we evaluate to true.
1;
__END__

=head1 NAME

Term::ANSIColor - Color screen output using ANSI escape sequences

=for stopwords
cyan colorize namespace runtime TMTOWTDI cmd.exe 4nt.exe command.com NT
ESC Delvare SSH OpenSSH aixterm ECMA-048 Fraktur overlining Zenin
reimplemented Allbery PUSHCOLOR POPCOLOR LOCALCOLOR openmethods.com
grey ATTR

=head1 SYNOPSIS

    use Term::ANSIColor;
    print color 'bold blue';
    print "This text is bold blue.\n";
    print color 'reset';
    print "This text is normal.\n";
    print colored ("Yellow on magenta.", 'yellow on_magenta'), "\n";
    print "This text is normal.\n";
    print colored ['yellow on_magenta'], 'Yellow on magenta.', "\n";
    print colored ['red on_bright_yellow'], 'Red on bright yellow.', "\n";
    print colored ['bright_red on_black'], 'Bright red on black.', "\n";
    print "\n";

    use Term::ANSIColor qw(uncolor);
    print uncolor ('01;31'), "\n";

    use Term::ANSIColor qw(colorstrip);
    print colorstrip '\e[1mThis is bold\e[0m', "\n";

    use Term::ANSIColor qw(colorvalid);
    my $valid = colorvalid ('blue bold', 'on_magenta');
    print "Color string is ", $valid ? "valid\n" : "invalid\n";

    use Term::ANSIColor qw(:constants);
    print BOLD, BLUE, "This text is in bold blue.\n", RESET;

    use Term::ANSIColor qw(:constants);
    {
        local $Term::ANSIColor::AUTORESET = 1;
        print BOLD BLUE "This text is in bold blue.\n";
        print "This text is normal.\n";
    }

    use Term::ANSIColor qw(:pushpop);
    print PUSHCOLOR RED ON_GREEN "This text is red on green.\n";
    print PUSHCOLOR BRIGHT_BLUE "This text is bright blue on green.\n";
    print RESET BRIGHT_BLUE "This text is just bright blue.\n";
    print POPCOLOR "Back to red on green.\n";
    print LOCALCOLOR GREEN ON_BLUE "This text is green on blue.\n";
    print "This text is red on green.\n";
    {
        local $Term::ANSIColor::AUTOLOCAL = 1;
        print ON_BLUE "This text is red on blue.\n";
        print "This text is red on green.\n";
    }
    print POPCOLOR "Back to whatever we started as.\n";

=head1 DESCRIPTION

This module has two interfaces, one through color() and colored() and the
other through constants.  It also offers the utility functions uncolor(),
colorstrip(), and colorvalid(), which have to be explicitly imported to be
used (see L</SYNOPSIS>).

=head2 Supported Colors

Terminal emulators that support color divide into two types: ones that
support only eight colors, and ones that support sixteen.  This module
provides both the ANSI escape codes for the "normal" colors, supported by
both types, as well as the additional colors supported by sixteen-color
emulators.  These colors are referred to as ANSI colors 0 through 7
(normal) and 8 through 15.

Unfortunately, interpretation of colors 0 through 7 often depends on
whether the emulator supports eight colors or sixteen colors.  Emulators
that only support eight colors (such as the Linux console) will display
colors 0 through 7 with normal brightness and ignore colors 8 through 15,
treating them the same as white.  Emulators that support 16 colors, such
as gnome-terminal, normally display colors 0 through 7 as dim or darker
versions and colors 8 through 15 as normal brightness.  On such emulators,
the "normal" white (color 7) usually is shown as pale grey, requiring
bright white (15) to be used to get a real white color.  Bright black
usually is a dark grey color, although some terminals display it as pure
black.  Some sixteen-color terminal emulators also treat normal yellow
(color 3) as orange or brown, and bright yellow (color 11) as yellow.

Following the normal convention of sixteen-color emulators, this module
provides a pair of attributes for each color.  For every normal color (0
through 7), the corresponding bright color (8 through 15) is obtained by
prepending the string C<bright_> to the normal color name.  For example,
C<red> is color 1 and C<bright_red> is color 9.  The same applies for
background colors: C<on_red> is the normal color and C<on_bright_red> is
the bright color.  Capitalize these strings for the constant interface.

There is unfortunately no way to know whether the current emulator
supports sixteen colors or not, which makes the choice of colors
difficult.  The most conservative choice is to use only the regular
colors, which are at least displayed on all emulators.  However, they will
appear dark in sixteen-color terminal emulators, including most common
emulators in UNIX X environments.  If you know the display is one of those
emulators, you may wish to use the bright variants instead.  Even better,
offer the user a way to configure the colors for a given application to
fit their terminal emulator.

Support for colors 8 through 15 (the C<bright_> variants) was added in
Term::ANSIColor 3.0.

=head2 Function Interface

The function interface uses attribute strings to describe the colors and
text attributes to assign to text.  The recognized non-color attributes
are clear, reset, bold, dark, faint, underline, underscore, blink,
reverse, and concealed.  Clear and reset (reset to default attributes),
dark and faint (dim and saturated), and underline and underscore are
equivalent, so use whichever is the most intuitive to you.

Note that not all attributes are supported by all terminal types, and some
terminals may not support any of these sequences.  Dark and faint, blink,
and concealed in particular are frequently not implemented.

The recognized normal foreground color attributes (colors 0 to 7) are:

  black  red  green  yellow  blue  magenta  cyan  white

The corresponding bright foreground color attributes (colors 8 to 15) are:

  bright_black  bright_red      bright_green  bright_yellow
  bright_blue   bright_magenta  bright_cyan   bright_white

The recognized normal background color attributes (colors 0 to 7) are:

  on_black  on_red      on_green  on yellow
  on_blue   on_magenta  on_cyan   on_white

The recognized bright background color attributes (colors 8 to 15) are:

  on_bright_black  on_bright_red      on_bright_green  on_bright_yellow
  on_bright_blue   on_bright_magenta  on_bright_cyan   on_bright_white

For any of the above listed attributes, case is not significant.

Attributes, once set, last until they are unset (by printing the attribute
C<clear> or C<reset>).  Be careful to do this, or otherwise your attribute
will last after your script is done running, and people get very annoyed
at having their prompt and typing changed to weird colors.

=over 4

=item color(ATTR[, ATTR ...])

color() takes any number of strings as arguments and considers them to be
space-separated lists of attributes.  It then forms and returns the escape
sequence to set those attributes.  It doesn't print it out, just returns
it, so you'll have to print it yourself if you want to.  This is so that
you can save it as a string, pass it to something else, send it to a file
handle, or do anything else with it that you might care to.  color()
throws an exception if given an invalid attribute.

=item colored(STRING, ATTRIBUTES)

=item colored(ATTR-REF, STRING[, STRING...])

As an aid in resetting colors, colored() takes a scalar as the first
argument and any number of attribute strings as the second argument and
returns the scalar wrapped in escape codes so that the attributes will be
set as requested before the string and reset to normal after the string.
Alternately, you can pass a reference to an array as the first argument,
and then the contents of that array will be taken as attributes and color
codes and the remainder of the arguments as text to colorize.

Normally, colored() just puts attribute codes at the beginning and end of
the string, but if you set $Term::ANSIColor::EACHLINE to some string, that
string will be considered the line delimiter and the attribute will be set
at the beginning of each line of the passed string and reset at the end of
each line.  This is often desirable if the output contains newlines and
you're using background colors, since a background color that persists
across a newline is often interpreted by the terminal as providing the
default background color for the next line.  Programs like pagers can also
be confused by attributes that span lines.  Normally you'll want to set
$Term::ANSIColor::EACHLINE to C<"\n"> to use this feature.

=item uncolor(ESCAPE)

uncolor() performs the opposite translation as color(), turning escape
sequences into a list of strings corresponding to the attributes being set
by those sequences.

=item colorstrip(STRING[, STRING ...])

colorstrip() removes all color escape sequences from the provided strings,
returning the modified strings separately in array context or joined
together in scalar context.  Its arguments are not modified.

=item colorvalid(ATTR[, ATTR ...])

colorvalid() takes attribute strings the same as color() and returns true
if all attributes are known and false otherwise.

=back

=head2 Constant Interface

Alternately, if you import C<:constants>, you can use the following
constants directly:

  CLEAR           RESET             BOLD            DARK
  FAINT           UNDERLINE         UNDERSCORE      BLINK
  REVERSE         CONCEALED

  BLACK           RED               GREEN           YELLOW
  BLUE            MAGENTA           CYAN            WHITE
  BRIGHT_BLACK    BRIGHT_RED        BRIGHT_GREEN    BRIGHT_YELLOW
  BRIGHT_BLUE     BRIGHT_MAGENTA    BRIGHT_CYAN     BRIGHT_WHITE

  ON_BLACK        ON_RED            ON_GREEN        ON_YELLOW
  ON_BLUE         ON_MAGENTA        ON_CYAN         ON_WHITE
  ON_BRIGHT_BLACK ON_BRIGHT_RED     ON_BRIGHT_GREEN ON_BRIGHT_YELLOW
  ON_BRIGHT_BLUE  ON_BRIGHT_MAGENTA ON_BRIGHT_CYAN  ON_BRIGHT_WHITE

These are the same as color('attribute') and can be used if you prefer
typing:

    print BOLD BLUE ON_WHITE "Text", RESET, "\n";

to

    print colored ("Text", 'bold blue on_white'), "\n";

(Note that the newline is kept separate to avoid confusing the terminal as
described above since a background color is being used.)

When using the constants, if you don't want to have to remember to add the
C<, RESET> at the end of each print line, you can set
$Term::ANSIColor::AUTORESET to a true value.  Then, the display mode will
automatically be reset if there is no comma after the constant.  In other
words, with that variable set:

    print BOLD BLUE "Text\n";

will reset the display mode afterward, whereas:

    print BOLD, BLUE, "Text\n";

will not.  If you are using background colors, you will probably want to
print the newline with a separate print statement to avoid confusing the
terminal.

The subroutine interface has the advantage over the constants interface in
that only two subroutines are exported into your namespace, versus
thirty-eight in the constants interface.  On the flip side, the constants
interface has the advantage of better compile time error checking, since
misspelled names of colors or attributes in calls to color() and colored()
won't be caught until runtime whereas misspelled names of constants will
be caught at compile time.  So, pollute your namespace with almost two
dozen subroutines that you may not even use that often, or risk a silly
bug by mistyping an attribute.  Your choice, TMTOWTDI after all.

=head2 The Color Stack

As of Term::ANSIColor 2.0, you can import C<:pushpop> and maintain a stack
of colors using PUSHCOLOR, POPCOLOR, and LOCALCOLOR.  PUSHCOLOR takes the
attribute string that starts its argument and pushes it onto a stack of
attributes.  POPCOLOR removes the top of the stack and restores the
previous attributes set by the argument of a prior PUSHCOLOR.  LOCALCOLOR
surrounds its argument in a PUSHCOLOR and POPCOLOR so that the color
resets afterward.

When using PUSHCOLOR, POPCOLOR, and LOCALCOLOR, it's particularly
important to not put commas between the constants.

    print PUSHCOLOR BLUE "Text\n";

will correctly push BLUE onto the top of the stack.

    print PUSHCOLOR, BLUE, "Text\n";    # wrong!

will not, and a subsequent pop won't restore the correct attributes.
PUSHCOLOR pushes the attributes set by its argument, which is normally a
string of color constants.  It can't ask the terminal what the current
attributes are.

=head1 DIAGNOSTICS

=over 4

=item Bad escape sequence %s

(F) You passed an invalid ANSI escape sequence to uncolor().

=item Bareword "%s" not allowed while "strict subs" in use

(F) You probably mistyped a constant color name such as:

    $Foobar = FOOBAR . "This line should be blue\n";

or:

    @Foobar = FOOBAR, "This line should be blue\n";

This will only show up under use strict (another good reason to run under
use strict).

=item Invalid attribute name %s

(F) You passed an invalid attribute name to either color() or colored().

=item Name "%s" used only once: possible typo

(W) You probably mistyped a constant color name such as:

    print FOOBAR "This text is color FOOBAR\n";

It's probably better to always use commas after constant names in order to
force the next error.

=item No comma allowed after filehandle

(F) You probably mistyped a constant color name such as:

    print FOOBAR, "This text is color FOOBAR\n";

Generating this fatal compile error is one of the main advantages of using
the constants interface, since you'll immediately know if you mistype a
color name.

=item No name for escape sequence %s

(F) The ANSI escape sequence passed to uncolor() contains escapes which
aren't recognized and can't be translated to names.

=back

=head1 ENVIRONMENT

=over 4

=item ANSI_COLORS_DISABLED

If this environment variable is set, all of the functions defined by this
module (color(), colored(), and all of the constants not previously used
in the program) will not output any escape sequences and instead will just
return the empty string or pass through the original text as appropriate.
This is intended to support easy use of scripts using this module on
platforms that don't support ANSI escape sequences.

For it to have its proper effect, this environment variable must be set
before any color constants are used in the program.

=back

=head1 RESTRICTIONS

It would be nice if one could leave off the commas around the constants
entirely and just say:

    print BOLD BLUE ON_WHITE "Text\n" RESET;

but the syntax of Perl doesn't allow this.  You need a comma after the
string.  (Of course, you may consider it a bug that commas between all the
constants aren't required, in which case you may feel free to insert
commas unless you're using $Term::ANSIColor::AUTORESET or
PUSHCOLOR/POPCOLOR.)

For easier debugging, you may prefer to always use the commas when not
setting $Term::ANSIColor::AUTORESET or PUSHCOLOR/POPCOLOR so that you'll
get a fatal compile error rather than a warning.

It's not possible to use this module to embed formatting and color
attributes using Perl formats.  They replace the escape character with a
space (as documented in L<perlform(1)>), resulting in garbled output from
the unrecognized attribute.  Even if there were a way around that problem,
the format doesn't know that the non-printing escape sequence is
zero-length and would incorrectly format the output.  For formatted output
using color or other attributes, either use sprintf() instead or use
formline() and then add the color or other attributes after formatting and
before output.

=head1 NOTES

The codes generated by this module are standard terminal control codes,
complying with ECMA-048 and ISO 6429 (generally referred to as "ANSI
color" for the color codes).  The non-color control codes (bold, dark,
italic, underline, and reverse) are part of the earlier ANSI X3.64
standard for control sequences for video terminals and peripherals.

Note that not all displays are ISO 6429-compliant, or even X3.64-compliant
(or are even attempting to be so).  This module will not work as expected
on displays that do not honor these escape sequences, such as cmd.exe,
4nt.exe, and command.com under either Windows NT or Windows 2000.  They
may just be ignored, or they may display as an ESC character followed by
some apparent garbage.

Jean Delvare provided the following table of different common terminal
emulators and their support for the various attributes and others have
helped me flesh it out:

              clear    bold     faint   under    blink   reverse  conceal
 ------------------------------------------------------------------------
 xterm         yes      yes      no      yes      yes      yes      yes
 linux         yes      yes      yes    bold      yes      yes      no
 rxvt          yes      yes      no      yes  bold/black   yes      no
 dtterm        yes      yes      yes     yes    reverse    yes      yes
 teraterm      yes    reverse    no      yes    rev/red    yes      no
 aixterm      kinda   normal     no      yes      no       yes      yes
 PuTTY         yes     color     no      yes      no       yes      no
 Windows       yes      no       no      no       no       yes      no
 Cygwin SSH    yes      yes      no     color    color    color     yes
 Mac Terminal  yes      yes      no      yes      yes      yes      yes

Windows is Windows telnet, Cygwin SSH is the OpenSSH implementation under
Cygwin on Windows NT, and Mac Terminal is the Terminal application in Mac
OS X.  Where the entry is other than yes or no, that emulator displays the
given attribute as something else instead.  Note that on an aixterm, clear
doesn't reset colors; you have to explicitly set the colors back to what
you want.  More entries in this table are welcome.

Note that codes 3 (italic), 6 (rapid blink), and 9 (strike-through) are
specified in ANSI X3.64 and ECMA-048 but are not commonly supported by
most displays and emulators and therefore aren't supported by this module
at the present time.  ECMA-048 also specifies a large number of other
attributes, including a sequence of attributes for font changes, Fraktur
characters, double-underlining, framing, circling, and overlining.  As
none of these attributes are widely supported or useful, they also aren't
currently supported by this module.

=head1 SEE ALSO

ECMA-048 is available on-line (at least at the time of this writing) at
L<http://www.ecma-international.org/publications/standards/Ecma-048.htm>.

ISO 6429 is available from ISO for a charge; the author of this module
does not own a copy of it.  Since the source material for ISO 6429 was
ECMA-048 and the latter is available for free, there seems little reason
to obtain the ISO standard.

The current version of this module is always available from its web site
at L<http://www.eyrie.org/~eagle/software/ansicolor/>.  It is also part of
the Perl core distribution as of 5.6.0.

=head1 AUTHORS

Original idea (using constants) by Zenin, reimplemented using subs by Russ
Allbery <rra@stanford.edu>, and then combined with the original idea by
Russ with input from Zenin.  Russ Allbery now maintains this module.

=head1 COPYRIGHT AND LICENSE

Copyright 1996, 1997, 1998, 2000, 2001, 2002, 2005, 2006, 2008, 2009, 2010,
2011 Russ Allbery <rra@stanford.edu> and Zenin.  This program is free
software; you may redistribute it and/or modify it under the same terms as
Perl itself.

PUSHCOLOR, POPCOLOR, and LOCALCOLOR were contributed by openmethods.com
voice solutions.

=cut
