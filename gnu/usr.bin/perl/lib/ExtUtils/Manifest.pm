package ExtUtils::Manifest;

require Exporter;
use Config;
use File::Find;
use File::Copy 'copy';
use File::Spec;
use Carp;
use strict;

use vars qw($VERSION @ISA @EXPORT_OK 
          $Is_MacOS $Is_VMS 
          $Debug $Verbose $Quiet $MANIFEST $DEFAULT_MSKIP);

$VERSION = 1.38;
@ISA=('Exporter');
@EXPORT_OK = ('mkmanifest', 'manicheck', 'fullcheck', 'filecheck', 
	      'skipcheck', 'maniread', 'manicopy');

$Is_MacOS = $^O eq 'MacOS';
$Is_VMS = $^O eq 'VMS';
require VMS::Filespec if $Is_VMS;

$Debug = $ENV{PERL_MM_MANIFEST_DEBUG} || 0;
$Verbose = defined $ENV{PERL_MM_MANIFEST_VERBOSE} ?
                   $ENV{PERL_MM_MANIFEST_VERBOSE} : 1;
$Quiet = 0;
$MANIFEST = 'MANIFEST';
$DEFAULT_MSKIP = (File::Spec->splitpath($INC{"ExtUtils/Manifest.pm"}))[1].
                 "$MANIFEST.SKIP";

sub mkmanifest {
    my $manimiss = 0;
    my $read = (-r 'MANIFEST' && maniread()) or $manimiss++;
    $read = {} if $manimiss;
    local *M;
    rename $MANIFEST, "$MANIFEST.bak" unless $manimiss;
    open M, ">$MANIFEST" or die "Could not open $MANIFEST: $!";
    my $skip = _maniskip();
    my $found = manifind();
    my($key,$val,$file,%all);
    %all = (%$found, %$read);
    $all{$MANIFEST} = ($Is_VMS ? "$MANIFEST\t\t" : '') . 'This list of files'
        if $manimiss; # add new MANIFEST to known file list
    foreach $file (sort keys %all) {
	if ($skip->($file)) {
	    # Policy: only remove files if they're listed in MANIFEST.SKIP.
	    # Don't remove files just because they don't exist.
	    warn "Removed from $MANIFEST: $file\n" if $Verbose and exists $read->{$file};
	    next;
	}
	if ($Verbose){
	    warn "Added to $MANIFEST: $file\n" unless exists $read->{$file};
	}
	my $text = $all{$file};
	($file,$text) = split(/\s+/,$text,2) if $Is_VMS && $text;
	$file = _unmacify($file);
	my $tabs = (5 - (length($file)+1)/8);
	$tabs = 1 if $tabs < 1;
	$tabs = 0 unless $text;
	print M $file, "\t" x $tabs, $text, "\n";
    }
    close M;
}

# Geez, shouldn't this use File::Spec or File::Basename or something?  
# Why so careful about dependencies?
sub clean_up_filename {
  my $filename = shift;
  $filename =~ s|^\./||;
  $filename =~ s/^:([^:]+)$/$1/ if $Is_MacOS;
  return $filename;
}

sub manifind {
    my $p = shift || {};
    my $found = {};

    my $wanted = sub {
	my $name = clean_up_filename($File::Find::name);
	warn "Debug: diskfile $name\n" if $Debug;
	return if -d $_;
	
        if( $Is_VMS ) {
            $name =~ s#(.*)\.$#\L$1#;
            $name = uc($name) if $name =~ /^MANIFEST(\.SKIP)?$/i;
        }
	$found->{$name} = "";
    };

    # We have to use "$File::Find::dir/$_" in preprocess, because 
    # $File::Find::name is unavailable.
    # Also, it's okay to use / here, because MANIFEST files use Unix-style 
    # paths.
    find({wanted => $wanted},
	 $Is_MacOS ? ":" : ".");

    return $found;
}

sub fullcheck {
    return [_check_files()], [_check_manifest()];
}

sub manicheck {
    return _check_files();
}

sub filecheck {
    return _check_manifest();
}

sub skipcheck {
    my($p) = @_;
    my $found = manifind();
    my $matches = _maniskip();

    my @skipped = ();
    foreach my $file (sort keys %$found){
        if (&$matches($file)){
            warn "Skipping $file\n";
            push @skipped, $file;
            next;
        }
    }

    return @skipped;
}


