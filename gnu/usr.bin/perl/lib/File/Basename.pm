package File::Basename;

=head1 NAME

Basename - parse file specifications

fileparse - split a pathname into pieces

basename - extract just the filename from a path

dirname - extract just the directory from a path

=head1 SYNOPSIS

    use File::Basename;

    ($name,$path,$suffix) = fileparse($fullname,@suffixlist)
    fileparse_set_fstype($os_string);
    $basename = basename($fullname,@suffixlist);
    $dirname = dirname($fullname);

    ($name,$path,$suffix) = fileparse("lib/File/Basename.pm","\.pm");
    fileparse_set_fstype("VMS");
    $basename = basename("lib/File/Basename.pm",".pm");
    $dirname = dirname("lib/File/Basename.pm");

=head1 DESCRIPTION

These routines allow you to parse file specifications into useful
pieces using the syntax of different operating systems.

=over 4

=item fileparse_set_fstype

You select the syntax via the routine fileparse_set_fstype().
If the argument passed to it contains one of the substrings
"VMS", "MSDOS", or "MacOS", the file specification syntax of that
operating system is used in future calls to fileparse(),
basename(), and dirname().  If it contains none of these
substrings, UNIX syntax is used.  This pattern matching is
case-insensitive.  If you've selected VMS syntax, and the file
specification you pass to one of these routines contains a "/",
they assume you are using UNIX emulation and apply the UNIX syntax
rules instead, for that function call only.

If you haven't called fileparse_set_fstype(), the syntax is chosen
by examining the builtin variable C<$^O> according to these rules.

=item fileparse

The fileparse() routine divides a file specification into three
parts: a leading B<path>, a file B<name>, and a B<suffix>.  The
B<path> contains everything up to and including the last directory
separator in the input file specification.  The remainder of the input
file specification is then divided into B<name> and B<suffix> based on
the optional patterns you specify in C<@suffixlist>.  Each element of
this list is interpreted as a regular expression, and is matched
against the end of B<name>.  If this succeeds, the matching portion of
B<name> is removed and prepended to B<suffix>.  By proper use of
C<@suffixlist>, you can remove file types or versions for examination.

You are guaranteed that if you concatenate B<path>, B<name>, and
B<suffix> together in that order, the result will be identical to the
input file specification.

=back

=head1 EXAMPLES

Using UNIX file syntax:

    ($base,$path,$type) = fileparse('/virgil/aeneid/draft.book7', 
				    '\.book\d+');

would yield

    $base eq 'draft'
    $path eq '/virgil/aeneid',
    $tail eq '.book7'

Similarly, using VMS syntax:

    ($name,$dir,$type) = fileparse('Doc_Root:[Help]Rhetoric.Rnh',
				   '\..*');

would yield

    $name eq 'Rhetoric'
    $dir  eq 'Doc_Root:[Help]'
    $type eq '.Rnh'

=item C<basename>

The basename() routine returns the first element of the list produced
by calling fileparse() with the same arguments.  It is provided for
compatibility with the UNIX shell command basename(1).

=item C<dirname>

The dirname() routine returns the directory portion of the input file
specification.  When using VMS or MacOS syntax, this is identical to the
second element of the list produced by calling fileparse() with the same
input file specification.  When using UNIX or MSDOS syntax, the return
value conforms to the behavior of the UNIX shell command dirname(1).  This
is usually the same as the behavior of fileparse(), but differs in some
cases.  For example, for the input file specification F<lib/>, fileparse()
considers the directory name to be F<lib/>, while dirname() considers the
directory name to be F<.>).

=cut

require 5.002;
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(fileparse fileparse_set_fstype basename dirname);

#   fileparse_set_fstype() - specify OS-based rules used in future
#                            calls to routines in this package
#
#   Currently recognized values: VMS, MSDOS, MacOS
#       Any other name uses Unix-style rules

sub fileparse_set_fstype {
  my($old) = $Fileparse_fstype;
  $Fileparse_fstype = $_[0] if $_[0];
  $old;
}

