package open;
use warnings;
use Carp;
$open::hint_bits = 0x20000;

our $VERSION = '1.01';

my $locale_encoding;

sub in_locale { $^H & ($locale::hint_bits || 0)}

sub _get_locale_encoding {
    unless (defined $locale_encoding) {
	# I18N::Langinfo isn't available everywhere
	eval {
	    require I18N::Langinfo;
	    I18N::Langinfo->import(qw(langinfo CODESET));
	    $locale_encoding = langinfo(CODESET());
	};
	my $country_language;

	no warnings 'uninitialized';

        if (not $locale_encoding && in_locale()) {
	    if ($ENV{LC_ALL} =~ /^([^.]+)\.([^.]+)$/) {
		($country_language, $locale_encoding) = ($1, $2);
	    } elsif ($ENV{LANG} =~ /^([^.]+)\.([^.]+)$/) {
		($country_language, $locale_encoding) = ($1, $2);
	    }
	} elsif (not $locale_encoding) {
	    if ($ENV{LC_ALL} =~ /\butf-?8\b/i ||
		$ENV{LANG}   =~ /\butf-?8\b/i) {
		$locale_encoding = 'utf8';
	    }
	    # Could do more heuristics based on the country and language
	    # parts of LC_ALL and LANG (the parts before the dot (if any)),
	    # since we have Locale::Country and Locale::Language available.
	    # TODO: get a database of Language -> Encoding mappings
	    # (the Estonian database at http://www.eki.ee/letter/
	    # would be excellent!) --jhi
	}
	if (defined $locale_encoding &&
	    $locale_encoding eq 'euc' &&
	    defined $country_language) {
	    if ($country_language =~ /^ja_JP|japan(?:ese)?$/i) {
		$locale_encoding = 'euc-jp';
	    } elsif ($country_language =~ /^ko_KR|korean?$/i) {
		$locale_encoding = 'euc-kr';
	    } elsif ($country_language =~ /^zh_CN|chin(?:a|ese)?$/i) {
		$locale_encoding = 'euc-cn';
	    } elsif ($country_language =~ /^zh_TW|taiwan(?:ese)?$/i) {
		$locale_encoding = 'euc-tw';
	    }
	    croak "Locale encoding 'euc' too ambiguous"
		if $locale_encoding eq 'euc';
	}
    }
}

sub import {
    my ($class,@args) = @_;
    croak("`use open' needs explicit list of PerlIO layers") unless @args;
    my $std;
    $^H |= $open::hint_bits;
    my ($in,$out) = split(/\0/,(${^OPEN} || "\0"), -1);
    while (@args) {
	my $type = shift(@args);
	my $dscp;
	if ($type =~ /^:?(utf8|locale|encoding\(.+\))$/) {
	    $type = 'IO';
	    $dscp = ":$1";
	} elsif ($type eq ':std') {
	    $std = 1;
	    next;
	} else {
	    $dscp = shift(@args) || '';
	}
	my @val;
	foreach my $layer (split(/\s+/,$dscp)) {
            $layer =~ s/^://;
	    if ($layer eq 'locale') {
		use Encode;
		_get_locale_encoding()
		    unless defined $locale_encoding;
		(warnings::warnif("layer", "Cannot figure out an encoding to use"), last)
		    unless defined $locale_encoding;
		if ($locale_encoding =~ /^utf-?8$/i) {
		    $layer = "utf8";
		} else {
		    $layer = "encoding($locale_encoding)";
		}
		$std = 1;
	    } else {
		my $target = $layer;		# the layer name itself
		$target =~ s/^(\w+)\(.+\)$/$1/;	# strip parameters

		unless(PerlIO::Layer::->find($target)) {
		    warnings::warnif("layer", "Unknown PerlIO layer '$layer'");
		}
	    }
	    push(@val,":$layer");
	    if ($layer =~ /^(crlf|raw)$/) {
		$^H{"open_$type"} = $layer;
	    }
	}
	if ($type eq 'IN') {
	    $in  = join(' ',@val);
	}
	elsif ($type eq 'OUT') {
	    $out = join(' ',@val);
	}
	elsif ($type eq 'IO') {
	    $in = $out = join(' ',@val);
	}
	else {
	    croak "Unknown PerlIO layer class '$type'";
	}
    }
    ${^OPEN} = join("\0",$in,$out) if $in or $out;
    if ($std) {
	if ($in) {
	    if ($in =~ /:utf8\b/) {
		    binmode(STDIN,  ":utf8");
		} elsif ($in =~ /(\w+\(.+\))/) {
		    binmode(STDIN,  ":$1");
		}
	}
	if ($out) {
	    if ($out =~ /:utf8\b/) {
		binmode(STDOUT,  ":utf8");
		binmode(STDERR,  ":utf8");
	    } elsif ($out =~ /(\w+\(.+\))/) {
		binmode(STDOUT,  ":$1");
		binmode(STDERR,  ":$1");
	    }
	}
    }
}

