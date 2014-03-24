#!/usr/bin/perl -w
# 
# Unconditionally regenerate:
#
#    pod/perlintern.pod
#    pod/perlapi.pod
#
# from information stored in
#
#    embed.fnc
#    plus all the .c and .h files listed in MANIFEST
#
# Has an optional arg, which is the directory to chdir to before reading
# MANIFEST and *.[ch].
#
# This script is normally invoked as part of 'make all', but is also
# called from from regen.pl.
#
# '=head1' are the only headings looked for.  If the next line after the
# heading begins with a word character, it is considered to be the first line
# of documentation that applies to the heading itself.  That is, it is output
# immediately after the heading, before the first function, and not indented.
# The next input line that is a pod directive terminates this heading-level
# documentation.

use strict;

#
# See database of global and static function prototypes in embed.fnc
# This is used to generate prototype headers under various configurations,
# export symbols lists for different platforms, and macros to provide an
# implicit interpreter context argument.
#

my %docs;
my %funcflags;
my %macro = (
	     ax => 1,
	     items => 1,
	     ix => 1,
	     svtype => 1,
	    );
my %missing;

my $curheader = "Unknown section";

sub autodoc ($$) { # parse a file and extract documentation info
    my($fh,$file) = @_;
    my($in, $doc, $line, $header_doc);
FUNC:
    while (defined($in = <$fh>)) {
	if ($in =~ /^#\s*define\s+([A-Za-z_][A-Za-z_0-9]+)\(/ &&
	    ($file ne 'embed.h' || $file ne 'proto.h')) {
	    $macro{$1} = $file;
	    next FUNC;
	}
        if ($in=~ /^=head1 (.*)/) {
            $curheader = $1;

            # If the next line begins with a word char, then is the start of
            # heading-level documentation.
	    if (defined($doc = <$fh>)) {
                if ($doc !~ /^\w/) {
                    $in = $doc;
                    redo FUNC;
                }
                $header_doc = $doc;
                $line++;

                # Continue getting the heading-level documentation until read
                # in any pod directive (or as a fail-safe, find a closing
                # comment to this pod in a C language file
HDR_DOC:
                while (defined($doc = <$fh>)) {
                    if ($doc =~ /^=\w/) {
                        $in = $doc;
                        redo FUNC;
                    }
                    $line++;

                    if ($doc =~ m:^\s*\*/$:) {
                        warn "=cut missing? $file:$line:$doc";;
                        last HDR_DOC;
                    }
                    $header_doc .= $doc;
                }
            }
            next FUNC;
        }
	$line++;
	if ($in =~ /^=for\s+apidoc\s+(.*?)\s*\n/) {
	    my $proto = $1;
	    $proto = "||$proto" unless $proto =~ /\|/;
	    my($flags, $ret, $name, @args) = split /\|/, $proto;
	    my $docs = "";
DOC:
	    while (defined($doc = <$fh>)) {
		$line++;
		last DOC if $doc =~ /^=\w+/;
		if ($doc =~ m:^\*/$:) {
		    warn "=cut missing? $file:$line:$doc";;
		    last DOC;
		}
		$docs .= $doc;
	    }
	    $docs = "\n$docs" if $docs and $docs !~ /^\n/;

	    # Check the consistency of the flags
	    my ($embed_where, $inline_where);
	    my ($embed_may_change, $inline_may_change);

	    my $docref = delete $funcflags{$name};
	    if ($docref and %$docref) {
		$embed_where = $docref->{flags} =~ /A/ ? 'api' : 'guts';
		$embed_may_change = $docref->{flags} =~ /M/;
	    } else {
		$missing{$name} = $file;
	    }
	    if ($flags =~ /m/) {
		$inline_where = $flags =~ /A/ ? 'api' : 'guts';
		$inline_may_change = $flags =~ /x/;

		if (defined $embed_where && $inline_where ne $embed_where) {
		    warn "Function '$name' inconsistency: embed.fnc says $embed_where, Pod says $inline_where";
		}

		if (defined $embed_may_change
		    && $inline_may_change ne $embed_may_change) {
		    my $message = "Function '$name' inconsistency: ";
		    if ($embed_may_change) {
			$message .= "embed.fnc says 'may change', Pod does not";
		    } else {
			$message .= "Pod says 'may change', embed.fnc does not";
		    }
		    warn $message;
		}
	    } elsif (!defined $embed_where) {
		warn "Unable to place $name!\n";
		next;
	    } else {
		$inline_where = $embed_where;
		$flags .= 'x' if $embed_may_change;
		@args = @{$docref->{args}};
		$ret = $docref->{retval};
	    }

	    $docs{$inline_where}{$curheader}{$name}
		= [$flags, $docs, $ret, $file, @args];

            # Create a special entry with an empty-string name for the
            # heading-level documentation.
	    if (defined $header_doc) {
                $docs{$inline_where}{$curheader}{""} = $header_doc;
                undef $header_doc;
            }

	    if (defined $doc) {
		if ($doc =~ /^=(?:for|head)/) {
		    $in = $doc;
		    redo FUNC;
		}
	    } else {
		warn "$file:$line:$in";
	    }
	}
    }
}

