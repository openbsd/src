# GetOpt::Long.pm -- Universal options parsing

package Getopt::Long;

# RCS Status      : $Id: Long.pm,v 1.2 1997/11/30 07:57:41 millert Exp $
# Author          : Johan Vromans
# Created On      : Tue Sep 11 15:00:12 1990
# Last Modified By: Johan Vromans
# Last Modified On: Wed Sep 17 12:20:10 1997
# Update Count    : 608
# Status          : Released

=head1 NAME

GetOptions - extended processing of command line options

=head1 SYNOPSIS

  use Getopt::Long;
  $result = GetOptions (...option-descriptions...);

=head1 DESCRIPTION

The Getopt::Long module implements an extended getopt function called
GetOptions(). This function adheres to the POSIX syntax for command
line options, with GNU extensions. In general, this means that options
have long names instead of single letters, and are introduced with a
double dash "--". Support for bundling of command line options, as was
the case with the more traditional single-letter approach, is provided
but not enabled by default. For example, the UNIX "ps" command can be
given the command line "option"

  -vax

which means the combination of B<-v>, B<-a> and B<-x>. With the new
syntax B<--vax> would be a single option, probably indicating a
computer architecture. 

Command line options can be used to set values. These values can be
specified in one of two ways:

  --size 24
  --size=24

GetOptions is called with a list of option-descriptions, each of which
consists of two elements: the option specifier and the option linkage.
The option specifier defines the name of the option and, optionally,
the value it can take. The option linkage is usually a reference to a
variable that will be set when the option is used. For example, the
following call to GetOptions:

  GetOptions("size=i" => \$offset);

will accept a command line option "size" that must have an integer
value. With a command line of "--size 24" this will cause the variable
$offset to get the value 24.

Alternatively, the first argument to GetOptions may be a reference to
a HASH describing the linkage for the options, or an object whose
class is based on a HASH. The following call is equivalent to the
example above:

  %optctl = ("size" => \$offset);
  GetOptions(\%optctl, "size=i");

Linkage may be specified using either of the above methods, or both.
Linkage specified in the argument list takes precedence over the
linkage specified in the HASH.

The command line options are taken from array @ARGV. Upon completion
of GetOptions, @ARGV will contain the rest (i.e. the non-options) of
the command line.
 
Each option specifier designates the name of the option, optionally
followed by an argument specifier. Values for argument specifiers are:

=over 8

=item E<lt>noneE<gt>

Option does not take an argument. 
The option variable will be set to 1.

=item !

Option does not take an argument and may be negated, i.e. prefixed by
"no". E.g. "foo!" will allow B<--foo> (with value 1) and B<-nofoo>
(with value 0).
The option variable will be set to 1, or 0 if negated.

=item =s

Option takes a mandatory string argument.
This string will be assigned to the option variable.
Note that even if the string argument starts with B<-> or B<-->, it
will not be considered an option on itself.

=item :s

Option takes an optional string argument.
This string will be assigned to the option variable.
If omitted, it will be assigned "" (an empty string).
If the string argument starts with B<-> or B<-->, it
will be considered an option on itself.

=item =i

Option takes a mandatory integer argument.
This value will be assigned to the option variable.
Note that the value may start with B<-> to indicate a negative
value. 

=item :i

Option takes an optional integer argument.
This value will be assigned to the option variable.
If omitted, the value 0 will be assigned.
Note that the value may start with B<-> to indicate a negative
value.

=item =f

Option takes a mandatory real number argument.
This value will be assigned to the option variable.
Note that the value may start with B<-> to indicate a negative
value.

=item :f

Option takes an optional real number argument.
This value will be assigned to the option variable.
If omitted, the value 0 will be assigned.

=back

A lone dash B<-> is considered an option, the corresponding option
name is the empty string.

A double dash on itself B<--> signals end of the options list.

=head2 Linkage specification

The linkage specifier is optional. If no linkage is explicitly
specified but a ref HASH is passed, GetOptions will place the value in
the HASH. For example:

  %optctl = ();
  GetOptions (\%optctl, "size=i");

will perform the equivalent of the assignment

  $optctl{"size"} = 24;

For array options, a reference to an array is used, e.g.:

  %optctl = ();
  GetOptions (\%optctl, "sizes=i@");

with command line "-sizes 24 -sizes 48" will perform the equivalent of
the assignment

  $optctl{"sizes"} = [24, 48];

For hash options (an option whose argument looks like "name=value"),
a reference to a hash is used, e.g.:

  %optctl = ();
  GetOptions (\%optctl, "define=s%");

with command line "--define foo=hello --define bar=world" will perform the
equivalent of the assignment

  $optctl{"define"} = {foo=>'hello', bar=>'world')

