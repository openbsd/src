package encoding;
our $VERSION = do { my @r = (q$Revision: 1.35 $ =~ /\d+/g); sprintf "%d."."%02d" x $#r, @r };

use Encode;
use strict;

BEGIN {
    if (ord("A") == 193) {
	require Carp;
	Carp::croak("encoding pragma does not support EBCDIC platforms");
    }
}

our $HAS_PERLIO = 0;
eval { require PerlIO::encoding };
unless ($@){
    $HAS_PERLIO = (PerlIO::encoding->VERSION >= 0.02);
}

sub import {
    my $class = shift;
    my $name  = shift;
    my %arg = @_;
    $name ||= $ENV{PERL_ENCODING};

    my $enc = find_encoding($name);
    unless (defined $enc) {
	require Carp;
	Carp::croak("Unknown encoding '$name'");
    }
    unless ($arg{Filter}){
	${^ENCODING} = $enc; # this is all you need, actually.
	$HAS_PERLIO or return 1;
	for my $h (qw(STDIN STDOUT)){
	    if ($arg{$h}){
		unless (defined find_encoding($arg{$h})) {
		    require Carp;
		    Carp::croak("Unknown encoding for $h, '$arg{$h}'");
		}
		eval { binmode($h, ":encoding($arg{$h})") };
	    }else{
		unless (exists $arg{$h}){
		    eval { 
			no warnings 'uninitialized';
			binmode($h, ":encoding($name)");
		    };
		}
	    }
	    if ($@){
		require Carp;
		Carp::croak($@);
	    }
	}
    }else{
	defined(${^ENCODING}) and undef ${^ENCODING};
	eval {
	    require Filter::Util::Call ;
	    Filter::Util::Call->import ;
	    binmode(STDIN);
	    binmode(STDOUT);
	    filter_add(sub{
			   my $status;
                           if (($status = filter_read()) > 0){
			       $_ = $enc->decode($_, 1);
			       # warn $_;
			   }
			   $status ;
		       });
	};
	# warn "Filter installed";
    }
    return 1; # I doubt if we need it, though
}

sub unimport{
    no warnings;
    undef ${^ENCODING};
    if ($HAS_PERLIO){
	binmode(STDIN,  ":raw");
	binmode(STDOUT, ":raw");
    }else{
    binmode(STDIN);
    binmode(STDOUT);
    }
    if ($INC{"Filter/Util/Call.pm"}){
	eval { filter_del() };
    }
}

1;
__END__

=pod

=head1 NAME

encoding - allows you to write your script in non-ascii or non-utf8

=head1 SYNOPSIS

  use encoding "greek";  # Perl like Greek to you?
  use encoding "euc-jp"; # Jperl!

  # or you can even do this if your shell supports your native encoding

  perl -Mencoding=latin2 -e '...' # Feeling centrally European?
  perl -Mencoding=euc-kr -e '...' # Or Korean?

  # more control

  # A simple euc-cn => utf-8 converter
  use encoding "euc-cn", STDOUT => "utf8";  while(<>){print};

  # "no encoding;" supported (but not scoped!)
  no encoding;

  # an alternate way, Filter
  use encoding "euc-jp", Filter=>1;
  use utf8;
  # now you can use kanji identifiers -- in euc-jp!

=head1 ABSTRACT

Let's start with a bit of history: Perl 5.6.0 introduced Unicode
support.  You could apply C<substr()> and regexes even to complex CJK
characters -- so long as the script was written in UTF-8.  But back
then, text editors that supported UTF-8 were still rare and many users
instead chose to write scripts in legacy encodings, giving up a whole
new feature of Perl 5.6.

Rewind to the future: starting from perl 5.8.0 with the B<encoding>
pragma, you can write your script in any encoding you like (so long
as the C<Encode> module supports it) and still enjoy Unicode support.
You can write code in EUC-JP as follows:

  my $Rakuda = "\xF1\xD1\xF1\xCC"; # Camel in Kanji
               #<-char-><-char->   # 4 octets
  s/\bCamel\b/$Rakuda/;

And with C<use encoding "euc-jp"> in effect, it is the same thing as
the code in UTF-8:

  my $Rakuda = "\x{99F1}\x{99DD}"; # two Unicode Characters
  s/\bCamel\b/$Rakuda/;

The B<encoding> pragma also modifies the filehandle disciplines of
STDIN, STDOUT, and STDERR to the specified encoding.  Therefore,

  use encoding "euc-jp";
  my $message = "Camel is the symbol of perl.\n";
  my $Rakuda = "\xF1\xD1\xF1\xCC"; # Camel in Kanji
  $message =~ s/\bCamel\b/$Rakuda/;
  print $message;

Will print "\xF1\xD1\xF1\xCC is the symbol of perl.\n",
not "\x{99F1}\x{99DD} is the symbol of perl.\n".

You can override this by giving extra arguments; see below.

=head1 USAGE

=over 4

=item use encoding [I<ENCNAME>] ;

Sets the script encoding to I<ENCNAME>. Filehandle disciplines of
STDIN and STDOUT are set to ":encoding(I<ENCNAME>)".  Note that STDERR
will not be changed.

If no encoding is specified, the environment variable L<PERL_ENCODING>
is consulted.  If no encoding can be found, the error C<Unknown encoding
'I<ENCNAME>'> will be thrown.

Note that non-STD file handles remain unaffected.  Use C<use open> or
C<binmode> to change disciplines of those.

=item use encoding I<ENCNAME> [ STDIN =E<gt> I<ENCNAME_IN> ...] ;

You can also individually set encodings of STDIN and STDOUT via the
C<< STDIN => I<ENCNAME> >> form.  In this case, you cannot omit the
first I<ENCNAME>.  C<< STDIN => undef >> turns the IO transcoding
completely off.

=item no encoding;

Unsets the script encoding. The disciplines of STDIN, STDOUT are
reset to ":raw" (the default unprocessed raw stream of bytes).

=back

=head1 CAVEATS

=head2 NOT SCOPED

The pragma is a per script, not a per block lexical.  Only the last
C<use encoding> or C<no encoding> matters, and it affects 
B<the whole script>.  However, the <no encoding> pragma is supported and 
B<use encoding> can appear as many times as you want in a given script. 
The multiple use of this pragma is discouraged.

Because of this nature, the use of this pragma inside the module is
strongly discouraged (because the influence of this pragma lasts not
only for the module but the script that uses).  But if you have to,
make sure you say C<no encoding> at the end of the module so you
contain the influence of the pragma within the module.

=head2 DO NOT MIX MULTIPLE ENCODINGS

Notice that only literals (string or regular expression) having only
legacy code points are affected: if you mix data like this

	\xDF\x{100}

the data is assumed to be in (Latin 1 and) Unicode, not in your native
encoding.  In other words, this will match in "greek":

	"\xDF" =~ /\x{3af}/

but this will not

	"\xDF\x{100}" =~ /\x{3af}\x{100}/

since the C<\xDF> (ISO 8859-7 GREEK SMALL LETTER IOTA WITH TONOS) on
the left will B<not> be upgraded to C<\x{3af}> (Unicode GREEK SMALL
LETTER IOTA WITH TONOS) because of the C<\x{100}> on the left.  You
should not be mixing your legacy data and Unicode in the same string.

This pragma also affects encoding of the 0x80..0xFF code point range:
normally characters in that range are left as eight-bit bytes (unless
they are combined with characters with code points 0x100 or larger,
in which case all characters need to become UTF-8 encoded), but if
the C<encoding> pragma is present, even the 0x80..0xFF range always
gets UTF-8 encoded.

After all, the best thing about this pragma is that you don't have to
resort to \x{....} just to spell your name in a native encoding.
So feel free to put your strings in your encoding in quotes and
regexes.

=head1 Non-ASCII Identifiers and Filter option

The magic of C<use encoding> is not applied to the names of
identifiers.  In order to make C<${"\x{4eba}"}++> ($human++, where human
is a single Han ideograph) work, you still need to write your script
in UTF-8 or use a source filter.

In other words, the same restriction as with Jperl applies.

If you dare to experiment, however, you can try the Filter option.

=over 4

=item use encoding I<ENCNAME> Filter=E<gt>1;

This turns the encoding pragma into a source filter.  While the default
approach just decodes interpolated literals (in qq() and qr()), this
will apply a source filter to the entire source code.  In this case,
STDIN and STDOUT remain untouched.

=back

What does this mean?  Your source code behaves as if it is written in
UTF-8.  So even if your editor only supports Shift_JIS, for example,
you can still try examples in Chapter 15 of C<Programming Perl, 3rd
Ed.>.  For instance, you can use UTF-8 identifiers.

This option is significantly slower and (as of this writing) non-ASCII
identifiers are not very stable WITHOUT this option and with the
source code written in UTF-8.

To make your script in legacy encoding work with minimum effort,
do not use Filter=E<gt>1.

=head1 EXAMPLE - Greekperl

    use encoding "iso 8859-7";

    # \xDF in ISO 8859-7 (Greek) is \x{3af} in Unicode.

    $a = "\xDF";
    $b = "\x{100}";

    printf "%#x\n", ord($a); # will print 0x3af, not 0xdf

    $c = $a . $b;

    # $c will be "\x{3af}\x{100}", not "\x{df}\x{100}".

    # chr() is affected, and ...

    print "mega\n"  if ord(chr(0xdf)) == 0x3af;

    # ... ord() is affected by the encoding pragma ...

    print "tera\n" if ord(pack("C", 0xdf)) == 0x3af;

    # ... as are eq and cmp ...

    print "peta\n" if "\x{3af}" eq  pack("C", 0xdf);
    print "exa\n"  if "\x{3af}" cmp pack("C", 0xdf) == 0;

    # ... but pack/unpack C are not affected, in case you still
    # want to go back to your native encoding

    print "zetta\n" if unpack("C", (pack("C", 0xdf))) == 0xdf;

=head1 KNOWN PROBLEMS

For native multibyte encodings (either fixed or variable length),
the current implementation of the regular expressions may introduce
recoding errors for regular expression literals longer than 127 bytes.

The encoding pragma is not supported on EBCDIC platforms.
(Porters who are willing and able to remove this limitation are
welcome.)

=head1 SEE ALSO

L<perlunicode>, L<Encode>, L<open>, L<Filter::Util::Call>,

Ch. 15 of C<Programming Perl (3rd Edition)>
by Larry Wall, Tom Christiansen, Jon Orwant;
O'Reilly & Associates; ISBN 0-596-00027-8

=cut