sub docout ($$$) { # output the docs for one function
    my($fh, $name, $docref) = @_;
    my($flags, $docs, $ret, $file, @args) = @$docref;
    $name =~ s/\s*$//;

    $docs .= "NOTE: this function is experimental and may change or be
removed without notice.\n\n" if $flags =~ /x/;
    $docs .= "NOTE: the perl_ form of this function is deprecated.\n\n"
	if $flags =~ /p/;
    $docs .= "NOTE: this function must be explicitly called as Perl_$name with an aTHX_ parameter.\n\n"
        if $flags =~ /o/;

    print $fh "=item $name\nX<$name>\n$docs";

    if ($flags =~ /U/) { # no usage
	# nothing
    } elsif ($flags =~ /s/) { # semicolon ("dTHR;")
	print $fh "\t\t$name;\n\n";
    } elsif ($flags =~ /n/) { # no args
	print $fh "\t$ret\t$name\n\n";
    } else { # full usage
	my $p            = $flags =~ /o/; # no #define foo Perl_foo
	my $n            = "Perl_"x$p . $name;
	my $large_ret    = length $ret > 7;
	my $indent_size  = 7+8 # nroff: 7 under =head + 8 under =item
	                  +8+($large_ret ? 1 + length $ret : 8)
	                  +length($n) + 1;
	my $indent;
	print $fh "\t$ret" . ($large_ret ? ' ' : "\t") . "$n(";
	my $long_args;
	for (@args) {
	    if ($indent_size + 2 + length > 79) {
		$long_args=1;
		$indent_size -= length($n) - 3;
		last;
	    }
	}
	my $args = '';
	if ($p) {
	    $args = @args ? "pTHX_ " : "pTHX";
	    if ($long_args) { print $fh $args; $args = '' }
	}
	$long_args and print $fh "\n";
	my $first = !$long_args;
	while () {
	    if (!@args or
	         length $args
	         && $indent_size + 3 + length($args[0]) + length $args > 79
	    ) {
		print $fh
		  $first ? '' : (
		    $indent //=
		       "\t".($large_ret ? " " x (1+length $ret) : "\t")
		      ." "x($long_args ? 4 : 1 + length $n)
		  ),
		  $args, (","x($args ne 'pTHX_ ') . "\n")x!!@args;
		$args = $first = '';
	    }
	    @args or last;
	    $args .= ", "x!!(length $args && $args ne 'pTHX_ ')
	           . shift @args;
	}
	if ($long_args) { print $fh "\n", substr $indent, 0, -4 }
	print $fh ")\n\n";
    }
    print $fh "=for hackers\nFound in file $file\n\n";
}

