#   Perl hooks into the routines in vms.c for interconversion
#   of VMS and Unix file specification syntax.
#
#   Version:  see $VERSION below
#   Author:   Charles Bailey  bailey@newman.upenn.edu
#   Revised:  08-Mar-1995

=head1 NAME

VMS::Filespec - convert between VMS and Unix file specification syntax

=head1 SYNOPSIS

use VMS::Filespec;
$fullspec = rmsexpand('[.VMS]file.specification'[, 'default:[file.spec]']);
$vmsspec = vmsify('/my/Unix/file/specification');
$unixspec = unixify('my:[VMS]file.specification');
$path = pathify('my:[VMS.or.Unix.directory]specification.dir');
$dirfile = fileify('my:[VMS.or.Unix.directory.specification]');
$vmsdir = vmspath('my/VMS/or/Unix/directory/specification.dir');
$unixdir = unixpath('my:[VMS.or.Unix.directory]specification.dir');
candelete('my:[VMS.or.Unix]file.specification');

=head1 DESCRIPTION

This package provides routines to simplify conversion between VMS and
Unix syntax when processing file specifications.  This is useful when
porting scripts designed to run under either OS, and also allows you
to take advantage of conveniences provided by either syntax (I<e.g.>
ability to easily concatenate Unix-style specifications).  In
addition, it provides an additional file test routine, C<candelete>,
which determines whether you have delete access to a file.

If you're running under VMS, the routines in this package are special,
in that they're automatically made available to any Perl script,
whether you're running F<miniperl> or the full F<perl>.  The C<use
VMS::Filespec> or C<require VMS::Filespec; import VMS::Filespec ...>
statement can be used to import the function names into the current
package, but they're always available if you use the fully qualified
name, whether or not you've mentioned the F<.pm> file in your script. 
If you're running under another OS and have installed this package, it
behaves like a normal Perl extension (in fact, you're using Perl
substitutes to emulate the necessary VMS system calls).

Each of these routines accepts a file specification in either VMS or
Unix syntax, and returns the converted file specification, or C<undef>
if an error occurs.  The conversions are, for the most part, simply
string manipulations; the routines do not check the details of syntax
(e.g. that only legal characters are used).  There is one exception:
when running under VMS, conversions from VMS syntax use the $PARSE
service to expand specifications, so illegal syntax, or a relative
directory specification which extends above the tope of the current
directory path (e.g [---.foo] when in dev:[dir.sub]) will cause
errors.  In general, any legal file specification will be converted
properly, but garbage input tends to produce garbage output.  

Each of these routines is prototyped as taking a single scalar
argument, so you can use them as unary operators in complex
expressions (as long as you don't use the C<&> form of
subroutine call, which bypasses prototype checking).


The routines provided are:

=head2 rmsexpand

Uses the RMS $PARSE and $SEARCH services to expand the input
specification to its fully qualified form, except that a null type
or version is not added unless it was present in either the original
file specification or the default specification passed to C<rmsexpand>.
(If the file does not exist, the input specification is expanded as much
as possible.)  If an error occurs, returns C<undef> and sets C<$!>
and C<$^E>.

=head2 vmsify

Converts a file specification to VMS syntax.

=head2 unixify

Converts a file specification to Unix syntax.

=head2 pathify

Converts a directory specification to a path - that is, a string you
can prepend to a file name to form a valid file specification.  If the
input file specification uses VMS syntax, the returned path does, too;
likewise for Unix syntax (Unix paths are guaranteed to end with '/').
Note that this routine will insist that the input be a legal directory
file specification; the file type and version, if specified, must be
F<.DIR;1>.  For compatibility with Unix usage, the type and version
may also be omitted.

=head2 fileify

Converts a directory specification to the file specification of the
directory file - that is, a string you can pass to functions like
C<stat> or C<rmdir> to manipulate the directory file.  If the
input directory specification uses VMS syntax, the returned file
specification does, too; likewise for Unix syntax.  As with
C<pathify>, the input file specification must have a type and
version of F<.DIR;1>, or the type and version must be omitted.

=head2 vmspath

Acts like C<pathify>, but insures the returned path uses VMS syntax.

=head2 unixpath

Acts like C<pathify>, but insures the returned path uses Unix syntax.

=head2 candelete

