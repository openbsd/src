package FileHandle;

=head1 NAME

FileHandle - supply object methods for filehandles

=head1 SYNOPSIS

    use FileHandle;

    $fh = new FileHandle;
    if ($fh->open "< file") {
        print <$fh>;
        $fh->close;
    }

    $fh = new FileHandle "> FOO";
    if (defined $fh) {
        print $fh "bar\n";
        $fh->close;
    }

    $fh = new FileHandle "file", "r";
    if (defined $fh) {
        print <$fh>;
        undef $fh;       # automatically closes the file
    }

    $fh = new FileHandle "file", O_WRONLY|O_APPEND;
    if (defined $fh) {
        print $fh "corge\n";
        undef $fh;       # automatically closes the file
    }

    $pos = $fh->getpos;
    $fh->setpos $pos;

    $fh->setvbuf($buffer_var, _IOLBF, 1024);

    ($readfh, $writefh) = FileHandle::pipe;

    autoflush STDOUT 1;

=head1 DESCRIPTION

C<FileHandle::new> creates a C<FileHandle>, which is a reference to a
newly created symbol (see the C<Symbol> package).  If it receives any
parameters, they are passed to C<FileHandle::open>; if the open fails,
the C<FileHandle> object is destroyed.  Otherwise, it is returned to
the caller.

C<FileHandle::new_from_fd> creates a C<FileHandle> like C<new> does.
It requires two parameters, which are passed to C<FileHandle::fdopen>;
if the fdopen fails, the C<FileHandle> object is destroyed.
Otherwise, it is returned to the caller.

C<FileHandle::open> accepts one parameter or two.  With one parameter,
it is just a front end for the built-in C<open> function.  With two
parameters, the first parameter is a filename that may include
whitespace or other special characters, and the second parameter is
the open mode in either Perl form (">", "+<", etc.) or POSIX form
("w", "r+", etc.).

C<FileHandle::fdopen> is like C<open> except that its first parameter
is not a filename but rather a file handle name, a FileHandle object,
or a file descriptor number.

If the C functions fgetpos() and fsetpos() are available, then
C<FileHandle::getpos> returns an opaque value that represents the
current position of the FileHandle, and C<FileHandle::setpos> uses
that value to return to a previously visited position.

If the C function setvbuf() is available, then C<FileHandle::setvbuf>
sets the buffering policy for the FileHandle.  The calling sequence
for the Perl function is the same as its C counterpart, including the
macros C<_IOFBF>, C<_IOLBF>, and C<_IONBF>, except that the buffer
parameter specifies a scalar variable to use as a buffer.  WARNING: A
variable used as a buffer by C<FileHandle::setvbuf> must not be
modified in any way until the FileHandle is closed or until
C<FileHandle::setvbuf> is called again, or memory corruption may
result!

See L<perlfunc> for complete descriptions of each of the following
supported C<FileHandle> methods, which are just front ends for the
corresponding built-in functions:
  
    close
    fileno
    getc
    gets
    eof
    clearerr
    seek
    tell

See L<perlvar> for complete descriptions of each of the following
supported C<FileHandle> methods:

    autoflush
    output_field_separator
    output_record_separator
    input_record_separator
    input_line_number
    format_page_number
    format_lines_per_page
    format_lines_left
    format_name
    format_top_name
    format_line_break_characters
    format_formfeed

Furthermore, for doing normal I/O you might need these:

=over 

=item $fh->print

See L<perlfunc/print>.

=item $fh->printf

See L<perlfunc/printf>.

=item $fh->getline

This works like <$fh> described in L<perlop/"I/O Operators">
except that it's more readable and can be safely called in an
array context but still returns just one line.

=item $fh->getlines

This works like <$fh> when called in an array context to
read all the remaining lines in a file, except that it's more readable.
It will also croak() if accidentally called in a scalar context.

=back

=head1 SEE ALSO

L<perlfunc>, 
L<perlop/"I/O Operators">,
L<POSIX/"FileHandle">

=head1 BUGS

Due to backwards compatibility, all filehandles resemble objects
of class C<FileHandle>, or actually classes derived from that class.
They actually aren't.  Which means you can't derive your own 
class from C<FileHandle> and inherit those methods.

=cut

require 5.000;
use vars qw($VERSION @EXPORT @EXPORT_OK $AUTOLOAD);
use Carp;
use Symbol;
use SelectSaver;

require Exporter;
require DynaLoader;
@ISA = qw(Exporter DynaLoader);

$VERSION = "1.00" ;

@EXPORT = qw(_IOFBF _IOLBF _IONBF);

@EXPORT_OK = qw(
    autoflush
    output_field_separator
    output_record_separator
    input_record_separator
    input_line_number
    format_page_number
    format_lines_per_page
    format_lines_left
    format_name
    format_top_name
    format_line_break_characters
    format_formfeed

    print
    printf
    getline
    getlines
);


################################################
## If the Fcntl extension is available,
##  export its constants.
##

sub import {
    my $pkg = shift;
    my $callpkg = caller;
    Exporter::export $pkg, $callpkg;
    eval {
	require Fcntl;
	Exporter::export 'Fcntl', $callpkg;
    };
};


################################################
## Interaction with the XS.
##

eval {
    bootstrap FileHandle;
};
if ($@) {
    *constant = sub { undef };
}

sub AUTOLOAD {
    if ($AUTOLOAD =~ /::(_?[a-z])/) {
	$AutoLoader::AUTOLOAD = $AUTOLOAD;
	goto &AutoLoader::AUTOLOAD
    }
    my $constname = $AUTOLOAD;
    $constname =~ s/.*:://;
    my $val = constant($constname);
    defined $val or croak "$constname is not a valid FileHandle macro";
    *$AUTOLOAD = sub { $val };
    goto &$AUTOLOAD;
}