sub output {
    my ($podname, $header, $dochash, $missing, $footer) = @_;
    my $filename = "pod/$podname.pod";
    open my $fh, '>', $filename or die "Can't open $filename: $!";

    print $fh <<"_EOH_", $header;
-*- buffer-read-only: t -*-

!!!!!!!   DO NOT EDIT THIS FILE   !!!!!!!
This file is built by $0 extracting documentation from the C source
files.

_EOH_

    my $key;
    # case insensitive sort, with fallback for determinacy
    for $key (sort { uc($a) cmp uc($b) || $a cmp $b } keys %$dochash) {
	my $section = $dochash->{$key}; 
	print $fh "\n=head1 $key\n\n";

        # Output any heading-level documentation and delete so won't get in
        # the way later
        if (exists $section->{""}) {
            print $fh $section->{""} . "\n";
            delete $section->{""};
        }
	print $fh "=over 8\n\n";

	# Again, fallback for determinacy
	for my $key (sort { uc($a) cmp uc($b) || $a cmp $b } keys %$section) {
	    docout($fh, $key, $section->{$key});
	}
	print $fh "\n=back\n";
    }

    if (@$missing) {
        print $fh "\n=head1 Undocumented functions\n\n";
    print $fh $podname eq 'perlapi' ? <<'_EOB_' : <<'_EOB_';
The following functions have been flagged as part of the public API,
but are currently undocumented. Use them at your own risk, as the
interfaces are subject to change.  Functions that are not listed in this
document are not intended for public use, and should NOT be used under any
circumstances.

If you use one of the undocumented functions below, you may wish to consider
creating and submitting documentation for it. If your patch is accepted, this
will indicate that the interface is stable (unless it is explicitly marked
otherwise).

=over

_EOB_
The following functions are currently undocumented.  If you use one of
them, you may wish to consider creating and submitting documentation for
it.

=over

_EOB_
    for my $missing (sort @$missing) {
        print $fh "=item $missing\nX<$missing>\n\n";
    }
    print $fh "=back\n\n";
}

print $fh $footer, <<'_EOF_';
=cut

 ex: set ro:
_EOF_

    close $fh or die "Can't close $filename: $!";
}

if (@ARGV) {
    my $workdir = shift;
    chdir $workdir
        or die "Couldn't chdir to '$workdir': $!";
}

open IN, "embed.fnc" or die $!;