sub _check_files {
    my $p = shift;
    my $dosnames=(defined(&Dos::UseLFN) && Dos::UseLFN()==0);
    my $read = maniread() || {};
    my $found = manifind($p);

    my(@missfile) = ();
    foreach my $file (sort keys %$read){
        warn "Debug: manicheck checking from $MANIFEST $file\n" if $Debug;
        if ($dosnames){
            $file = lc $file;
            $file =~ s=(\.(\w|-)+)=substr ($1,0,4)=ge;
            $file =~ s=((\w|-)+)=substr ($1,0,8)=ge;
        }
        unless ( exists $found->{$file} ) {
            warn "No such file: $file\n" unless $Quiet;
            push @missfile, $file;
        }
    }

    return @missfile;
}


sub _check_manifest {
    my($p) = @_;
    my $read = maniread() || {};
    my $found = manifind($p);
    my $skip  = _maniskip();

    my @missentry = ();
    foreach my $file (sort keys %$found){
        next if $skip->($file);
        warn "Debug: manicheck checking from disk $file\n" if $Debug;
        unless ( exists $read->{$file} ) {
            my $canon = $Is_MacOS ? "\t" . _unmacify($file) : '';
            warn "Not in $MANIFEST: $file$canon\n" unless $Quiet;
            push @missentry, $file;
        }
    }

    return @missentry;
}


sub maniread {
    my ($mfile) = @_;
    $mfile ||= $MANIFEST;
    my $read = {};
    local *M;
    unless (open M, $mfile){
	warn "$mfile: $!";
	return $read;
    }
    while (<M>){
	chomp;
	next if /^#/;

        my($file, $comment) = /^(\S+)\s*(.*)/;
        next unless $file;

	if ($Is_MacOS) {
	    $file = _macify($file);
	    $file =~ s/\\([0-3][0-7][0-7])/sprintf("%c", oct($1))/ge;
	}
	elsif ($Is_VMS) {
        require File::Basename;
	    my($base,$dir) = File::Basename::fileparse($file);
	    # Resolve illegal file specifications in the same way as tar
	    $dir =~ tr/./_/;
	    my(@pieces) = split(/\./,$base);
	    if (@pieces > 2) { $base = shift(@pieces) . '.' . join('_',@pieces); }
	    my $okfile = "$dir$base";
	    warn "Debug: Illegal name $file changed to $okfile\n" if $Debug;
            $file = $okfile;
            $file = lc($file) unless $file =~ /^MANIFEST(\.SKIP)?$/;
	}

        $read->{$file} = $comment;
    }
    close M;
    $read;
}

# returns an anonymous sub that decides if an argument matches
sub _maniskip {
    my @skip ;
    my $mfile = "$MANIFEST.SKIP";
    local *M;
    open M, $mfile or open M, $DEFAULT_MSKIP or return sub {0};
    while (<M>){
	chomp;
	next if /^#/;
	next if /^\s*$/;
	push @skip, _macify($_);
    }
    close M;
    my $opts = $Is_VMS ? '(?i)' : '';

    # Make sure each entry is isolated in its own parentheses, in case
    # any of them contain alternations
    my $regex = join '|', map "(?:$_)", @skip;

    return sub { $_[0] =~ qr{$opts$regex} };
}

sub manicopy {
    my($read,$target,$how)=@_;
    croak "manicopy() called without target argument" unless defined $target;
    $how ||= 'cp';
    require File::Path;
    require File::Basename;

    $target = VMS::Filespec::unixify($target) if $Is_VMS;
    File::Path::mkpath([ $target ],! $Quiet,$Is_VMS ? undef : 0755);
    foreach my $file (keys %$read){
    	if ($Is_MacOS) {
	    if ($file =~ m!:!) { 
	   	my $dir = _maccat($target, $file);
		$dir =~ s/[^:]+$//;
	    	File::Path::mkpath($dir,1,0755);
	    }
	    cp_if_diff($file, _maccat($target, $file), $how);
	} else {
	    $file = VMS::Filespec::unixify($file) if $Is_VMS;
	    if ($file =~ m!/!) { # Ilya, that hurts, I fear, or maybe not?
		my $dir = File::Basename::dirname($file);
		$dir = VMS::Filespec::unixify($dir) if $Is_VMS;
		File::Path::mkpath(["$target/$dir"],! $Quiet,$Is_VMS ? undef : 0755);
	    }
	    cp_if_diff($file, "$target/$file", $how);
	}
    }
}