#   fileparse() - parse file specification
#
#   calling sequence:
#     ($filename,$prefix,$tail) = &basename_pat($filespec,@excludelist);
#     where  $filespec    is the file specification to be parsed, and
#            @excludelist is a list of patterns which should be removed
#                         from the end of $filename.
#            $filename    is the part of $filespec after $prefix (i.e. the
#                         name of the file).  The elements of @excludelist
#                         are compared to $filename, and if an  
#            $prefix     is the path portion $filespec, up to and including
#                        the end of the last directory name
#            $tail        any characters removed from $filename because they
#                         matched an element of @excludelist.
#
#   fileparse() first removes the directory specification from $filespec,
#   according to the syntax of the OS (code is provided below to handle
#   VMS, Unix, MSDOS and MacOS; you can pick the one you want using
#   fileparse_set_fstype(), or you can accept the default, which is
#   based on the information in the builtin variable $^O).  It then compares
#   each element of @excludelist to $filename, and if that element is a
#   suffix of $filename, it is removed from $filename and prepended to
#   $tail.  By specifying the elements of @excludelist in the right order,
#   you can 'nibble back' $filename to extract the portion of interest
#   to you.
#
#   For example, on a system running Unix,
#   ($base,$path,$type) = fileparse('/virgil/aeneid/draft.book7',
#                                       '\.book\d+');
#   would yield $base == 'draft',
#               $path == '/virgil/aeneid/'  (note trailing slash)
#               $tail == '.book7'.
#   Similarly, on a system running VMS,
#   ($name,$dir,$type) = fileparse('Doc_Root:[Help]Rhetoric.Rnh','\..*');
#   would yield $name == 'Rhetoric';
#               $dir == 'Doc_Root:[Help]', and
#               $type == '.Rnh'.
#
#   Version 2.2  13-Oct-1994  Charles Bailey  bailey@genetics.upenn.edu 


sub fileparse {
  my($fullname,@suffices) = @_;
  my($fstype) = $Fileparse_fstype;
  my($dirpath,$tail,$suffix);

  if ($fstype =~ /^VMS/i) {
    if ($fullname =~ m#/#) { $fstype = '' }  # We're doing Unix emulation
    else {
      ($dirpath,$basename) = ($fullname =~ /(.*[:>\]])?(.*)/);
      $dirpath = $ENV{'DEFAULT'} unless $dirpath;
    }
  }
  if ($fstype =~ /^MSDOS/i) {
    ($dirpath,$basename) = ($fullname =~ /(.*\\)?(.*)/);
    $dirpath = '.\\' unless $dirpath;
  }
  elsif ($fstype =~ /^MAC/i) {
    ($dirpath,$basename) = ($fullname =~ /(.*:)?(.*)/);
  }
  elsif ($fstype !~ /^VMS/i) {  # default to Unix
    ($dirpath,$basename) = ($fullname =~ m#(.*/)?(.*)#);
    $dirpath = './' unless $dirpath;
  }

  if (@suffices) {
    $tail = '';
    foreach $suffix (@suffices) {
      if ($basename =~ /($suffix)$/) {
        $tail = $1 . $tail;
        $basename = $`;
      }
    }
  }

  wantarray ? ($basename,$dirpath,$tail) : $basename;

}


#   basename() - returns first element of list returned by fileparse()

sub basename {
  my($name) = shift;
  (fileparse($name, map("\Q$_\E",@_)))[0];
}
  

#    dirname() - returns device and directory portion of file specification
#        Behavior matches that of Unix dirname(1) exactly for Unix and MSDOS
#        filespecs except for names ending with a separator, e.g., "/xx/yy/".
#        This differs from the second element of the list returned
#        by fileparse() in that the trailing '/' (Unix) or '\' (MSDOS) (and
#        the last directory name if the filespec ends in a '/' or '\'), is lost.

sub dirname {
    my($basename,$dirname) = fileparse($_[0]);
    my($fstype) = $Fileparse_fstype;

    if ($fstype =~ /VMS/i) { 
        if ($_[0] =~ m#/#) { $fstype = '' }
        else { return $dirname }
    }
    if ($fstype =~ /MacOS/i) { return $dirname }
    elsif ($fstype =~ /MSDOS/i) { 
        if ( $dirname =~ /:\\$/) { return $dirname }
        chop $dirname;
        $dirname =~ s:[^\\]+$:: unless $basename;
        $dirname = '.' unless $dirname;
    }
    else { 
        if ( $dirname eq '/') { return $dirname }
        chop $dirname;
        $dirname =~ s:[^/]+$:: unless $basename;
        $dirname = '.' unless $dirname;
    }

    $dirname;
}

$Fileparse_fstype = $^O;

1;