Determines whether you have delete access to a file.  If you do, C<candelete>
returns true.  If you don't, or its argument isn't a legal file specification,
C<candelete> returns FALSE.  Unlike other file tests, the argument to
C<candelete> must be a file name (not a FileHandle), and, since it's an XSUB,
it's a list operator, so you need to be careful about parentheses.  Both of
these restrictions may be removed in the future if the functionality of
C<candelete> becomes part of the Perl core.

=head1 REVISION

This document was last revised 22-Feb-1996, for Perl 5.002.

=cut

package VMS::Filespec;
require 5.002;

our $VERSION = '1.1';

# If you want to use this package on a non-VMS system,
# uncomment the following line.
# use AutoLoader;
require Exporter;

@ISA = qw( Exporter );
@EXPORT = qw( &vmsify &unixify &pathify &fileify
              &vmspath &unixpath &candelete &rmsexpand );

1;


__END__


# The autosplit routines here are provided for use by non-VMS systems
# They are not guaranteed to function identically to the XSUBs of the
# same name, since they do not have access to the RMS system routine
# sys$parse() (in particular, no real provision is made for handling
# of complex DECnet node specifications).  However, these routines
# should be adequate for most purposes.

# A sort-of sys$parse() replacement
sub rmsexpand ($;$) {
  my($fspec,$defaults) = @_;
  if (!$fspec) { return undef }
  my($node,$dev,$dir,$name,$type,$ver,$dnode,$ddev,$ddir,$dname,$dtype,$dver);

  $fspec =~ s/:$//;
  $defaults = [] unless $defaults;
  $defaults = [ $defaults ] unless ref($defaults) && ref($defaults) eq 'ARRAY';

  while ($fspec !~ m#[:>\]]# && $ENV{$fspec}) { $fspec = $ENV{$fspec} }

  if ($fspec =~ /:/) {
    my($dev,$devtrn,$base);
    ($dev,$base) = split(/:/,$fspec);
    $devtrn = $dev;
    while ($devtrn = $ENV{$devtrn}) {
      if ($devtrn =~ /(.)([:>\]])$/) {
        $dev .= ':', last if $1 eq '.';
        $dev = $devtrn, last;
      }
    }
    $fspec = $dev . $base;
  }

  ($node,$dev,$dir,$name,$type,$ver) = $fspec =~
     /([^:]*::)?([^:]*:)?([^>\]]*[>\]])?([^.;]*)(\.?[^.;]*)([.;]?\d*)/;
  foreach ((@$defaults,$ENV{'DEFAULT'})) {
    last if $node && $ver && $type && $dev && $dir && $name;
    ($dnode,$ddev,$ddir,$dname,$dtype,$dver) =
       /([^:]*::)?([^:]*:)?([^>\]]*[>\]])?([^.;]*)(\.?[^.;]*)([.;]?\d*)/;
    $node = $dnode if $dnode && !$node;
    $dev = $ddev if $ddev && !$dev;
    $dir = $ddir if $ddir && !$dir;
    $name = $dname if $dname && !$name;
    $type = $dtype if $dtype && !$type;
    $ver = $dver if $dver && !$ver;
  }
  # do this the long way to keep -w happy
  $fspec = '';
  $fspec .= $node if $node;
  $fspec .= $dev if $dev;
  $fspec .= $dir if $dir;
  $fspec .= $name if $name;
  $fspec .= $type if $type;
  $fspec .= $ver if $ver;
  $fspec;
}  