sub cp_if_diff {
    my($from, $to, $how)=@_;
    -f $from or carp "$0: $from not found";
    my($diff) = 0;
    local(*F,*T);
    open(F,"< $from\0") or die "Can't read $from: $!\n";
    if (open(T,"< $to\0")) {
	while (<F>) { $diff++,last if $_ ne <T>; }
	$diff++ unless eof(T);
	close T;
    }
    else { $diff++; }
    close F;
    if ($diff) {
	if (-e $to) {
	    unlink($to) or confess "unlink $to: $!";
	}
      STRICT_SWITCH: {
	    best($from,$to), last STRICT_SWITCH if $how eq 'best';
	    cp($from,$to), last STRICT_SWITCH if $how eq 'cp';
	    ln($from,$to), last STRICT_SWITCH if $how eq 'ln';
	    croak("ExtUtils::Manifest::cp_if_diff " .
		  "called with illegal how argument [$how]. " .
		  "Legal values are 'best', 'cp', and 'ln'.");
	}
    }
}

sub cp {
    my ($srcFile, $dstFile) = @_;
    my ($perm,$access,$mod) = (stat $srcFile)[2,8,9];
    copy($srcFile,$dstFile);
    utime $access, $mod + ($Is_VMS ? 1 : 0), $dstFile;
    # chmod a+rX-w,go-w
    chmod(  0444 | ( $perm & 0111 ? 0111 : 0 ),  $dstFile ) 
      unless ($^O eq 'MacOS');
}

sub ln {
    my ($srcFile, $dstFile) = @_;
    return &cp if $Is_VMS or ($^O eq 'MSWin32' and Win32::IsWin95());
    link($srcFile, $dstFile);

    # chmod a+r,go-w+X (except "X" only applies to u=x)
    local($_) = $dstFile;
    my $mode= 0444 | (stat)[2] & 0700;
    if (! chmod(  $mode | ( $mode & 0100 ? 0111 : 0 ),  $_  )) {
        unlink $dstFile;
        return;
    }
    1;
}

unless (defined $Config{d_link}) {
    # Really cool fix from Ilya :)
    local $SIG{__WARN__} = sub { 
        warn @_ unless $_[0] =~ /^Subroutine .* redefined/;
    };
    *ln = \&cp;
}




sub best {
    my ($srcFile, $dstFile) = @_;
    if (-l $srcFile) {
	cp($srcFile, $dstFile);
    } else {
	ln($srcFile, $dstFile) or cp($srcFile, $dstFile);
    }
}

sub _macify {
    my($file) = @_;

    return $file unless $Is_MacOS;
    
    $file =~ s|^\./||;
    if ($file =~ m|/|) {
	$file =~ s|/+|:|g;
	$file = ":$file";
    }
    
    $file;
}

sub _maccat {
    my($f1, $f2) = @_;
    
    return "$f1/$f2" unless $Is_MacOS;
    
    $f1 .= ":$f2";
    $f1 =~ s/([^:]:):/$1/g;
    return $f1;
}

sub _unmacify {
    my($file) = @_;

    return $file unless $Is_MacOS;
    
    $file =~ s|^:||;
    $file =~ s|([/ \n])|sprintf("\\%03o", unpack("c", $1))|ge;
    $file =~ y|:|/|;
    
    $file;
}

1;

__END__

=head1 NAME

ExtUtils::Manifest - utilities to write and check a MANIFEST file

=head1 SYNOPSIS

    require ExtUtils::Manifest;

    ExtUtils::Manifest::mkmanifest;

    ExtUtils::Manifest::manicheck;

    ExtUtils::Manifest::filecheck;

    ExtUtils::Manifest::fullcheck;

    ExtUtils::Manifest::skipcheck;

    ExtUtils::Manifest::manifind();

    ExtUtils::Manifest::maniread($file);

    ExtUtils::Manifest::manicopy($read,$target,$how);

=head1 DESCRIPTION

mkmanifest() writes all files in and below the current directory to a
file named in the global variable $ExtUtils::Manifest::MANIFEST (which
defaults to C<MANIFEST>) in the current directory. It works similar to

    find . -print

but in doing so checks each line in an existing C<MANIFEST> file and
includes any comments that are found in the existing C<MANIFEST> file
in the new one. Anything between white space and an end of line within
a C<MANIFEST> file is considered to be a comment. Filenames and
comments are separated by one or more TAB characters in the
output. All files that match any regular expression in a file
C<MANIFEST.SKIP> (if such a file exists) are ignored.