################################################
## Constructors, destructors.
##

sub new {
    @_ >= 1 && @_ <= 3 or croak 'usage: new FileHandle [FILENAME [,MODE]]';
    my $class = shift;
    my $fh = gensym;
    if (@_) {
	FileHandle::open($fh, @_)
	    or return undef;
    }
    bless $fh, $class;
}

sub new_from_fd {
    @_ == 3 or croak 'usage: new_from_fd FileHandle FD, MODE';
    my $class = shift;
    my $fh = gensym;
    FileHandle::fdopen($fh, @_)
	or return undef;
    bless $fh, $class;
}

sub DESTROY {
    my ($fh) = @_;
    close($fh);
}

################################################
## Open and close.
##

sub pipe {
    @_ and croak 'usage: FileHandle::pipe()';
    my $readfh = new FileHandle;
    my $writefh = new FileHandle;
    pipe($readfh, $writefh)
	or return undef;
    ($readfh, $writefh);
}

sub _open_mode_string {
    my ($mode) = @_;
    $mode =~ /^\+?(<|>>?)$/
      or $mode =~ s/^r(\+?)$/$1</
      or $mode =~ s/^w(\+?)$/$1>/
      or $mode =~ s/^a(\+?)$/$1>>/
      or croak "FileHandle: bad open mode: $mode";
    $mode;
}

sub open {
    @_ >= 2 && @_ <= 4 or croak 'usage: $fh->open(FILENAME [,MODE [,PERMS]])';
    my ($fh, $file) = @_;
    if (@_ > 2) {
	my ($mode, $perms) = @_[2, 3];
	if ($mode =~ /^\d+$/) {
	    defined $perms or $perms = 0666;
	    return sysopen($fh, $file, $mode, $perms);
	}
        $file = "./" . $file unless $file =~ m#^/#;
	$file = _open_mode_string($mode) . " $file\0";
    }
    open($fh, $file);
}

sub fdopen {
    @_ == 3 or croak 'usage: $fh->fdopen(FD, MODE)';
    my ($fh, $fd, $mode) = @_;
    if (ref($fd) =~ /GLOB\(/) {
	# It's a glob reference; remove the star from its name.
	($fd = "".$$fd) =~ s/^\*//;
    } elsif ($fd =~ m#^\d+$#) {
	# It's an FD number; prefix with "=".
	$fd = "=$fd";
    }
    open($fh, _open_mode_string($mode) . '&' . $fd);
}

sub close {
    @_ == 1 or croak 'usage: $fh->close()';
    close($_[0]);
}

################################################
## Normal I/O functions.
##

sub fileno {
    @_ == 1 or croak 'usage: $fh->fileno()';
    fileno($_[0]);
}

sub getc {
    @_ == 1 or croak 'usage: $fh->getc()';
    getc($_[0]);
}

sub gets {
    @_ == 1 or croak 'usage: $fh->gets()';
    my ($handle) = @_;
    scalar <$handle>;
}

sub eof {
    @_ == 1 or croak 'usage: $fh->eof()';
    eof($_[0]);
}

sub clearerr {
    @_ == 1 or croak 'usage: $fh->clearerr()';
    seek($_[0], 0, 1);
}

sub seek {
    @_ == 3 or croak 'usage: $fh->seek(POS, WHENCE)';
    seek($_[0], $_[1], $_[2]);
}

sub tell {
    @_ == 1 or croak 'usage: $fh->tell()';
    tell($_[0]);
}

sub print {
    @_ or croak 'usage: $fh->print([ARGS])';
    my $this = shift;
    print $this @_;
}

sub printf {
    @_ or croak 'usage: $fh->printf([ARGS])';
    my $this = shift;
    printf $this @_;
}

sub getline {
    @_ == 1 or croak 'usage: $fh->getline';
    my $this = shift;
    return scalar <$this>;
} 

sub getlines {
    @_ == 1 or croak 'usage: $fh->getline()';
    my $this = shift;
    wantarray or croak "Can't call FileHandle::getlines in a scalar context";
    return <$this>;
}

################################################
## State modification functions.
##

sub autoflush {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $|;
    $| = @_ > 1 ? $_[1] : 1;
    $prev;
}

sub output_field_separator {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $,;
    $, = $_[1] if @_ > 1;
    $prev;
}

sub output_record_separator {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $\;
    $\ = $_[1] if @_ > 1;
    $prev;
}

sub input_record_separator {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $/;
    $/ = $_[1] if @_ > 1;
    $prev;
}

sub input_line_number {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $.;
    $. = $_[1] if @_ > 1;
    $prev;
}

sub format_page_number {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $%;
    $% = $_[1] if @_ > 1;
    $prev;
}

sub format_lines_per_page {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $=;
    $= = $_[1] if @_ > 1;
    $prev;
}

sub format_lines_left {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $-;
    $- = $_[1] if @_ > 1;
    $prev;
}

sub format_name {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $~;
    $~ = qualify($_[1], caller) if @_ > 1;
    $prev;
}

sub format_top_name {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $^;
    $^ = qualify($_[1], caller) if @_ > 1;
    $prev;
}

sub format_line_break_characters {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $:;
    $: = $_[1] if @_ > 1;
    $prev;
}

sub format_formfeed {
    my $old = new SelectSaver qualify($_[0], caller);
    my $prev = $^L;
    $^L = $_[1] if @_ > 1;
    $prev;
}

1;