sub vmsify ($) {
  my($fspec) = @_;
  my($hasdev,$dev,$defdirs,$dir,$base,@dirs,@realdirs);

  if ($fspec =~ m#^\.(\.?)/?$#) { return $1 ? '[-]' : '[]'; }
  return $fspec if $fspec !~ m#/#;
  ($hasdev,$dir,$base) = $fspec =~ m#(/?)(.*)/(.*)#;
  @dirs = split(m#/#,$dir);
  if ($base eq '.') { $base = ''; }
  elsif ($base eq '..') {
    push @dirs,$base;
    $base = '';
  }
  foreach (@dirs) {
    next unless $_;  # protect against // in input
    next if $_ eq '.';
    if ($_ eq '..') {
      if (@realdirs && $realdirs[$#realdirs] ne '-') { pop @realdirs }
      else                                           { push @realdirs, '-' }
    }
    else { push @realdirs, $_; }
  }
  if ($hasdev) {
    $dev = shift @realdirs;
    @realdirs = ('000000') unless @realdirs;
    $base = '' unless $base;  # keep -w happy
    $dev . ':[' . join('.',@realdirs) . "]$base";
  }
  else {
    '[' . join('',map($_ eq '-' ? $_ : ".$_",@realdirs)) . "]$base";
  }
}

sub unixify ($) {
  my($fspec) = @_;

  return $fspec if $fspec !~ m#[:>\]]#;
  return '.' if ($fspec eq '[]' || $fspec eq '<>');
  if ($fspec =~ m#^[<\[](\.|-+)(.*)# ) {
    $fspec = ($1 eq '.' ? '' : "$1.") . $2;
    my($dir,$base) = split(/[\]>]/,$fspec);
    my(@dirs) = grep($_,split(m#\.#,$dir));
    if ($dirs[0] =~ /^-/) {
      my($steps) = shift @dirs;
      for (1..length($steps)) { unshift @dirs, '..'; }
    }
    join('/',@dirs) . "/$base";
  }
  else {
    $fspec = rmsexpand($fspec,'_N_O_T_:[_R_E_A_L_]');
    $fspec =~ s/.*_N_O_T_:(?:\[_R_E_A_L_\])?//;
    my($dev,$dir,$base) = $fspec =~ m#([^:<\[]*):?[<\[](.*)[>\]](.*)#;
    my(@dirs) = split(m#\.#,$dir);
    if ($dirs[0] && $dirs[0] =~ /^-/) {
      my($steps) = shift @dirs;
      for (1..length($steps)) { unshift @dirs, '..'; }
    }
    "/$dev/" . join('/',@dirs) . "/$base";
  }
}


sub fileify ($) {
  my($path) = @_;

  if (!$path) { return undef }
  if ($path eq '/') { return 'sys$disk:[000000]'; }
  if ($path =~ /(.+)\.([^:>\]]*)$/) {
    $path = $1;
    if ($2 !~ /^dir(?:;1)?$/i) { return undef }
  }

  if ($path !~ m#[/>\]]#) {
    $path =~ s/:$//;
    while ($ENV{$path}) {
      ($path = $ENV{$path}) =~ s/:$//;
      last if $path =~ m#[/>\]]#;
    }
  }
  if ($path =~ m#[>\]]#) {
    my($dir,$sep,$base) = $path =~ /(.*)([>\]])(.*)/;
    $sep =~ tr/<[/>]/;
    if ($base) {
      "$dir$sep$base.dir;1";
    }
    else {
      if ($dir !~ /\./) { $dir =~ s/([<\[])/${1}000000./; }
      $dir =~ s#\.(\w+)$#$sep$1#;
      $dir =~ s/^.$sep//;
      "$dir.dir;1";
    }
  }
  else {
    $path =~ s#/$##;
    "$path.dir;1";
  }
}

sub pathify ($) {
  my($fspec) = @_;

  if (!$fspec) { return undef }
  if ($fspec =~ m#[/>\]]$#) { return $fspec; }
  if ($fspec =~ m#(.+)\.([^/>\]]*)$# && $2 && $2 ne '.') {
    $fspec = $1;
    if ($2 !~ /^dir(?:;1)?$/i) { return undef }
  }

  if ($fspec !~ m#[/>\]]#) {
    $fspec =~ s/:$//;
    while ($ENV{$fspec}) {
      if ($ENV{$fspec} =~ m#[>\]]$#) { return $ENV{$fspec} }
      else { $fspec = $ENV{$fspec} =~ s/:$// }
    }
  }
  
  if ($fspec !~ m#[>\]]#) { "$fspec/"; }
  else {
    if ($fspec =~ /([^>\]]+)([>\]])(.+)/) { "$1.$3$2"; }
    else { $fspec; }
  }
}

sub vmspath ($) {
  pathify(vmsify($_[0]));
}

sub unixpath ($) {
  pathify(unixify($_[0]));
}

sub candelete ($) {
  my($fspec) = @_;
  my($parent);

  return '' unless -w $fspec;
  $fspec =~ s#/$##;
  if ($fspec =~ m#/#) {
    ($parent = $fspec) =~ s#/[^/]+$#;
    return (-w $parent);
  }
  elsif ($parent = fileify($fspec)) { # fileify() here to expand lnms
    $parent =~ s/[>\]][^>\]]+//;
    return (-w fileify($parent));
  }
  else { return (-w '[-]'); }
}
