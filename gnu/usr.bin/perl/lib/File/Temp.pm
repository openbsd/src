package File::Temp;

=head1 NAME

File::Temp - return name and handle of a temporary file safely

=head1 SYNOPSIS

  use File::Temp qw/ tempfile tempdir /; 

  $dir = tempdir( CLEANUP => 1 );
  ($fh, $filename) = tempfile( DIR => $dir );

  ($fh, $filename) = tempfile( $template, DIR => $dir);
  ($fh, $filename) = tempfile( $template, SUFFIX => '.dat');

  $fh = tempfile();

MkTemp family:

  use File::Temp qw/ :mktemp  /;

  ($fh, $file) = mkstemp( "tmpfileXXXXX" );
  ($fh, $file) = mkstemps( "tmpfileXXXXXX", $suffix);

  $tmpdir = mkdtemp( $template );

  $unopened_file = mktemp( $template );

POSIX functions:

  use File::Temp qw/ :POSIX /;

  $file = tmpnam();
  $fh = tmpfile();

  ($fh, $file) = tmpnam();
  ($fh, $file) = tmpfile();


Compatibility functions:

  $unopened_file = File::Temp::tempnam( $dir, $pfx );

=begin later

Objects (NOT YET IMPLEMENTED):

  require File::Temp;

  $fh = new File::Temp($template);
  $fname = $fh->filename;

=end later

=head1 DESCRIPTION

C<File::Temp> can be used to create and open temporary files in a safe way.
The tempfile() function can be used to return the name and the open
filehandle of a temporary file.  The tempdir() function can 
be used to create a temporary directory.

The security aspect of temporary file creation is emphasized such that
a filehandle and filename are returned together.  This helps guarantee that 
a race condition can not occur where the temporary file is created by another process 
between checking for the existence of the file and its
opening.  Additional security levels are provided to check, for 
example, that the sticky bit is set on world writable directories.
See L<"safe_level"> for more information.

For compatibility with popular C library functions, Perl implementations of
the mkstemp() family of functions are provided. These are, mkstemp(),
mkstemps(), mkdtemp() and mktemp().

Additionally, implementations of the standard L<POSIX|POSIX>
tmpnam() and tmpfile() functions are provided if required.

Implementations of mktemp(), tmpnam(), and tempnam() are provided,
but should be used with caution since they return only a filename
that was valid when function was called, so cannot guarantee
that the file will not exist by the time the caller opens the filename.

=cut

use v5.5.670;  # Uses S_IWOTH in Fcntl and auto-viv filehandles, new File::Spec
use strict;
use Carp;
use File::Spec 0.8;
use File::Path qw/ rmtree /;
use Fcntl 1.03 qw/ :DEFAULT S_IWOTH S_IWGRP /;
use Errno qw( EEXIST ENOENT ENOTDIR EINVAL );

our ($VERSION, @EXPORT_OK, %EXPORT_TAGS, $DEBUG);

$DEBUG = 0;

# We are exporting functions

#require Exporter;
#@ISA = qw/Exporter/;
use base qw/Exporter/;

# Export list - to allow fine tuning of export table

@EXPORT_OK = qw{
	      tempfile
	      tempdir
	      tmpnam
	      tmpfile
	      mktemp
	      mkstemp 
	      mkstemps
	      mkdtemp
	      unlink0
		};

# Groups of functions for export

%EXPORT_TAGS = (
		'POSIX' => [qw/ tmpnam tmpfile /],
		'mktemp' => [qw/ mktemp mkstemp mkstemps mkdtemp/],
	       );

# add contents of these tags to @EXPORT
Exporter::export_tags('POSIX','mktemp');

# Version number 

$VERSION = '0.05';

# This is a list of characters that can be used in random filenames

my @CHARS = (qw/ A B C D E F G H I J K L M N O P Q R S T U V W X Y Z
	         a b c d e f g h i j k l m n o p q r s t u v w x y z
	         0 1 2 3 4 5 6 7 8 9 _ 
	     /);

# Maximum number of tries to make a temp file before failing

use constant MAX_TRIES => 10;

# Minimum number of X characters that should be in a template
use constant MINX => 4;

# Default template when no template supplied

use constant TEMPXXX => 'X' x 10;

# Constants for the security level

use constant STANDARD => 0;
use constant MEDIUM   => 1;
use constant HIGH     => 2;

# INTERNAL ROUTINES - not to be used outside of package

# Generic routine for getting a temporary filename
# modelled on OpenBSD _gettemp() in mktemp.c

# The template must contain X's that are to be replaced 
# with the random values

#  Arguments:

#  TEMPLATE   - string containing the XXXXX's that is converted
#           to a random filename and opened if required

# Optionally, a hash can also be supplied containing specific options
#   "open" => if true open the temp file, else just return the name
#             default is 0
#   "mkdir"=> if true, we are creating a temp directory rather than tempfile
#             default is 0
#   "suffixlen" => number of characters at end of PATH to be ignored.
#                  default is 0.
# "open" and "mkdir" can not both be true