1;
__END__

=head1 NAME

open - perl pragma to set default PerlIO layers for input and output

=head1 SYNOPSIS

    use open IN  => ":crlf", OUT => ":bytes";
    use open OUT => ':utf8';
    use open IO  => ":encoding(iso-8859-7)";

    use open IO  => ':locale';

    use open ':utf8';
    use open ':locale';
    use open ':encoding(iso-8859-7)';

    use open ':std';

=head1 DESCRIPTION

Full-fledged support for I/O layers is now implemented provided
Perl is configured to use PerlIO as its IO system (which is now the
default).

The C<open> pragma serves as one of the interfaces to declare default
"layers" (also known as "disciplines") for all I/O. Any open(),
readpipe() (aka qx//) and similar operators found within the lexical
scope of this pragma will use the declared defaults.

With the C<IN> subpragma you can declare the default layers
of input streams, and with the C<OUT> subpragma you can declare
the default layers of output streams.  With the C<IO>  subpragma
you can control both input and output streams simultaneously.

If you have a legacy encoding, you can use the C<:encoding(...)> tag.

if you want to set your encoding layers based on your
locale environment variables, you can use the C<:locale> tag.
For example:

    $ENV{LANG} = 'ru_RU.KOI8-R';
    # the :locale will probe the locale environment variables like LANG
    use open OUT => ':locale';
    open(O, ">koi8");
    print O chr(0x430); # Unicode CYRILLIC SMALL LETTER A = KOI8-R 0xc1
    close O;
    open(I, "<koi8");
    printf "%#x\n", ord(<I>), "\n"; # this should print 0xc1
    close I;

These are equivalent

    use open ':utf8';
    use open IO => ':utf8';

as are these

    use open ':locale';
    use open IO => ':locale';

and these

    use open ':encoding(iso-8859-7)';
    use open IO => ':encoding(iso-8859-7)';

The matching of encoding names is loose: case does not matter, and
many encodings have several aliases.  See L<Encode::Supported> for
details and the list of supported locales.

Note that C<:utf8> PerlIO layer must always be specified exactly like
that, it is not subject to the loose matching of encoding names.

When open() is given an explicit list of layers they are appended to
the list declared using this pragma.

The C<:std> subpragma on its own has no effect, but if combined with
the C<:utf8> or C<:encoding> subpragmas, it converts the standard
filehandles (STDIN, STDOUT, STDERR) to comply with encoding selected
for input/output handles.  For example, if both input and out are
chosen to be C<:utf8>, a C<:std> will mean that STDIN, STDOUT, and
STDERR are also in C<:utf8>.  On the other hand, if only output is
chosen to be in C<< :encoding(koi8r) >>, a C<:std> will cause only the
STDOUT and STDERR to be in C<koi8r>.  The C<:locale> subpragma
implicitly turns on C<:std>.

The logic of C<:locale> is as follows:

=over 4

=item 1.

If the platform supports the langinfo(CODESET) interface, the codeset
returned is used as the default encoding for the open pragma.

=item 2.

If 1. didn't work but we are under the locale pragma, the environment
variables LC_ALL and LANG (in that order) are matched for encodings
(the part after C<.>, if any), and if any found, that is used 
as the default encoding for the open pragma.

=item 3.

If 1. and 2. didn't work, the environment variables LC_ALL and LANG
(in that order) are matched for anything looking like UTF-8, and if
any found, C<:utf8> is used as the default encoding for the open
pragma.

=back

If your locale environment variables (LANGUAGE, LC_ALL, LC_CTYPE, LANG)
contain the strings 'UTF-8' or 'UTF8' (case-insensitive matching),
the default encoding of your STDIN, STDOUT, and STDERR, and of
B<any subsequent file open>, is UTF-8.

Directory handles may also support PerlIO layers in the future.

=head1 NONPERLIO FUNCTIONALITY

If Perl is not built to use PerlIO as its IO system then only the two
pseudo-layers C<:bytes> and C<:crlf> are available.

The C<:bytes> layer corresponds to "binary mode" and the C<:crlf>
layer corresponds to "text mode" on platforms that distinguish
between the two modes when opening files (which is many DOS-like
platforms, including Windows).  These two layers are no-ops on
platforms where binmode() is a no-op, but perform their functions
everywhere if PerlIO is enabled.

=head1 IMPLEMENTATION DETAILS

There is a class method in C<PerlIO::Layer> C<find> which is
implemented as XS code.  It is called by C<import> to validate the
layers:

   PerlIO::Layer::->find("perlio")

The return value (if defined) is a Perl object, of class
C<PerlIO::Layer> which is created by the C code in F<perlio.c>.  As
yet there is nothing useful you can do with the object at the perl
level.

=head1 SEE ALSO

L<perlfunc/"binmode">, L<perlfunc/"open">, L<perlunicode>, L<PerlIO>,
L<encoding>

=cut
