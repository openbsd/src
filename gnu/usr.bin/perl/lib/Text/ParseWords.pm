package Text::ParseWords;

require 5.000;
require Exporter;
require AutoLoader;
use Carp;

@ISA = qw(Exporter AutoLoader);
@EXPORT = qw(shellwords quotewords);
@EXPORT_OK = qw(old_shellwords);

=head1 NAME

Text::ParseWords - parse text into an array of tokens

=head1 SYNOPSIS

  use Text::ParseWords;
  @words = &quotewords($delim, $keep, @lines);
  @words = &shellwords(@lines);
  @words = &old_shellwords(@lines);

=head1 DESCRIPTION

&quotewords() accepts a delimiter (which can be a regular expression)
and a list of lines and then breaks those lines up into a list of
words ignoring delimiters that appear inside quotes.

The $keep argument is a boolean flag.  If true, the quotes are kept
with each word, otherwise quotes are stripped in the splitting process.
$keep also defines whether unprotected backslashes are retained.

A &shellwords() replacement is included to demonstrate the new package.
This version differs from the original in that it will _NOT_ default
to using $_ if no arguments are given.  I personally find the old behavior
to be a mis-feature.


&quotewords() works by simply jamming all of @lines into a single
string in $_ and then pulling off words a bit at a time until $_
is exhausted.

=head1 AUTHORS

Hal Pomeranz (pomeranz@netcom.com), 23 March 1994

Basically an update and generalization of the old shellwords.pl.
Much code shamelessly stolen from the old version (author unknown).

=cut

1;
__END__

sub shellwords {
    local(@lines) = @_;
    $lines[$#lines] =~ s/\s+$//;
    &quotewords('\s+', 0, @lines);
}



sub quotewords {

# The inner "for" loop builds up each word (or $field) one $snippet
# at a time.  A $snippet is a quoted string, a backslashed character,
# or an unquoted string.  We fall out of the "for" loop when we reach
# the end of $_ or when we hit a delimiter.  Falling out of the "for"
# loop, we push the $field we've been building up onto the list of
# @words we'll be returning, and then loop back and pull another word
# off of $_.
#
# The first two cases inside the "for" loop deal with quoted strings.
# The first case matches a double quoted string, removes it from $_,
# and assigns the double quoted string to $snippet in the body of the
# conditional.  The second case handles single quoted strings.  In
# the third case we've found a quote at the current beginning of $_,
# but it didn't match the quoted string regexps in the first two cases,
# so it must be an unbalanced quote and we croak with an error (which can
# be caught by eval()).
#
# The next case handles backslashed characters, and the next case is the
# exit case on reaching the end of the string or finding a delimiter.
#
# Otherwise, we've found an unquoted thing and we pull of characters one
# at a time until we reach something that could start another $snippet--
# a quote of some sort, a backslash, or the delimiter.  This one character
# at a time behavior was necessary if the delimiter was going to be a
# regexp (love to hear it if you can figure out a better way).

    local($delim, $keep, @lines) = @_;
    local(@words,$snippet,$field,$_);

    $_ = join('', @lines);
    while ($_) {
	$field = '';
	for (;;) {
            $snippet = '';
	    if (s/^"(([^"\\]|\\[\\"])*)"//) {
		$snippet = $1;
                $snippet = "\"$snippet\"" if ($keep);
	    }
	    elsif (s/^'(([^'\\]|\\[\\'])*)'//) {
		$snippet = $1;
                $snippet = "'$snippet'" if ($keep);
	    }
	    elsif (/^["']/) {
		croak "Unmatched quote";
	    }
            elsif (s/^\\(.)//) {
                $snippet = $1;
                $snippet = "\\$snippet" if ($keep);
            }
	    elsif (!$_ || s/^$delim//) {
               last;
	    }
	    else {
                while ($_ && !(/^$delim/ || /^['"\\]/)) {
		   $snippet .=  substr($_, 0, 1);
                   substr($_, 0, 1) = '';
                }
	    }
	    $field .= $snippet;
	}
	push(@words, $field);
    }
    @words;
}


sub old_shellwords {

    # Usage:
    #	use ParseWords;
    #	@words = old_shellwords($line);
    #	or
    #	@words = old_shellwords(@lines);

    local($_) = join('', @_);
    my(@words,$snippet,$field);

    s/^\s+//;
    while ($_ ne '') {
	$field = '';
	for (;;) {
	    if (s/^"(([^"\\]|\\.)*)"//) {
		($snippet = $1) =~ s#\\(.)#$1#g;
	    }
	    elsif (/^"/) {
		croak "Unmatched double quote: $_";
	    }
	    elsif (s/^'(([^'\\]|\\.)*)'//) {
		($snippet = $1) =~ s#\\(.)#$1#g;
	    }
	    elsif (/^'/) {
		croak "Unmatched single quote: $_";
	    }
	    elsif (s/^\\(.)//) {
		$snippet = $1;
	    }
	    elsif (s/^([^\s\\'"]+)//) {
		$snippet = $1;
	    }
	    else {
		s/^\s+//;
		last;
	    }
	    $field .= $snippet;
	}
	push(@words, $field);
    }
    @words;
}