If no linkage is explicitly specified and no ref HASH is passed,
GetOptions will put the value in a global variable named after the
option, prefixed by "opt_". To yield a usable Perl variable,
characters that are not part of the syntax for variables are
translated to underscores. For example, "--fpp-struct-return" will set
the variable $opt_fpp_struct_return. Note that this variable resides
in the namespace of the calling program, not necessarily B<main>.
For example:

  GetOptions ("size=i", "sizes=i@");

with command line "-size 10 -sizes 24 -sizes 48" will perform the
equivalent of the assignments

  $opt_size = 10;
  @opt_sizes = (24, 48);

A lone dash B<-> is considered an option, the corresponding Perl
identifier is $opt_ .

The linkage specifier can be a reference to a scalar, a reference to
an array, a reference to a hash or a reference to a subroutine.

If a REF SCALAR is supplied, the new value is stored in the referenced
variable. If the option occurs more than once, the previous value is
overwritten. 

If a REF ARRAY is supplied, the new value is appended (pushed) to the
referenced array. 

If a REF HASH is supplied, the option value should look like "key" or
"key=value" (if the "=value" is omitted then a value of 1 is implied).
In this case, the element of the referenced hash with the key "key"
is assigned "value". 

If a REF CODE is supplied, the referenced subroutine is called with
two arguments: the option name and the option value.
The option name is always the true name, not an abbreviation or alias.

=head2 Aliases and abbreviations

The option name may actually be a list of option names, separated by
"|"s, e.g. "foo|bar|blech=s". In this example, "foo" is the true name
of this option. If no linkage is specified, options "foo", "bar" and
"blech" all will set $opt_foo.

Option names may be abbreviated to uniqueness, depending on
configuration option B<auto_abbrev>.

=head2 Non-option call-back routine

A special option specifier, E<lt>E<gt>, can be used to designate a subroutine
to handle non-option arguments. GetOptions will immediately call this
subroutine for every non-option it encounters in the options list.
This subroutine gets the name of the non-option passed.
This feature requires configuration option B<permute>, see section
CONFIGURATION OPTIONS.

See also the examples.

=head2 Option starters

On the command line, options can start with B<-> (traditional), B<-->
(POSIX) and B<+> (GNU, now being phased out). The latter is not
allowed if the environment variable B<POSIXLY_CORRECT> has been
defined.

Options that start with "--" may have an argument appended, separated
with an "=", e.g. "--foo=bar".

=head2 Return value

A return status of 0 (false) indicates that the function detected
one or more errors.

=head1 COMPATIBILITY

Getopt::Long::GetOptions() is the successor of
B<newgetopt.pl> that came with Perl 4. It is fully upward compatible.
In fact, the Perl 5 version of newgetopt.pl is just a wrapper around
the module.

If an "@" sign is appended to the argument specifier, the option is
treated as an array. Value(s) are not set, but pushed into array
@opt_name. If explicit linkage is supplied, this must be a reference
to an ARRAY.

If an "%" sign is appended to the argument specifier, the option is
treated as a hash. Value(s) of the form "name=value" are set by
setting the element of the hash %opt_name with key "name" to "value"
(if the "=value" portion is omitted it defaults to 1). If explicit
linkage is supplied, this must be a reference to a HASH.

If configuration option B<getopt_compat> is set (see section
CONFIGURATION OPTIONS), options that start with "+" or "-" may also
include their arguments, e.g. "+foo=bar". This is for compatiblity
with older implementations of the GNU "getopt" routine.

If the first argument to GetOptions is a string consisting of only
non-alphanumeric characters, it is taken to specify the option starter
characters. Everything starting with one of these characters from the
starter will be considered an option. B<Using a starter argument is
strongly deprecated.>

For convenience, option specifiers may have a leading B<-> or B<-->,
so it is possible to write:

   GetOptions qw(-foo=s --bar=i --ar=s);

=head1 EXAMPLES

If the option specifier is "one:i" (i.e. takes an optional integer
argument), then the following situations are handled:

   -one -two		-> $opt_one = '', -two is next option
   -one -2		-> $opt_one = -2

Also, assume specifiers "foo=s" and "bar:s" :

   -bar -xxx		-> $opt_bar = '', '-xxx' is next option
   -foo -bar		-> $opt_foo = '-bar'
   -foo --		-> $opt_foo = '--'

In GNU or POSIX format, option names and values can be combined:

   +foo=blech		-> $opt_foo = 'blech'
   --bar=		-> $opt_bar = ''
   --bar=--		-> $opt_bar = '--'

Example of using variable references:

   $ret = GetOptions ('foo=s', \$foo, 'bar=i', 'ar=s', \@ar);

With command line options "-foo blech -bar 24 -ar xx -ar yy" 
this will result in:

   $foo = 'blech'
   $opt_bar = 24
   @ar = ('xx','yy')

Example of using the E<lt>E<gt> option specifier:

   @ARGV = qw(-foo 1 bar -foo 2 blech);
   GetOptions("foo=i", \$myfoo, "<>", \&mysub);