while (<IN>) {
    chomp;
    next if /^:/;
    while (s|\\\s*$||) {
	$_ .= <IN>;
	chomp;
    }
    s/\s+$//;
    next if /^\s*(#|$)/;

    my ($flags, $retval, $func, @args) = split /\s*\|\s*/, $_;

    next unless $func;

    s/\b(NN|NULLOK)\b\s+//g for @args;
    $func =~ s/\t//g; # clean up fields from embed.pl
    $retval =~ s/\t//;

    $funcflags{$func} = {
			 flags => $flags,
			 retval => $retval,
			 args => \@args,
			};
}

my $file;
# glob() picks up docs from extra .c or .h files that may be in unclean
# development trees.
my $MANIFEST = do {
  local ($/, *FH);
  open FH, "MANIFEST" or die "Can't open MANIFEST: $!";
  <FH>;
};

for $file (($MANIFEST =~ /^(\S+\.c)\t/gm), ($MANIFEST =~ /^(\S+\.h)\t/gm)) {
    open F, "< $file" or die "Cannot open $file for docs: $!\n";
    $curheader = "Functions in file $file\n";
    autodoc(\*F,$file);
    close F or die "Error closing $file: $!\n";
}

for (sort keys %funcflags) {
    next unless $funcflags{$_}{flags} =~ /d/;
    warn "no docs for $_\n"
}

foreach (sort keys %missing) {
    next if $macro{$_};
    # Heuristics for known not-a-function macros:
    next if /^[A-Z]/;
    next if /^dj?[A-Z]/;

    warn "Function '$_', documented in $missing{$_}, not listed in embed.fnc";
}

# walk table providing an array of components in each line to
# subroutine, printing the result

# List of funcs in the public API that aren't also marked as experimental.
my @missing_api = grep $funcflags{$_}{flags} =~ /A/ && $funcflags{$_}{flags} !~ /M/ && !$docs{api}{$_}, keys %funcflags;
output('perlapi', <<'_EOB_', $docs{api}, \@missing_api, <<'_EOE_');
=head1 NAME

perlapi - autogenerated documentation for the perl public API

=head1 DESCRIPTION
X<Perl API> X<API> X<api>

This file contains the documentation of the perl public API generated by
F<embed.pl>, specifically a listing of functions, macros, flags, and variables
that may be used by extension writers.  L<At the end|/Undocumented functions>
is a list of functions which have yet to be documented.  The interfaces of
those are subject to change without notice.  Any functions not listed here are
not part of the public API, and should not be used by extension writers at
all.  For these reasons, blindly using functions listed in proto.h is to be
avoided when writing extensions.

Note that all Perl API global variables must be referenced with the C<PL_>
prefix.  Some macros are provided for compatibility with the older,
unadorned names, but this support may be disabled in a future release.

Perl was originally written to handle US-ASCII only (that is characters
whose ordinal numbers are in the range 0 - 127).
And documentation and comments may still use the term ASCII, when
sometimes in fact the entire range from 0 - 255 is meant.

Note that Perl can be compiled and run under EBCDIC (See L<perlebcdic>)
or ASCII.  Most of the documentation (and even comments in the code)
ignore the EBCDIC possibility.  
For almost all purposes the differences are transparent.
As an example, under EBCDIC,
instead of UTF-8, UTF-EBCDIC is used to encode Unicode strings, and so
whenever this documentation refers to C<utf8>
(and variants of that name, including in function names),
it also (essentially transparently) means C<UTF-EBCDIC>.
But the ordinals of characters differ between ASCII, EBCDIC, and
the UTF- encodings, and a string encoded in UTF-EBCDIC may occupy more bytes
than in UTF-8.

The listing below is alphabetical, case insensitive.

_EOB_

=head1 AUTHORS

Until May 1997, this document was maintained by Jeff Okamoto
<okamoto@corp.hp.com>.  It is now maintained as part of Perl itself.

With lots of help and suggestions from Dean Roehrich, Malcolm Beattie,
Andreas Koenig, Paul Hudson, Ilya Zakharevich, Paul Marquess, Neil
Bowers, Matthew Green, Tim Bunce, Spider Boardman, Ulrich Pfeifer,
Stephen McCamant, and Gurusamy Sarathy.

API Listing originally by Dean Roehrich <roehrich@cray.com>.

Updated to be autogenerated from comments in the source by Benjamin Stuhl.

=head1 SEE ALSO

L<perlguts>, L<perlxs>, L<perlxstut>, L<perlintern>

_EOE_

# List of non-static internal functions
my @missing_guts =
 grep $funcflags{$_}{flags} !~ /[As]/ && !$docs{guts}{$_}, keys %funcflags;

output('perlintern', <<'END', $docs{guts}, \@missing_guts, <<'END');
=head1 NAME

perlintern - autogenerated documentation of purely B<internal>
		 Perl functions

=head1 DESCRIPTION
X<internal Perl functions> X<interpreter functions>

This file is the autogenerated documentation of functions in the
Perl interpreter that are documented using Perl's internal documentation
format but are not marked as part of the Perl API. In other words,
B<they are not for use in extensions>!

END

=head1 AUTHORS

The autodocumentation system was originally added to the Perl core by
Benjamin Stuhl. Documentation is by whoever was kind enough to
document their functions.

=head1 SEE ALSO

L<perlguts>, L<perlapi>

END