# The default options are equivalent to mktemp().

# Returns:
#   filehandle - open file handle (if called with doopen=1, else undef)
#   temp name  - name of the temp file or directory

# For example:
#   ($fh, $name) = _gettemp($template, "open" => 1);

# for the current version, failures are associated with
# a carp to give the reason whilst debugging

sub _gettemp {

  croak 'Usage: ($fh, $name) = _gettemp($template, OPTIONS);'
    unless scalar(@_) >= 1;

  # Default options
  my %options = (
		 "open" => 0,
		 "mkdir" => 0,
		 "suffixlen" => 0,
		);

  # Read the template
  my $template = shift;
  if (ref($template)) {
    carp "File::Temp::_gettemp: template must not be a reference";
    return ();
  }

  # Check that the number of entries on stack are even
  if (scalar(@_) % 2 != 0) {
    carp "File::Temp::_gettemp: Must have even number of options";
    return ();
  }

  # Read the options and merge with defaults
  %options = (%options, @_)  if @_;
  
  # Can not open the file and make a directory in a single call
  if ($options{"open"} && $options{"mkdir"}) {
    carp "File::Temp::_gettemp: doopen and domkdir can not both be true\n";
    return ();
  }

  # Find the start of the end of the  Xs (position of last X)
  # Substr starts from 0
  my $start = length($template) - 1 - $options{"suffixlen"};

  # Check that we have at least MINX x X (eg 'XXXX") at the end of the string
  # (taking suffixlen into account). Any fewer is insecure.

  # Do it using substr - no reason to use a pattern match since
  # we know where we are looking and what we are looking for

  if (substr($template, $start - MINX + 1, MINX) ne 'X' x MINX) {
    carp "File::Temp::_gettemp: The template must contain at least ". MINX ." 'X' characters\n";
    return ();
  }

  # Replace all the X at the end of the substring with a
  # random character or just all the XX at the end of a full string.
  # Do it as an if, since the suffix adjusts which section to replace
  # and suffixlen=0 returns nothing if used in the substr directly
  # and generate a full path from the template

  my $path = _replace_XX($template, $options{"suffixlen"});


  # Split the path into constituent parts - eventually we need to check
  # whether the directory exists
  # We need to know whether we are making a temp directory
  # or a tempfile

  my ($volume, $directories, $file);
  my $parent; # parent directory
  if ($options{"mkdir"}) {
    # There is no filename at the end
    ($volume, $directories, $file) = File::Spec->splitpath( $path, 1);

    # The parent is then $directories without the last directory
    # Split the directory and put it back together again
    my @dirs = File::Spec->splitdir($directories);

    # If @dirs only has one entry that means we are in the current
    # directory
    if ($#dirs == 0) {
      $parent = File::Spec->curdir;
    } else {

      # Put it back together without the last one
      $parent = File::Spec->catdir(@dirs[0..$#dirs-1]);

      # ...and attach the volume (no filename)
      $parent = File::Spec->catpath($volume, $parent, '');

    }

  } else {

    # Get rid of the last filename (use File::Basename for this?)
    ($volume, $directories, $file) = File::Spec->splitpath( $path );

    # Join up without the file part
    $parent = File::Spec->catpath($volume,$directories,'');

    # If $parent is empty replace with curdir
    $parent = File::Spec->curdir
      unless $directories ne '';

  }

  # Check that the parent directories exist 
  # Do this even for the case where we are simply returning a name
  # not a file -- no point returning a name that includes a directory
  # that does not exist or is not writable

  unless (-d $parent && -w _) {
    carp "File::Temp::_gettemp: Parent directory ($parent) is not a directory" 
      . " or is not writable\n";
      return ();
  }

  # Check the stickiness of the directory and chown giveaway if required
  # If the directory is world writable the sticky bit
  # must be set

  if (File::Temp->safe_level == MEDIUM) {
    unless (_is_safe($parent)) {
      carp "File::Temp::_gettemp: Parent directory ($parent) is not safe (sticky bit not set when world writable?)";
      return ();
    }
  } elsif (File::Temp->safe_level == HIGH) {
    unless (_is_verysafe($parent)) {
      carp "File::Temp::_gettemp: Parent directory ($parent) is not safe (sticky bit not set when world writable?)";
      return ();
    }
  }


  # Calculate the flags that we wish to use for the sysopen
  # Some of these are not always available
  my $openflags;
  if ($options{"open"}) {
    # Default set
    $openflags = O_CREAT | O_EXCL | O_RDWR;

    # Add the remainder as available
    $openflags |= Fcntl::O_FOLLOW()
      if eval { Fcntl::O_FOLLOW(); 1 };

    $openflags |= Fcntl::O_BINARY()
      if eval { Fcntl::O_BINARY(); 1 };

    $openflags |= Fcntl::O_LARGEFILE()
      if eval { Fcntl::O_LARGEFILE(); 1 };

    $openflags |= Fcntl::O_EXLOCK()
        if eval { Fcntl::O_EXLOCK(); 1 };

  }
  

  # Now try MAX_TRIES time to open the file
  for (my $i = 0; $i < MAX_TRIES; $i++) {

    # Try to open the file if requested
    if ($options{"open"}) {
      my $fh;

      # Try to make sure this will be marked close-on-exec
      # XXX: Win32 doesn't respect this, nor the proper fcntl,
      #      but may have O_NOINHERIT.  This is not in Fcntl.pm, though.
      local $^F = 2; 

      # Store callers umask
      my $umask = umask();

      # Set a known umask
      umask(066);

      # Attempt to open the file
      if ( sysopen($fh, $path, $openflags, 0600) ) {

	# Reset umask
	umask($umask);
	
	# Opened successfully - return file handle and name
	return ($fh, $path);

      } else {
	# Reset umask
	umask($umask);

	# Error opening file - abort with error
	# if the reason was anything but EEXIST
	unless ($! == EEXIST) {
	  carp "File::Temp: Could not create temp file $path: $!";
	  return ();
	}

	# Loop round for another try
	
      }
    } elsif ($options{"mkdir"}) {

      # Store callers umask
      my $umask = umask();

      # Set a known umask
      umask(066);

      # Open the temp directory
      if (mkdir( $path, 0700)) {
	# created okay
	# Reset umask
	umask($umask);

	return undef, $path;
      } else {

	# Reset umask
	umask($umask);

	# Abort with error if the reason for failure was anything
	# except EEXIST
	unless ($! == EEXIST) {
	  carp "File::Temp: Could not create directory $path: $!";
	  return ();
	}

	# Loop round for another try

      }

    } else {

      # Return true if the file can not be found
      # Directory has been checked previously

      return (undef, $path) unless -e $path;

      # Try again until MAX_TRIES 

    }
    
    # Did not successfully open the tempfile/dir
    # so try again with a different set of random letters
    # No point in trying to increment unless we have only
    # 1 X say and the randomness could come up with the same
    # file MAX_TRIES in a row.

    # Store current attempt - in principal this implies that the
    # 3rd time around the open attempt that the first temp file
    # name could be generated again. Probably should store each
    # attempt and make sure that none are repeated

    my $original = $path;
    my $counter = 0;  # Stop infinite loop
    my $MAX_GUESS = 50;

    do {

      # Generate new name from original template
      $path = _replace_XX($template, $options{"suffixlen"});

      $counter++;

    } until ($path ne $original || $counter > $MAX_GUESS);

    # Check for out of control looping
    if ($counter > $MAX_GUESS) {
      carp "Tried to get a new temp name different to the previous value$MAX_GUESS times.\nSomething wrong with template?? ($template)";
      return ();
    }

  }

  # If we get here, we have run out of tries
  carp "Have exceeded the maximum number of attempts (".MAX_TRIES .
    ") to open temp file/dir";

  return ();

}

# Internal routine to return a random character from the
# character list. Does not do an srand() since rand()
# will do one automatically

# No arguments. Return value is the random character

sub _randchar {

  $CHARS[ int( rand( $#CHARS ) ) ];

}

# Internal routine to replace the XXXX... with random characters
# This has to be done by _gettemp() every time it fails to 
# open a temp file/dir

# Arguments:  $template (the template with XXX), 
#             $ignore   (number of characters at end to ignore)

# Returns:    modified template

sub _replace_XX {

  croak 'Usage: _replace_XX($template, $ignore)'
    unless scalar(@_) == 2;

  my ($path, $ignore) = @_;

  # Do it as an if, since the suffix adjusts which section to replace
  # and suffixlen=0 returns nothing if used in the substr directly
  # Alternatively, could simply set $ignore to length($path)-1
  # Don't want to always use substr when not required though.

  if ($ignore) {
    substr($path, 0, - $ignore) =~ s/X(?=X*\z)/_randchar()/ge;
  } else {
    $path =~ s/X(?=X*\z)/_randchar()/ge;
  }

  return $path;
}

# internal routine to check to see if the directory is safe
# First checks to see if the directory is not owned by the 
# current user or root. Then checks to see if anyone else
# can write to the directory and if so, checks to see if 
# it has the sticky bit set

# Will not work on systems that do not support sticky bit

#Args:  directory path to check
# Returns true if the path is safe and false otherwise.
# Returns undef if can not even run stat() on the path

# This routine based on version written by Tom Christiansen

# Presumably, by the time we actually attempt to create the
# file or directory in this directory, it may not be safe
# anymore... Have to run _is_safe directly after the open.

sub _is_safe {

  my $path = shift;

  # Stat path
  my @info = stat($path);
  return 0 unless scalar(@info);

  # Check to see whether owner is neither superuser (or a system uid) nor me
  # Use the real uid from the $< variable
  # UID is in [4]
  if ( $info[4] > File::Temp->top_system_uid() && $info[4] != $<) {
    carp "Directory owned neither by root nor the current user";
    return 0;
  }

  # check whether group or other can write file
  # use 066 to detect either reading or writing
  # use 022 to check writability
  # Do it with S_IWOTH and S_IWGRP for portability (maybe)
  # mode is in info[2]
  if (($info[2] & S_IWGRP) ||   # Is group writable?
      ($info[2] & S_IWOTH) ) {  # Is world writable?
    return 0 unless -d _;       # Must be a directory
    return 0 unless -k _;       # Must be sticky
  }

  return 1;
}

# Internal routine to check whether a directory is safe
# for temp files. Safer than _is_safe since it checks for 
# the possibility of chown giveaway and if that is a possibility
# checks each directory in the path to see if it is safe (with _is_safe)

# If _PC_CHOWN_RESTRICTED is not set, does the full test of each
# directory anyway.

sub _is_verysafe {

  # Need POSIX - but only want to bother if really necessary due to overhead
  require POSIX;

  my $path = shift;

  # Should Get the value of _PC_CHOWN_RESTRICTED if it is defined
  # and If it is not there do the extensive test

  # Check for chown giveaway
  # Might want to do the directory splitting anyway
  return _is_safe($path) if POSIX::sysconf( &POSIX::_PC_CHOWN_RESTRICTED);

  # Convert path to an absolute directory if required
  unless (File::Spec->file_name_is_absolute($path)) {
    $path = File::Spec->rel2abs($path);
  }

  # Split directory into components - assume no file
  my ($volume, $directories, undef) = File::Spec->splitpath( $path, 1);

  # Slightly less efficient than having a a function in File::Spec
  # to chop off the end of a directory or even a function that
  # can handle ../ in a directory tree
  # Sometimes splitdir() returns a blank at the end
  # so we will probably check the bottom directory twice in some cases
  my @dirs = File::Spec->splitdir($directories);

  # Concatenate one less directory each time around
  foreach my $pos (0.. $#dirs) {
    # Get a directory name
    my $dir = File::Spec->catpath($volume,
				  File::Spec->catdir(@dirs[0.. $#dirs - $pos]),
				  ''
				  );

    print "TESTING DIR $dir\n" if $DEBUG;

    # Check the directory
    return unless _is_safe($dir);

  }

  return 1;
}



# internal routine to determine whether unlink works on this
# platform
# Returns true if we can, false otherwise.
# Simply uses $^O rather than creating a temp file and trying
# to delete it (the whole point of this module)
# Currently we are using rmtree() to do this since
# it can be supplied with a single file and knows about
# different platforms.

# Currently does nothing since File::Path::rmtree can unlink
# on WinNT.

sub _can_unlink {

  
  $^O ne 'MSWin32' ? 1 : 1;

}


=head1 FUNCTIONS

This section describes the recommended interface for generating
temporary files and directories.

=over 4

=item B<tempfile>

This is the basic function to generate temporary files.
The behaviour of the file can be changed using various options:

  ($fh, $filename) = tempfile();

Create a temporary file in  the directory specified for temporary
files, as specified by the tmpdir() function in L<File::Spec>.

  ($fh, $filename) = tempfile($template);

Create a temporary file using the template.  Trailing `X' characters
are replaced with random letters to generate the filename.
At least four `X' characters must be present in the template.

  ($fh, $filename) = tempfile($template, SUFFIX => $suffix)

Same as previously, except that a suffix is added to the template
after the `X' translation.  Useful for ensuring that a temporary
filename has a particular extension when needed by other applications.
But see the WARNING at the end.

  ($fh, $filename) = tempfile($template, DIR => $dir);

Translates the template as before except that a directory name
is specified.

If the template is not specified, a template is always
automatically generated. This temporary file is placed in tmpdir()
(L<File::Spec>) unless a directory is specified explicitly with the 
DIR option.

  $fh = tempfile( $template, DIR => $dir );

If called in scalar context, only the filehandle is returned
and the file will automatically be deleted when closed (see 
the description of tmpfile() elsewhere in this document).
This is the preferred mode of operation, as if you only 
have a filehandle, you can never create a race condition
by fumbling with the filename.

  (undef, $filename) = tempfile($template, OPEN => 0);

This will return the filename based on the template but
will not open this file.  Cannot be used in conjunction with
UNLINK set to true. Default is to always open the file 
to protect from possible race conditions. A warning is issued
if warnings are turned on. Consider using the tmpnam()
and mktemp() functions described elsewhere in this document
if opening the file is not required.

=cut

sub tempfile {

  # Can not check for argument count since we can have any
  # number of args

  # Default options
  my %options = (
		 "DIR"    => undef,  # Directory prefix
		 "SUFFIX" => '',      # Template suffix
		 "UNLINK" => 0,      # Unlink file on exit
		 "OPEN"   => 1,      # Do not open file
		);

  # Check to see whether we have an odd or even number of arguments
  my $template = (scalar(@_) % 2 == 1 ? shift(@_) : undef);

  # Read the options and merge with defaults
  %options = (%options, @_)  if @_;

  # First decision is whether or not to open the file
  if (! $options{"OPEN"}) {

    warn "tempfile(): temporary filename requested but not opened.\nPossibly unsafe, consider using tempfile() with OPEN set to true\n"
      if $^W;

  }

  # Construct the template 

  # Have a choice of trying to work around the mkstemp/mktemp/tmpnam etc
  # functions or simply constructing a template and using _gettemp()
  # explicitly. Go for the latter

  # First generate a template if not defined and prefix the directory
  # If no template must prefix the temp directory
  if (defined $template) {
    if ($options{"DIR"}) {

      $template = File::Spec->catfile($options{"DIR"}, $template);

    }

  } else {

    if ($options{"DIR"}) {

      $template = File::Spec->catfile($options{"DIR"}, TEMPXXX);

    } else {
      
      $template = File::Spec->catfile(File::Spec->tmpdir, TEMPXXX);

    }
    
  }

  # Now add a suffix
  $template .= $options{"SUFFIX"};

  # Create the file
  my ($fh, $path);
  croak "Error in tempfile() using $template"
    unless (($fh, $path) = _gettemp($template,
				    "open" => $options{'OPEN'}, 
				    "mkdir"=> 0 ,
				    "suffixlen" => length($options{'SUFFIX'}),
				   ) );  

  # Setup an exit handler
  # Do not perform the safe unlink0 since the filehandle may be
  # closed and be invalid
  # Return status is not checked.
  END { rmtree($path, $DEBUG, 1) if ($options{"UNLINK"} && -f $path);  };
  
  # Return
  if (wantarray()) {

    if ($options{'OPEN'}) {
      return ($fh, $path);
    } else {
      return (undef, $path);
    }

  } else {
    # Assume that we can unlink the file with unlink0
    # since the caller is not interested in the name
    unlink0($fh, $path);
    
    # Return just the filehandle.
    return $fh;
  }


}

=item B<tempdir>

This is the recommended interface for creation of temporary directories.
The behaviour of the function depends on the arguments:

  $tempdir = tempdir();

Create a directory in tmpdir() (see L<File::Spec|File::Spec>).

  $tempdir = tempdir( $template );

Create a directory from the supplied template. This template is
similar to that described for tempfile(). `X' characters at the end
of the template are replaced with random letters to construct the
directory name. At least four `X' characters must be in the template.

  $tempdir = tempdir ( DIR => $dir );

Specifies the directory to use for the temporary directory.
The temporary directory name is derived from an internal template.

  $tempdir = tempdir ( $template, DIR => $dir );

Prepend the supplied directory name to the template. The template
should not include parent directory specifications itself. Any parent
directory specifications are removed from the template before
prepending the supplied directory.

  $tempdir = tempdir ( $template, TMPDIR => 1 );

Using the supplied template, creat the temporary directory in 
a standard location for temporary files. Equivalent to doing

  $tempdir = tempdir ( $template, DIR => File::Spec->tmpdir);

but shorter. Parent directory specifications are stripped from the
template itself. The C<TMPDIR> option is ignored if C<DIR> is set
explicitly.  Additionally, C<TMPDIR> is implied if neither a template
nor a directory are supplied.

  $tempdir = tempdir( $template, CLEANUP => 1);

Create a temporary directory using the supplied template, but 
attempt to remove it (and all files inside it) when the program
exits. Note that an attempt will be made to remove all files from
the directory even if they were not created by this module (otherwise
why ask to clean it up?). The directory removal is made with
the rmtree() function from the L<File::Path|File::Path> module.
Of course, if the template is not specified, the temporary directory
will be created in tmpdir() and will also be removed at program exit.

=cut

sub tempdir  {

  # Can not check for argument count since we can have any
  # number of args

  # Default options
  my %options = (
		 "CLEANUP"    => 0,  # Remove directory on exit
		 "DIR"        => '', # Root directory
		 "TMPDIR"     => 0,  # Use tempdir with template
		);

  # Check to see whether we have an odd or even number of arguments
  my $template = (scalar(@_) % 2 == 1 ? shift(@_) : undef );

  # Read the options and merge with defaults
  %options = (%options, @_)  if @_;

  # Modify or generate the template

  # Deal with the DIR and TMPDIR options
  if (defined $template) {

    # Need to strip directory path if using DIR or TMPDIR
    if ($options{'TMPDIR'} || $options{'DIR'}) {

      # Strip parent directory from the filename
      # 
      # There is no filename at the end
      my ($volume, $directories, undef) = File::Spec->splitpath( $template, 1);

      # Last directory is then our template
      $template = (File::Spec->splitdir($directories))[-1];

      # Prepend the supplied directory or temp dir
      if ($options{"DIR"}) {

	$template = File::Spec->catfile($options{"DIR"}, $template);

      } elsif ($options{TMPDIR}) {

	# Prepend tmpdir
	$template = File::Spec->catdir(File::Spec->tmpdir, $template);

      }

    }

  } else {

    if ($options{"DIR"}) {

      $template = File::Spec->catdir($options{"DIR"}, TEMPXXX);

    } else {
      
      $template = File::Spec->catdir(File::Spec->tmpdir, TEMPXXX);

    }
    
  }

  # Create the directory
  my $tempdir;
  croak "Error in tempdir() using $template"
    unless ((undef, $tempdir) = _gettemp($template,
				    "open" => 0, 
				    "mkdir"=> 1 ,
				    "suffixlen" => 0,
				   ) );  
  
  # Install exit handler; must be dynamic to get lexical
  if ( $options{'CLEANUP'} && -d $tempdir) { 
      eval q{ END { rmtree($tempdir,$DEBUG, 1) } 1; } || die; 	    
  } 

  # Return the dir name
  return $tempdir;

}

=back

=head1 MKTEMP FUNCTIONS

The following functions are Perl implementations of the 
mktemp() family of temp file generation system calls.

=over 4

=item B<mkstemp>

Given a template, returns a filehandle to the temporary file and the name
of the file.

  ($fh, $name) = mkstemp( $template );

In scalar context, just the filehandle is returned.

The template may be any filename with some number of X's appended
to it, for example F</tmp/temp.XXXX>. The trailing X's are replaced
with unique alphanumeric combinations.

=cut



sub mkstemp {

  croak "Usage: mkstemp(template)"
    if scalar(@_) != 1;

  my $template = shift;

  my ($fh, $path);
  croak "Error in mkstemp using $template"
    unless (($fh, $path) = _gettemp($template, 
				    "open" => 1, 
				    "mkdir"=> 0 ,
				    "suffixlen" => 0,
				   ) );

  if (wantarray()) {
    return ($fh, $path);
  } else {
    return $fh;
  }

}


=item B<mkstemps>

Similar to mkstemp(), except that an extra argument can be supplied
with a suffix to be appended to the template.

  ($fh, $name) = mkstemps( $template, $suffix );

For example a template of C<testXXXXXX> and suffix of C<.dat>
would generate a file similar to F<testhGji_w.dat>.

Returns just the filehandle alone when called in scalar context.

=cut

sub mkstemps {

  croak "Usage: mkstemps(template, suffix)"
    if scalar(@_) != 2;


  my $template = shift;
  my $suffix   = shift;

  $template .= $suffix;
  
  my ($fh, $path);
  croak "Error in mkstemps using $template"
    unless (($fh, $path) = _gettemp($template,
				    "open" => 1, 
				    "mkdir"=> 0 ,
				    "suffixlen" => length($suffix),
				   ) );

  if (wantarray()) {
    return ($fh, $path);
  } else {
    return $fh;
  }

}

=item B<mkdtemp>

Create a directory from a template. The template must end in
X's that are replaced by the routine.

  $tmpdir_name = mkdtemp($template);

Returns the name of the temporary directory created.
Returns undef on failure.

Directory must be removed by the caller.

=cut



sub mkdtemp {

  croak "Usage: mkdtemp(template)"
    if scalar(@_) != 1;
  
  my $template = shift;

  my ($junk, $tmpdir);
  croak "Error creating temp directory from template $template\n"
    unless (($junk, $tmpdir) = _gettemp($template,
					"open" => 0, 
					"mkdir"=> 1 ,
					"suffixlen" => 0,
				       ) );

  return $tmpdir;

}

=item B<mktemp>

Returns a valid temporary filename but does not guarantee
that the file will not be opened by someone else.

  $unopened_file = mktemp($template);

Template is the same as that required by mkstemp().

=cut

sub mktemp {

  croak "Usage: mktemp(template)"
    if scalar(@_) != 1;

  my $template = shift;

  my ($tmpname, $junk);
  croak "Error getting name to temp file from template $template\n"
    unless (($junk, $tmpname) = _gettemp($template,
					 "open" => 0, 
					 "mkdir"=> 0 ,
					 "suffixlen" => 0,
					 ) );

  return $tmpname;
}

=back

=head1 POSIX FUNCTIONS

This section describes the re-implementation of the tmpnam()
and tmpfile() functions described in L<POSIX> 
using the mkstemp() from this module.

Unlike the L<POSIX|POSIX> implementations, the directory used
for the temporary file is not specified in a system include
file (C<P_tmpdir>) but simply depends on the choice of tmpdir()
returned by L<File::Spec|File::Spec>. On some implementations this
location can be set using the C<TMPDIR> environment variable, which
may not be secure.
If this is a problem, simply use mkstemp() and specify a template.

=over 4

=item B<tmpnam>

When called in scalar context, returns the full name (including path)
of a temporary file (uses mktemp()). The only check is that the file does
not already exist, but there's no guarantee that that condition will
continue to apply.

  $file = tmpnam();

When called in list context, a filehandle to the open file and
a filename are returned. This is achieved by calling mkstemp()
after constructing a suitable template.

  ($fh, $file) = tmpnam();

If possible, this form should be used to prevent possible
race conditions.

See L<File::Spec/tmpdir> for information on the choice of temporary
directory for a particular operating system.

=cut

sub tmpnam {

   # Retrieve the temporary directory name
   my $tmpdir = File::Spec->tmpdir;

   croak "Error temporary directory is not writable"
     if $tmpdir eq '';

   # Use a ten character template and append to tmpdir
   my $template = File::Spec->catfile($tmpdir, TEMPXXX);
  
   if (wantarray() ) {
       return mkstemp($template);
   } else {
       return mktemp($template);
   }

}

=item B<tmpfile>

In scalar context, returns the filehandle of a temporary file.

  $fh = tmpfile();

The file is removed when the filehandle is closed or when the program
exits. No access to the filename is provided.

=cut

sub tmpfile {

  # Simply call tmpnam() in an array context
  my ($fh, $file) = tmpnam();

  # Make sure file is removed when filehandle is closed
  unlink0($fh, $file) or croak "Unable to unlink temporary file: $!";

  return $fh;

}

=back

=head1 ADDITIONAL FUNCTIONS

These functions are provided for backwards compatibility
with common tempfile generation C library functions.

They are not exported and must be addressed using the full package
name. 

=over 4

=item B<tempnam>

Return the name of a temporary file in the specified directory
using a prefix. The file is guaranteed not to exist at the time
the function was called, but such guarantees are good for one 
clock tick only.  Always use the proper form of C<sysopen>
with C<O_CREAT | O_EXCL> if you must open such a filename.

  $filename = File::Temp::tempnam( $dir, $prefix );

Equivalent to running mktemp() with $dir/$prefixXXXXXXXX
(using unix file convention as an example) 

Because this function uses mktemp(), it can suffer from race conditions.

=cut

sub tempnam {

  croak 'Usage tempnam($dir, $prefix)' unless scalar(@_) == 2;

  my ($dir, $prefix) = @_;

  # Add a string to the prefix
  $prefix .= 'XXXXXXXX';

  # Concatenate the directory to the file
  my $template = File::Spec->catfile($dir, $prefix);

  return mktemp($template);

}

=back

=head1 UTILITY FUNCTIONS

Useful functions for dealing with the filehandle and filename.

=over 4

=item B<unlink0>

Given an open filehandle and the associated filename, make
a safe unlink. This is achieved by first checking that the filename
and filehandle initially point to the same file and that the number
of links to the file is 1.  Then the filename is unlinked and the
filehandle checked once again to verify that the number of links
on that file is now 0.  This is the closest you can come to making
sure that the filename unlinked was the same as the file whose
descriptor you hold.

  unlink0($fh, $path) or die "Error unlinking file $path safely";

Returns false on error.

=cut

sub unlink0 {

  croak 'Usage: unlink0(filehandle, filename)'
    unless scalar(@_) == 2;

  # Read args
  my ($fh, $path) = @_;

  # Might as well return immediately if we cant unlink
  # But still return success though
  unless (_can_unlink()) {
    warn "File $path can not be unlinked on this platform" if $^W;    
    return 1;
  }

  # Stat the filehandle
  my @fh = stat $fh;

  if ($fh[3] > 1 && $^W) {
    carp "unlink0: fstat found too many links; SB=@fh";
  } 

  # Stat the path
  my @path = stat $path;

  unless (@path) {
    carp "unlink0: $path is gone already" if $^W;
    return;
  } 

  # this is no longer a file, but may be a directory, or worse
  unless (-f _) {
    confess "panic: $path is no longer a file: SB=@fh";
  } 

  # Do comparison of each member of the array
  # On WinNT dev and rdev seem to be different
  # depending on whether it is a file or a handle.
  # Cannot simply compare all members of the stat return
  # Select the ones we can use
  my @okstat = (0..$#fh);  # Use all by default
  if ($^O eq 'MSWin32') {
    @okstat = (1,2,3,4,5,7,8,9,10);
  }

  # Now compare each entry explicitly by number
  for (@okstat) {
    # print "Comparing: $_ : $fh[$_] and $path[$_]\n";
    return 0 unless $fh[$_] == $path[$_];
  }
  
  # remove the file (does not work on some platforms)
  # Test is done earlier anyway!
  if (_can_unlink()) {
#    unlink($path) or return 0;
    # XXX: do *not* call this on a directory; possible race
    #      resulting in recursive removal
    croak "unlink0: $path has become a directory!" if -d $path;
    rmtree ($path, $DEBUG,1) or return 0;
  }

  # Stat the filehandle
  @fh = stat $fh;

  # Make sure that the link count is zero
  return ( $fh[3] == 0 ? 1 : 0);

}

=back

=head1 PACKAGE VARIABLES

These functions control the global state of the package.

=over 4

=item B<safe_level>

Controls the lengths to which the module will go to check the safety of the
temporary file or directory before proceeding.
Options are:

=over 8

=item STANDARD

Do the basic security measures to ensure the directory exists and
is writable, that the umask() is fixed before opening of the file,
that temporary files are opened only if they do not already exist, and
that possible race conditions are avoided.  Finally the L<unlink0|"unlink0">
function is used to remove files safely.

=item MEDIUM

In addition to the STANDARD security, the output directory is checked
to make sure that it is owned either by root or the user running the
program. If the directory is writable by group or by other, it is then
checked to make sure that the sticky bit is set.

Will not work on platforms that do not support the C<-k> test
for sticky bit.

=item HIGH

In addition to the MEDIUM security checks, also check for the
possibility of ``chown() giveaway'' using the L<POSIX|POSIX>
sysconf() function. If this is a possibility, each directory in the
path is checked in turn for safeness, recursively walking back to the 
root directory.

Will not work on platforms that do not support the L<POSIX|POSIX>
C<_PC_CHOWN_RESTRICTED> symbol (for example, Windows NT).

=back

The level can be changed as follows:

  File::Temp->safe_level( File::Temp::HIGH );

The level constants are not exported by the module.

=cut

{
  # protect from using the variable itself
  my $LEVEL = STANDARD;
  sub safe_level {
    my $self = shift;
    if (@_) { 
      my $level = shift;
      if (($level != STANDARD) && ($level != MEDIUM) && ($level != HIGH)) {
	carp "safe_level: Specified level ($level) not STANDARD, MEDIUM or HIGH - ignoring\n";
      } else {
        $LEVEL = $level; 
      }
    }
    return $LEVEL;
  }
}

=item TopSystemUID

This is the highest UID on the current system that refers to a root
UID. This is used to make sure that the temporary directory is 
owned by a system UID (C<root>, C<bin>, C<sys> etc) rather than 
simply by root.

This is required since on many unix systems C</tmp> is not owned
by root.

Default is to assume that any UID less than or equal to 10 is a root
UID.

  File::Temp->top_system_uid(10);
  my $topid = File::Temp->top_system_uid;

This value can be adjusted to reduce security checking if required.
The value is only relevant when C<safe_level> is set to MEDIUM or higher.

=back

=cut

{
  my $TopSystemUID = 10;
  sub top_system_uid {
    my $self = shift;
    if (@_) {
      my $newuid = shift;
      croak "top_system_uid: UIDs should be numeric"
        unless $newuid =~ /^\d+$/s;
      $TopSystemUID = $newuid;
    }
    return $TopSystemUID;
  }
}

=head1 WARNING

For maximum security, endeavour always to avoid ever looking at,
touching, or even imputing the existence of the filename.  You do not
know that that filename is connected to the same file as the handle
you have, and attempts to check this can only trigger more race
conditions.  It's far more secure to use the filehandle alone and
dispense with the filename altogether.

If you need to pass the handle to something that expects a filename
then, on a unix system, use C<"/dev/fd/" . fileno($fh)> for arbitrary
programs, or more generally C<< "+<=&" . fileno($fh) >> for Perl
programs.  You will have to clear the close-on-exec bit on that file
descriptor before passing it to another process.

    use Fcntl qw/F_SETFD F_GETFD/;
    fcntl($tmpfh, F_SETFD, 0)
        or die "Can't clear close-on-exec flag on temp fh: $!\n";

=head1 HISTORY

Originally began life in May 1999 as an XS interface to the system
mkstemp() function. In March 2000, the mkstemp() code was
translated to Perl for total control of the code's
security checking, to ensure the presence of the function regardless of
operating system and to help with portability.

=head1 SEE ALSO

L<POSIX/tmpnam>, L<POSIX/tmpfile>, L<File::Spec>, L<File::Path>

See L<File::MkTemp> for a different implementation of temporary
file handling.

=head1 AUTHOR

Tim Jenness E<lt>t.jenness@jach.hawaii.eduE<gt>

Copyright (C) 1999, 2000 Tim Jenness and the UK Particle Physics and
Astronomy Research Council. All Rights Reserved.  This program is free
software; you can redistribute it and/or modify it under the same
terms as Perl itself.

Original Perl implementation loosely based on the OpenBSD C code for 
mkstemp(). Thanks to Tom Christiansen for suggesting that this module
should be written and providing ideas for code improvements and
security enhancements.

=cut


1;