Results:

   mysub("bar") will be called (with $myfoo being 1)
   mysub("blech") will be called (with $myfoo being 2)

Compare this with:

   @ARGV = qw(-foo 1 bar -foo 2 blech);
   GetOptions("foo=i", \$myfoo);

This will leave the non-options in @ARGV:

   $myfoo -> 2
   @ARGV -> qw(bar blech)

=head1 CONFIGURATION OPTIONS

B<GetOptions> can be configured by calling subroutine
B<Getopt::Long::config>. This subroutine takes a list of quoted
strings, each specifying a configuration option to be set, e.g.
B<ignore_case>. Options can be reset by prefixing with B<no_>, e.g.
B<no_ignore_case>. Case does not matter. Multiple calls to B<config>
are possible.

Previous versions of Getopt::Long used variables for the purpose of
configuring. Although manipulating these variables still work, it
is strongly encouraged to use the new B<config> routine. Besides, it
is much easier.

The following options are available:

=over 12

=item default

This option causes all configuration options to be reset to their
default values.

=item auto_abbrev

Allow option names to be abbreviated to uniqueness.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case B<auto_abbrev> is reset.

=item getopt_compat   

Allow '+' to start options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case B<getopt_compat> is reset.

=item require_order

Whether non-options are allowed to be mixed with
options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case b<require_order> is reset.

See also B<permute>, which is the opposite of B<require_order>.

=item permute

Whether non-options are allowed to be mixed with
options.
Default is set unless environment variable
POSIXLY_CORRECT has been set, in which case B<permute> is reset.
Note that B<permute> is the opposite of B<require_order>.

If B<permute> is set, this means that 

    -foo arg1 -bar arg2 arg3

is equivalent to

    -foo -bar arg1 arg2 arg3

If a non-option call-back routine is specified, @ARGV will always be
empty upon succesful return of GetOptions since all options have been
processed, except when B<--> is used:

    -foo arg1 -bar arg2 -- arg3

will call the call-back routine for arg1 and arg2, and terminate
leaving arg2 in @ARGV.

If B<require_order> is set, options processing
terminates when the first non-option is encountered.

    -foo arg1 -bar arg2 arg3

is equivalent to

    -foo -- arg1 -bar arg2 arg3

=item bundling (default: reset)

Setting this variable to a non-zero value will allow single-character
options to be bundled. To distinguish bundles from long option names,
long options must be introduced with B<--> and single-character
options (and bundles) with B<->. For example,

    ps -vax --vax

would be equivalent to

    ps -v -a -x --vax

provided "vax", "v", "a" and "x" have been defined to be valid
options. 

Bundled options can also include a value in the bundle; this value has
to be the last part of the bundle, e.g.

    scale -h24 -w80

is equivalent to

    scale -h 24 -w 80

Note: resetting B<bundling> also resets B<bundling_override>.

=item bundling_override (default: reset)

If B<bundling_override> is set, bundling is enabled as with
B<bundling> but now long option names override option bundles. In the
above example, B<-vax> would be interpreted as the option "vax", not
the bundle "v", "a", "x".

Note: resetting B<bundling_override> also resets B<bundling>.

B<Note:> Using option bundling can easily lead to unexpected results,
especially when mixing long options and bundles. Caveat emptor.

=item ignore_case  (default: set)

If set, case is ignored when matching options.

Note: resetting B<ignore_case> also resets B<ignore_case_always>.

=item ignore_case_always (default: reset)

When bundling is in effect, case is ignored on single-character
options also. 

Note: resetting B<ignore_case_always> also resets B<ignore_case>.

=item pass_through (default: reset)

Unknown options are passed through in @ARGV instead of being flagged
as errors. This makes it possible to write wrapper scripts that
process only part of the user supplied options, and passes the
remaining options to some other program.

This can be very confusing, especially when B<permute> is also set.

=item debug (default: reset)

Enable copious debugging output.

=back

=head1 OTHER USEFUL VARIABLES

=over 12

=item $Getopt::Long::VERSION

The version number of this Getopt::Long implementation in the format
C<major>.C<minor>. This can be used to have Exporter check the
version, e.g.

    use Getopt::Long 3.00;

You can inspect $Getopt::Long::major_version and
$Getopt::Long::minor_version for the individual components.

=item $Getopt::Long::error

Internal error flag. May be incremented from a call-back routine to
cause options parsing to fail.

=back

=cut

################ Copyright ################

# This program is Copyright 1990,1997 by Johan Vromans.
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# If you do not have a copy of the GNU General Public License write to
# the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, 
# MA 02139, USA.

################ Module Preamble ################

use strict;

BEGIN {
    require 5.003;
    use Exporter ();
    use vars   qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
    $VERSION   = sprintf("%d.%02d", q$Revision: 1.2 $ =~ /(\d+)\.(\d+)/);

    @ISA       = qw(Exporter);
    @EXPORT    = qw(&GetOptions $REQUIRE_ORDER $PERMUTE $RETURN_IN_ORDER);
    %EXPORT_TAGS = ();
    @EXPORT_OK = qw();
}