manicheck() checks if all the files within a C<MANIFEST> in the current
directory really do exist. If C<MANIFEST> and the tree below the current
directory are in sync it exits silently, returning an empty list.  Otherwise
it returns a list of files which are listed in the C<MANIFEST> but missing
from the directory, and by default also outputs these names to STDERR.

filecheck() finds files below the current directory that are not
mentioned in the C<MANIFEST> file. An optional file C<MANIFEST.SKIP>
will be consulted. Any file matching a regular expression in such a
file will not be reported as missing in the C<MANIFEST> file. The list of
any extraneous files found is returned, and by default also reported to
STDERR.

fullcheck() does both a manicheck() and a filecheck(), returning references
to two arrays, the first for files manicheck() found to be missing, the
seond for unexpeced files found by filecheck().

skipcheck() lists all the files that are skipped due to your
C<MANIFEST.SKIP> file.

manifind() returns a hash reference. The keys of the hash are the
files found below the current directory.

maniread($file) reads a named C<MANIFEST> file (defaults to
C<MANIFEST> in the current directory) and returns a HASH reference
with files being the keys and comments being the values of the HASH.
Blank lines and lines which start with C<#> in the C<MANIFEST> file
are discarded.

C<manicopy($read,$target,$how)> copies the files that are the keys in
the HASH I<%$read> to the named target directory. The HASH reference
$read is typically returned by the maniread() function. This
function is useful for producing a directory tree identical to the
intended distribution tree. The third parameter $how can be used to
specify a different methods of "copying". Valid values are C<cp>,
which actually copies the files, C<ln> which creates hard links, and
C<best> which mostly links the files but copies any symbolic link to
make a tree without any symbolic link. Best is the default.

=head1 MANIFEST.SKIP

The file MANIFEST.SKIP may contain regular expressions of files that
should be ignored by mkmanifest() and filecheck(). The regular
expressions should appear one on each line. Blank lines and lines
which start with C<#> are skipped.  Use C<\#> if you need a regular
expression to start with a sharp character. A typical example:

    # Version control files and dirs.
    \bRCS\b
    \bCVS\b
    ,v$

    # Makemaker generated files and dirs.
    ^MANIFEST\.
    ^Makefile$
    ^blib/
    ^MakeMaker-\d

    # Temp, old and emacs backup files.
    ~$
    \.old$
    ^#.*#$
    ^\.#

If no MANIFEST.SKIP file is found, a default set of skips will be
used, similar to the example above.  If you want nothing skipped,
simply make an empty MANIFEST.SKIP file.


=head1 EXPORT_OK

C<&mkmanifest>, C<&manicheck>, C<&filecheck>, C<&fullcheck>,
C<&maniread>, and C<&manicopy> are exportable.

=head1 GLOBAL VARIABLES

C<$ExtUtils::Manifest::MANIFEST> defaults to C<MANIFEST>. Changing it
results in both a different C<MANIFEST> and a different
C<MANIFEST.SKIP> file. This is useful if you want to maintain
different distributions for different audiences (say a user version
and a developer version including RCS).

C<$ExtUtils::Manifest::Quiet> defaults to 0. If set to a true value,
all functions act silently.

C<$ExtUtils::Manifest::Debug> defaults to 0.  If set to a true value,
or if PERL_MM_MANIFEST_DEBUG is true, debugging output will be
produced.

=head1 DIAGNOSTICS

All diagnostic output is sent to C<STDERR>.

=over 4

=item C<Not in MANIFEST:> I<file>

is reported if a file is found which is not in C<MANIFEST>.

=item C<Skipping> I<file>

is reported if a file is skipped due to an entry in C<MANIFEST.SKIP>.

=item C<No such file:> I<file>

is reported if a file mentioned in a C<MANIFEST> file does not
exist.

=item C<MANIFEST:> I<$!>

is reported if C<MANIFEST> could not be opened.

=item C<Added to MANIFEST:> I<file>

is reported by mkmanifest() if $Verbose is set and a file is added
to MANIFEST. $Verbose is set to 1 by default.

=back

=head1 ENVIRONMENT

=over 4

=item B<PERL_MM_MANIFEST_DEBUG>

Turns on debugging

=back

=head1 SEE ALSO

L<ExtUtils::MakeMaker> which has handy targets for most of the functionality.

=head1 AUTHOR

Andreas Koenig <F<andreas.koenig@anima.de>>

=cut