use vars @EXPORT, @EXPORT_OK;
# User visible variables.
use vars qw($error $debug $major_version $minor_version);
# Deprecated visible variables.
use vars qw($autoabbrev $getopt_compat $ignorecase $bundling $order
	    $passthrough);

################ Local Variables ################

my $gen_prefix;			# generic prefix (option starters)
my $argend;			# option list terminator
my %opctl;			# table of arg.specs (long and abbrevs)
my %bopctl;			# table of arg.specs (bundles)
my @opctl;			# the possible long option names
my $pkg;			# current context. Needed if no linkage.
my %aliases;			# alias table
my $genprefix;			# so we can call the same module more 
my $opt;			# current option
my $arg;			# current option value, if any
my $array;			# current option is array typed
my $hash;			# current option is hash typed
my $key;			# hash key for a hash option
				# than once in differing environments
my $config_defaults;		# set config defaults
my $find_option;		# helper routine

################ Subroutines ################

sub GetOptions {

    my @optionlist = @_;	# local copy of the option descriptions
    $argend = '--';		# option list terminator
    %opctl = ();		# table of arg.specs (long and abbrevs)
    %bopctl = ();		# table of arg.specs (bundles)
    $pkg = (caller)[0];		# current context
				# Needed if linkage is omitted.
    %aliases= ();		# alias table
    my @ret = ();		# accum for non-options
    my %linkage;		# linkage
    my $userlinkage;		# user supplied HASH
    $genprefix = $gen_prefix;	# so we can call the same module many times
    $error = 0;

    print STDERR ('GetOptions $Revision: 1.2 $ ',
		  "[GetOpt::Long $Getopt::Long::VERSION] -- ",
		  "called from package \"$pkg\".\n",
		  "  (@ARGV)\n",
		  "  autoabbrev=$autoabbrev".
		  ",bundling=$bundling",
		  ",getopt_compat=$getopt_compat",
		  ",order=$order",
		  ",\n  ignorecase=$ignorecase",
		  ",passthrough=$passthrough",
		  ",genprefix=\"$genprefix\"",
		  ".\n")
	if $debug;

    # Check for ref HASH as first argument. 
    # First argument may be an object. It's OK to use this as long
    # as it is really a hash underneath. 
    $userlinkage = undef;
    if ( ref($optionlist[0]) and
	 "$optionlist[0]" =~ /^(?:.*\=)?HASH\([^\(]*\)$/ ) {
	$userlinkage = shift (@optionlist);
	print STDERR ("=> user linkage: $userlinkage\n") if $debug;
    }

    # See if the first element of the optionlist contains option
    # starter characters.
    if ( $optionlist[0] =~ /^\W+$/ ) {
	$genprefix = shift (@optionlist);
	# Turn into regexp.
	$genprefix =~ s/(\W)/\\$1/g;
	$genprefix = "[" . $genprefix . "]";
    }

    # Verify correctness of optionlist.
    %opctl = ();
    %bopctl = ();
    while ( @optionlist > 0 ) {
	my $opt = shift (@optionlist);

	# Strip leading prefix so people can specify "--foo=i" if they like.
	$opt = $' if $opt =~ /^($genprefix)+/;

	if ( $opt eq '<>' ) {
	    if ( (defined $userlinkage)
		&& !(@optionlist > 0 && ref($optionlist[0]))
		&& (exists $userlinkage->{$opt})
		&& ref($userlinkage->{$opt}) ) {
		unshift (@optionlist, $userlinkage->{$opt});
	    }
	    unless ( @optionlist > 0 
		    && ref($optionlist[0]) && ref($optionlist[0]) eq 'CODE' ) {
		warn ("Option spec <> requires a reference to a subroutine\n");
		$error++;
		next;
	    }
	    $linkage{'<>'} = shift (@optionlist);
	    next;
	}

	if ( $opt !~ /^(\w+[-\w|]*)?(!|[=:][infse][@%]?)?$/ ) {
	    warn ("Error in option spec: \"", $opt, "\"\n");
	    $error++;
	    next;
	}
	my ($o, $c, $a) = ($1, $2);
	$c = '' unless defined $c;

	if ( ! defined $o ) {
	    # empty -> '-' option
	    $opctl{$o = ''} = $c;
	}
	else {
	    # Handle alias names
	    my @o =  split (/\|/, $o);
	    my $linko = $o = $o[0];
	    # Force an alias if the option name is not locase.
	    $a = $o unless $o eq lc($o);
	    $o = lc ($o)
		if $ignorecase > 1 
		    || ($ignorecase
			&& ($bundling ? length($o) > 1  : 1));

	    foreach ( @o ) {
		if ( $bundling && length($_) == 1 ) {
		    $_ = lc ($_) if $ignorecase > 1;
		    if ( $c eq '!' ) {
			$opctl{"no$_"} = $c;
			warn ("Ignoring '!' modifier for short option $_\n");
			$c = '';
		    }
		    $opctl{$_} = $bopctl{$_} = $c;
		}
		else {
		    $_ = lc ($_) if $ignorecase;
		    if ( $c eq '!' ) {
			$opctl{"no$_"} = $c;
			$c = '';
		    }
		    $opctl{$_} = $c;
		}
		if ( defined $a ) {
		    # Note alias.
		    $aliases{$_} = $a;
		}
		else {
		    # Set primary name.
		    $a = $_;
		}
	    }
	    $o = $linko;
	}

	# If no linkage is supplied in the @optionlist, copy it from
	# the userlinkage if available.
	if ( defined $userlinkage ) {
	    unless ( @optionlist > 0 && ref($optionlist[0]) ) {
		if ( exists $userlinkage->{$o} && ref($userlinkage->{$o}) ) {
		    print STDERR ("=> found userlinkage for \"$o\": ",
				  "$userlinkage->{$o}\n")
			if $debug;
		    unshift (@optionlist, $userlinkage->{$o});
		}
		else {
		    # Do nothing. Being undefined will be handled later.
		    next;
		}
	    }
	}

	# Copy the linkage. If omitted, link to global variable.
	if ( @optionlist > 0 && ref($optionlist[0]) ) {
	    print STDERR ("=> link \"$o\" to $optionlist[0]\n")
		if $debug;
	    if ( ref($optionlist[0]) =~ /^(SCALAR|CODE)$/ ) {
		$linkage{$o} = shift (@optionlist);
	    }
	    elsif ( ref($optionlist[0]) =~ /^(ARRAY)$/ ) {
		$linkage{$o} = shift (@optionlist);
		$opctl{$o} .= '@'
		  if $opctl{$o} ne '' and $opctl{$o} !~ /\@$/;
		$bopctl{$o} .= '@'
		  if $bundling and $bopctl{$o} ne '' and $bopctl{$o} !~ /\@$/;
	    }
	    elsif ( ref($optionlist[0]) =~ /^(HASH)$/ ) {
		$linkage{$o} = shift (@optionlist);
		$opctl{$o} .= '%'
		  if $opctl{$o} ne '' and $opctl{$o} !~ /\%$/;
		$bopctl{$o} .= '%'
		  if $bundling and $bopctl{$o} ne '' and $bopctl{$o} !~ /\%$/;
	    }
	    else {
		warn ("Invalid option linkage for \"", $opt, "\"\n");
		$error++;
	    }
	}
	else {
	    # Link to global $opt_XXX variable.
	    # Make sure a valid perl identifier results.
	    my $ov = $o;
	    $ov =~ s/\W/_/g;
	    if ( $c =~ /@/ ) {
		print STDERR ("=> link \"$o\" to \@$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\@".$pkg."::opt_$ov;");
	    }
	    elsif ( $c =~ /%/ ) {
		print STDERR ("=> link \"$o\" to \%$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\%".$pkg."::opt_$ov;");
	    }
	    else {
		print STDERR ("=> link \"$o\" to \$$pkg","::opt_$ov\n")
		    if $debug;
		eval ("\$linkage{\$o} = \\\$".$pkg."::opt_$ov;");
	    }
	}
    }

    # Bail out if errors found.
    return 0 if $error;

    # Sort the possible long option names.
    @opctl = sort(keys (%opctl)) if $autoabbrev;

    # Show the options tables if debugging.
    if ( $debug ) {
	my ($arrow, $k, $v);
	$arrow = "=> ";
	while ( ($k,$v) = each(%opctl) ) {
	    print STDERR ($arrow, "\$opctl{\"$k\"} = \"$v\"\n");
	    $arrow = "   ";
	}
	$arrow = "=> ";
	while ( ($k,$v) = each(%bopctl) ) {
	    print STDERR ($arrow, "\$bopctl{\"$k\"} = \"$v\"\n");
	    $arrow = "   ";
	}
    }

    # Process argument list
    while ( @ARGV > 0 ) {

	#### Get next argument ####

	$opt = shift (@ARGV);
	$arg = undef;
	$array = $hash = 0;
	print STDERR ("=> option \"", $opt, "\"\n") if $debug;

	#### Determine what we have ####

	# Double dash is option list terminator.
	if ( $opt eq $argend ) {
	    # Finish. Push back accumulated arguments and return.
	    unshift (@ARGV, @ret) 
		if $order == $PERMUTE;
	    return ($error == 0);
	}

	my $tryopt = $opt;

	# find_option operates on the GLOBAL $opt and $arg!
	if ( &$find_option () ) {
	    
	    # find_option undefines $opt in case of errors.
	    next unless defined $opt;

	    if ( defined $arg ) {
		$opt = $aliases{$opt} if defined $aliases{$opt};

		if ( defined $linkage{$opt} ) {
		    print STDERR ("=> ref(\$L{$opt}) -> ",
				  ref($linkage{$opt}), "\n") if $debug;

		    if ( ref($linkage{$opt}) eq 'SCALAR' ) {
			print STDERR ("=> \$\$L{$opt} = \"$arg\"\n") if $debug;
			${$linkage{$opt}} = $arg;
		    }
		    elsif ( ref($linkage{$opt}) eq 'ARRAY' ) {
			print STDERR ("=> push(\@{\$L{$opt}, \"$arg\")\n")
			    if $debug;
			push (@{$linkage{$opt}}, $arg);
		    }
		    elsif ( ref($linkage{$opt}) eq 'HASH' ) {
			print STDERR ("=> \$\$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$linkage{$opt}->{$key} = $arg;
		    }
		    elsif ( ref($linkage{$opt}) eq 'CODE' ) {
			print STDERR ("=> &L{$opt}(\"$opt\", \"$arg\")\n")
			    if $debug;
			&{$linkage{$opt}}($opt, $arg);
		    }
		    else {
			print STDERR ("Invalid REF type \"", ref($linkage{$opt}),
				      "\" in linkage\n");
			die ("Getopt::Long -- internal error!\n");
		    }
		}
		# No entry in linkage means entry in userlinkage.
		elsif ( $array ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> push(\@{\$L{$opt}}, \"$arg\")\n")
			    if $debug;
			push (@{$userlinkage->{$opt}}, $arg);
		    }
		    else {
			print STDERR ("=>\$L{$opt} = [\"$arg\"]\n")
			    if $debug;
			$userlinkage->{$opt} = [$arg];
		    }
		}
		elsif ( $hash ) {
		    if ( defined $userlinkage->{$opt} ) {
			print STDERR ("=> \$L{$opt}->{$key} = \"$arg\"\n")
			    if $debug;
			$userlinkage->{$opt}->{$key} = $arg;
		    }
		    else {
			print STDERR ("=>\$L{$opt} = {$key => \"$arg\"}\n")
			    if $debug;
			$userlinkage->{$opt} = {$key => $arg};
		    }
		}
		else {
		    print STDERR ("=>\$L{$opt} = \"$arg\"\n") if $debug;
		    $userlinkage->{$opt} = $arg;
		}
	    }
	}

	# Not an option. Save it if we $PERMUTE and don't have a <>.
	elsif ( $order == $PERMUTE ) {
	    # Try non-options call-back.
	    my $cb;
	    if ( (defined ($cb = $linkage{'<>'})) ) {
		&$cb($tryopt);
	    }
	    else {
		print STDERR ("=> saving \"$tryopt\" ",
			      "(not an option, may permute)\n") if $debug;
		push (@ret, $tryopt);
	    }
	    next;
	}

	# ...otherwise, terminate.
	else {
	    # Push this one back and exit.
	    unshift (@ARGV, $tryopt);
	    return ($error == 0);
	}

    }

    # Finish.
    if ( $order == $PERMUTE ) {
	#  Push back accumulated arguments
	print STDERR ("=> restoring \"", join('" "', @ret), "\"\n")
	    if $debug && @ret > 0;
	unshift (@ARGV, @ret) if @ret > 0;
    }

    return ($error == 0);
}

sub config (@) {
    my (@options) = @_;
    my $opt;
    foreach $opt ( @options ) {
	my $try = lc ($opt);
	my $action = 1;
	if ( $try =~ /^no_?/ ) {
	    $action = 0;
	    $try = $';
	}
	if ( $try eq 'default' or $try eq 'defaults' ) {
	    &$config_defaults () if $action;
	}
	elsif ( $try eq 'auto_abbrev' or $try eq 'autoabbrev' ) {
	    $autoabbrev = $action;
	}
	elsif ( $try eq 'getopt_compat' ) {
	    $getopt_compat = $action;
	}
	elsif ( $try eq 'ignorecase' or $try eq 'ignore_case' ) {
	    $ignorecase = $action;
	}
	elsif ( $try eq 'ignore_case_always' ) {
	    $ignorecase = $action ? 2 : 0;
	}
	elsif ( $try eq 'bundling' ) {
	    $bundling = $action;
	}
	elsif ( $try eq 'bundling_override' ) {
	    $bundling = $action ? 2 : 0;
	}
	elsif ( $try eq 'require_order' ) {
	    $order = $action ? $REQUIRE_ORDER : $PERMUTE;
	}
	elsif ( $try eq 'permute' ) {
	    $order = $action ? $PERMUTE : $REQUIRE_ORDER;
	}
	elsif ( $try eq 'pass_through' or $try eq 'passthrough' ) {
	    $passthrough = $action;
	}
	elsif ( $try eq 'debug' ) {
	    $debug = $action;
	}
	else {
	    $Carp::CarpLevel = 1;
	    Carp::croak("Getopt::Long: unknown config parameter \"$opt\"")
	}
    }
}

# Modified from Exporter. This one handles 2.001 and 2.01 etc just like 2.1.
sub require_version {
    no strict;
    my ($self, $wanted) = @_;
    my $pkg = ref $self || $self;
    my $version = $ {"${pkg}::VERSION"} || "(undef)";

    $wanted .= '.0' unless $wanted =~ /\./;
    $wanted = $1 * 1000 + $2 if $wanted =~ /^(\d+)\.(\d+)$/;
    $version = $1 * 1000 + $2 if $version =~ /^(\d+)\.(\d+)$/;
    if ( $version < $wanted ) {
	$version =~ s/^(\d+)(\d\d\d)$/$1.'.'.(0+$2)/e;
	$wanted =~ s/^(\d+)(\d\d\d)$/$1.'.'.(0+$2)/e;
	$Carp::CarpLevel = 1;
	Carp::croak("$pkg $wanted required--this is only version $version")
    }
    $version;
}

################ Private Subroutines ################

$find_option = sub {

    return 0 unless $opt =~ /^$genprefix/;

    $opt = $';
    my ($starter) = $&;

    my $optarg = undef;	# value supplied with --opt=value
    my $rest = undef;	# remainder from unbundling

    # If it is a long option, it may include the value.
    if (($starter eq "--" || $getopt_compat)
	&& $opt =~ /^([^=]+)=/ ) {
	$opt = $1;
	$optarg = $';
	print STDERR ("=> option \"", $opt, 
		      "\", optarg = \"$optarg\"\n") if $debug;
    }

    #### Look it up ###

    my $tryopt = $opt;		# option to try
    my $optbl = \%opctl;	# table to look it up (long names)
    my $type;

    if ( $bundling && $starter eq '-' ) {
	# Unbundle single letter option.
	$rest = substr ($tryopt, 1);
	$tryopt = substr ($tryopt, 0, 1);
	$tryopt = lc ($tryopt) if $ignorecase > 1;
	print STDERR ("=> $starter$tryopt unbundled from ",
		      "$starter$tryopt$rest\n") if $debug;
	$rest = undef unless $rest ne '';
	$optbl = \%bopctl;	# look it up in the short names table

	# If bundling == 2, long options can override bundles.
	if ( $bundling == 2 and
	     defined ($type = $opctl{$tryopt.$rest}) ) {
	    print STDERR ("=> $starter$tryopt rebundled to ",
			  "$starter$tryopt$rest\n") if $debug;
	    $tryopt .= $rest;
	    undef $rest;
	}
    } 

    # Try auto-abbreviation.
    elsif ( $autoabbrev ) {
	# Downcase if allowed.
	$tryopt = $opt = lc ($opt) if $ignorecase;
	# Turn option name into pattern.
	my $pat = quotemeta ($opt);
	# Look up in option names.
	my @hits = grep (/^$pat/, @opctl);
	print STDERR ("=> ", scalar(@hits), " hits (@hits) with \"$pat\" ",
		      "out of ", scalar(@opctl), "\n") if $debug;

	# Check for ambiguous results.
	unless ( (@hits <= 1) || (grep ($_ eq $opt, @hits) == 1) ) {
	    # See if all matches are for the same option.
	    my %hit;
	    foreach ( @hits ) {
		$_ = $aliases{$_} if defined $aliases{$_};
		$hit{$_} = 1;
	    }
	    # Now see if it really is ambiguous.
	    unless ( keys(%hit) == 1 ) {
		return 0 if $passthrough;
		print STDERR ("Option ", $opt, " is ambiguous (",
			      join(", ", @hits), ")\n");
		$error++;
		undef $opt;
		return 1;
	    }
	    @hits = keys(%hit);
	}

	# Complete the option name, if appropriate.
	if ( @hits == 1 && $hits[0] ne $opt ) {
	    $tryopt = $hits[0];
	    $tryopt = lc ($tryopt) if $ignorecase;
	    print STDERR ("=> option \"$opt\" -> \"$tryopt\"\n")
		if $debug;
	}
    }

    # Map to all lowercase if ignoring case.
    elsif ( $ignorecase ) {
	$tryopt = lc ($opt);
    }

    # Check validity by fetching the info.
    $type = $optbl->{$tryopt} unless defined $type;
    unless  ( defined $type ) {
	return 0 if $passthrough;
	warn ("Unknown option: ", $opt, "\n");
	$error++;
	return 1;
    }
    # Apparently valid.
    $opt = $tryopt;
    print STDERR ("=> found \"$type\" for ", $opt, "\n") if $debug;

    #### Determine argument status ####

    # If it is an option w/o argument, we're almost finished with it.
    if ( $type eq '' || $type eq '!' ) {
	if ( defined $optarg ) {
	    return 0 if $passthrough;
	    print STDERR ("Option ", $opt, " does not take an argument\n");
	    $error++;
	    undef $opt;
	}
	elsif ( $type eq '' ) {
	    $arg = 1;		# supply explicit value
	}
	else {
	    substr ($opt, 0, 2) = ''; # strip NO prefix
	    $arg = 0;		# supply explicit value
	}
	unshift (@ARGV, $starter.$rest) if defined $rest;
	return 1;
    }

    # Get mandatory status and type info.
    my $mand;
    ($mand, $type, $array, $hash) = $type =~ /^(.)(.)(@?)(%?)$/;

    # Check if there is an option argument available.
    if ( defined $optarg ? ($optarg eq '') 
	 : !(defined $rest || @ARGV > 0) ) {
	# Complain if this option needs an argument.
	if ( $mand eq "=" ) {
	    return 0 if $passthrough;
	    print STDERR ("Option ", $opt, " requires an argument\n");
	    $error++;
	    undef $opt;
	}
	if ( $mand eq ":" ) {
	    $arg = $type eq "s" ? '' : 0;
	}
	return 1;
    }

    # Get (possibly optional) argument.
    $arg = (defined $rest ? $rest
	    : (defined $optarg ? $optarg : shift (@ARGV)));

    # Get key if this is a "name=value" pair for a hash option.
    $key = undef;
    if ($hash && defined $arg) {
	($key, $arg) = ($arg =~ /=/o) ? ($`, $') : ($arg, 1);
    }

    #### Check if the argument is valid for this option ####

    if ( $type eq "s" ) {	# string
	# A mandatory string takes anything. 
	return 1 if $mand eq "=";

	# An optional string takes almost anything. 
	return 1 if defined $optarg || defined $rest;
	return 1 if $arg eq "-"; # ??

	# Check for option or option list terminator.
	if ($arg eq $argend ||
	    $arg =~ /^$genprefix.+/) {
	    # Push back.
	    unshift (@ARGV, $arg);
	    # Supply empty value.
	    $arg = '';
	}
    }

    elsif ( $type eq "n" || $type eq "i" ) { # numeric/integer
	if ( $arg !~ /^-?[0-9]+$/ ) {
	    if ( defined $optarg || $mand eq "=" ) {
		if ( $passthrough ) {
		    unshift (@ARGV, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return 0;
		}
		print STDERR ("Value \"", $arg, "\" invalid for option ",
			      $opt, " (number expected)\n");
		$error++;
		undef $opt;
		# Push back.
		unshift (@ARGV, $starter.$rest) if defined $rest;
	    }
	    else {
		# Push back.
		unshift (@ARGV, defined $rest ? $starter.$rest : $arg);
		# Supply default value.
		$arg = 0;
	    }
	}
    }

    elsif ( $type eq "f" ) { # real number, int is also ok
	if ( $arg !~ /^-?[0-9.]+([eE]-?[0-9]+)?$/ ) {
	    if ( defined $optarg || $mand eq "=" ) {
		if ( $passthrough ) {
		    unshift (@ARGV, defined $rest ? $starter.$rest : $arg)
		      unless defined $optarg;
		    return 0;
		}
		print STDERR ("Value \"", $arg, "\" invalid for option ",
			      $opt, " (real number expected)\n");
		$error++;
		undef $opt;
		# Push back.
		unshift (@ARGV, $starter.$rest) if defined $rest;
	    }
	    else {
		# Push back.
		unshift (@ARGV, defined $rest ? $starter.$rest : $arg);
		# Supply default value.
		$arg = 0.0;
	    }
	}
    }
    else {
	die ("GetOpt::Long internal error (Can't happen)\n");
    }
    return 1;
};

$config_defaults = sub {
    # Handle POSIX compliancy.
    if ( defined $ENV{"POSIXLY_CORRECT"} ) {
	$gen_prefix = "(--|-)";
	$autoabbrev = 0;		# no automatic abbrev of options
	$bundling = 0;			# no bundling of single letter switches
	$getopt_compat = 0;		# disallow '+' to start options
	$order = $REQUIRE_ORDER;
    }
    else {
	$gen_prefix = "(--|-|\\+)";
	$autoabbrev = 1;		# automatic abbrev of options
	$bundling = 0;			# bundling off by default
	$getopt_compat = 1;		# allow '+' to start options
	$order = $PERMUTE;
    }
    # Other configurable settings.
    $debug = 0;			# for debugging
    $error = 0;			# error tally
    $ignorecase = 1;		# ignore case when matching options
    $passthrough = 0;		# leave unrecognized options alone
};

################ Initialization ################

# Values for $order. See GNU getopt.c for details.
($REQUIRE_ORDER, $PERMUTE, $RETURN_IN_ORDER) = (0..2);
# Version major/minor numbers.
($major_version, $minor_version) = $VERSION =~ /^(\d+)\.(\d+)/;

# Set defaults.
&$config_defaults ();

################ Package return ################

1;
