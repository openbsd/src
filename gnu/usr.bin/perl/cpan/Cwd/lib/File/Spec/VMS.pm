package File::Spec::VMS;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.30';
$VERSION = eval $VERSION;

@ISA = qw(File::Spec::Unix);

use File::Basename;
use VMS::Filespec;

=head1 NAME

File::Spec::VMS - methods for VMS file specs

=head1 SYNOPSIS

 require File::Spec::VMS; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

The default behavior is to allow either VMS or Unix syntax on input and to 
return VMS syntax on output, even when Unix syntax was given on input.

When used with a Perl of version 5.10 or greater and a CRTL possessing the
relevant capabilities, override behavior depends on the CRTL features
C<DECC$FILENAME_UNIX_REPORT> and C<DECC$EFS_CHARSET>.  When the
C<DECC$EFS_CHARSET> feature is enabled and the input parameters are clearly
in Unix syntax, the output will be in Unix syntax.  If
C<DECC$FILENAME_UNIX_REPORT> is enabled and the output syntax cannot be
determined from the input syntax, the output will be in Unix syntax.

=over 4

=cut

# Need to look up the feature settings.  The preferred way is to use the
# VMS::Feature module, but that may not be available to dual life modules.

my $use_feature;
BEGIN {
    if (eval { local $SIG{__DIE__}; require VMS::Feature; }) {
        $use_feature = 1;
    }
}

# Need to look up the UNIX report mode.  This may become a dynamic mode
# in the future.
sub _unix_rpt {
    my $unix_rpt;
    if ($use_feature) {
        $unix_rpt = VMS::Feature::current("filename_unix_report");
    } else {
        my $env_unix_rpt = $ENV{'DECC$FILENAME_UNIX_REPORT'} || '';
        $unix_rpt = $env_unix_rpt =~ /^[ET1]/i; 
    }
    return $unix_rpt;
}

# Need to look up the EFS character set mode.  This may become a dynamic
# mode in the future.
sub _efs {
    my $efs;
    if ($use_feature) {
        $efs = VMS::Feature::current("efs_charset");
    } else {
        my $env_efs = $ENV{'DECC$EFS_CHARSET'} || '';
        $efs = $env_efs =~ /^[ET1]/i; 
    }
    return $efs;
}

=item canonpath (override)

Removes redundant portions of file specifications according to the syntax
detected.

=cut


sub canonpath {
    my($self,$path) = @_;

    return undef unless defined $path;

    my $efs = $self->_efs;

    if ($path =~ m|/|) { # Fake Unix
      my $pathify = $path =~ m|/\Z(?!\n)|;
      $path = $self->SUPER::canonpath($path);

      # Do not convert to VMS when EFS character sets are in use
      return $path if $efs;

      if ($pathify) { return vmspath($path); }
      else          { return vmsify($path);  }
    }
    else {

#FIXME - efs parsing has different rules.  Characters in a VMS filespec
#        are only delimiters if not preceded by '^';

	$path =~ tr/<>/[]/;			# < and >       ==> [ and ]
	$path =~ s/\]\[\./\.\]\[/g;		# ][.		==> .][
	$path =~ s/\[000000\.\]\[/\[/g;		# [000000.][	==> [
	$path =~ s/\[000000\./\[/g;		# [000000.	==> [
	$path =~ s/\.\]\[000000\]/\]/g;		# .][000000]	==> ]
	$path =~ s/\.\]\[/\./g;			# foo.][bar     ==> foo.bar
	1 while ($path =~ s/([\[\.])(-+)\.(-+)([\.\]])/$1$2$3$4/);
						# That loop does the following
						# with any amount of dashes:
						# .-.-.		==> .--.
						# [-.-.		==> [--.
						# .-.-]		==> .--]
						# [-.-]		==> [--]
	1 while ($path =~ s/([\[\.])[^\]\.]+\.-(-+)([\]\.])/$1$2$3/);
						# That loop does the following
						# with any amount (minimum 2)
						# of dashes:
						# .foo.--.	==> .-.
						# .foo.--]	==> .-]
						# [foo.--.	==> [-.
						# [foo.--]	==> [-]
						#
						# And then, the remaining cases
	$path =~ s/\[\.-/[-/;			# [.-		==> [-
	$path =~ s/\.[^\]\.]+\.-\./\./g;	# .foo.-.	==> .
	$path =~ s/\[[^\]\.]+\.-\./\[/g;	# [foo.-.	==> [
	$path =~ s/\.[^\]\.]+\.-\]/\]/g;	# .foo.-]	==> ]
	$path =~ s/\[[^\]\.]+\.-\]/\[000000\]/g;# [foo.-]       ==> [000000]
	$path =~ s/\[\]// unless $path eq '[]';	# []		==>
	return $path;
    }
}

=item catdir (override)

Concatenates a list of file specifications, and returns the result as a
directory specification.  No check is made for "impossible"
cases (e.g. elements other than the first being absolute filespecs).

=cut

sub catdir {
    my $self = shift;
    my $dir = pop;

    my $efs = $self->_efs;
    my $unix_rpt = $self->_unix_rpt;


    my @dirs = grep {defined() && length()} @_;
    if ($efs) {
        # Legacy mode removes blank entries.
        # But that breaks existing generic perl code that
        # uses a blank path at the beginning of the array
        # to indicate an absolute path.
        # So put it back if found.
        if (@_) {
            if ($_[0] eq '') {
                unshift @dirs, '';
            }
        }
    }

    my $rslt;
    if (@dirs) {
	my $path = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
	my ($spath,$sdir) = ($path,$dir);

        if ($efs) {
            # Extended character set in use, go into DWIM mode.

            # Now we need to identify what the directory is in
            # of the specification in order to merge them.
            my $path_unix = 0;
            $path_unix = 1 if ($path =~ m#/#);
            $path_unix = 1 if ($path =~ /^\.\.?$/);
            my $path_vms = 0;
            $path_vms = 1 if ($path =~ m#(?<!\^)[\[<\]:]#);
            $path_vms = 1 if ($path =~ /^--?$/);
            my $dir_unix = 0;
            $dir_unix = 1 if ($dir =~ m#/#);
            $dir_unix = 1 if ($dir =~ /^\.\.?$/);
            my $dir_vms = 0;
            $dir_vms = 1 if ($dir =~ m#(?<!\^)[\[<\]:]#);
            $dir_vms = 1 if ($dir =~ /^--?$/);

            my $unix_mode = 0;
            if (($path_unix != $dir_unix) && ($path_vms != $dir_vms)) {
                # Ambiguous, so if in $unix_rpt mode then assume UNIX.
                $unix_mode = 1 if $unix_rpt;
            } else {
                $unix_mode = 1 if (!$path_vms && !$dir_vms && $unix_rpt);
                $unix_mode = 1 if ($path_unix || $dir_unix);
            }

            if ($unix_mode) {

                # Fix up mixed syntax imput as good as possible - GIGO
                $path = unixify($path) if $path_vms;
                $dir = unixify($dir) if $dir_vms;

                $rslt = $path;
                # Append a path delimiter
                $rslt .= '/' unless ($rslt =~ m#/$#);

                $rslt .= $dir;
                return $self->SUPER::canonpath($rslt);
            } else {

                #with <> posible instead of [.
                # Normalize the brackets
                # Fixme - need to not switch when preceded by ^.
                $path =~ s/</\[/g;
                $path =~ s/>/\]/g;
                $dir =~ s/</\[/g;
                $dir =~ s/>/\]/g;

                # Fix up mixed syntax imput as good as possible - GIGO
                $path = vmsify($path) if $path_unix;
                $dir = vmsify($dir) if $dir_unix;

                #Possible path values: foo: [.foo] [foo] foo, and $(foo)
                #or starting with '-', or foo.dir
                #If path is foo, it needs to be converted to [.foo]

                # Fix up a bare path name.
                unless ($path_vms) {
                    $path =~ s/\.dir\Z(?!\n)//i;
                    if (($path ne '') && ($path !~ /^-/)) {
                        # Non blank and not prefixed with '-', add a dot
                        $path = '[.' . $path;
                    } else {
                        # Just start a directory.
                        $path = '[' . $path;
                    }
                } else {
                    $path =~ s/\]$//;
                }

                #Possible dir values: [.dir] dir and $(foo)

                # No punctuation may have a trailing .dir
                unless ($dir_vms) {
                    $dir =~ s/\.dir\Z(?!\n)//i;
                } else {

                    #strip off the brackets
                    $dir =~ s/^\[//;
                    $dir =~ s/\]$//;
                }

                #strip off the leading dot if present.
                $dir =~ s/^\.//;

                # Now put the specifications together.
                if ($dir ne '') {
                    # Add a separator unless this is an absolute path
                    $path .= '.' if ($path ne '[');
                    $rslt = $path . $dir . ']';
                } else {
                    $rslt = $path . ']';
                }
            }

	} else {
	    # Traditional ODS-2 mode.
	    $spath =~ s/\.dir\Z(?!\n)//i; $sdir =~ s/\.dir\Z(?!\n)//i; 

	    $sdir = $self->eliminate_macros($sdir)
		unless $sdir =~ /^[\w\-]+\Z(?!\n)/s;
	    $rslt = $self->fixpath($self->eliminate_macros($spath)."/$sdir",1);

	    # Special case for VMS absolute directory specs: these will have
	    # had device prepended during trip through Unix syntax in
	    # eliminate_macros(), since Unix syntax has no way to express
	    # "absolute from the top of this device's directory tree".
	    if ($spath =~ /^[\[<][^.\-]/s) { $rslt =~ s/^[^\[<]+//s; }
	} 
    } else {
	# Single directory, just make sure it is in directory format
	# Return an empty string on null input, and pass through macros.

	if    (not defined $dir or not length $dir) { $rslt = ''; }
	elsif ($dir =~ /^\$\([^\)]+\)\Z(?!\n)/s) { 
	    $rslt = $dir;
	} else {
            my $unix_mode = 0;

            if ($efs) {
                my $dir_unix = 0;
                $dir_unix = 1 if ($dir =~ m#/#);
                $dir_unix = 1 if ($dir =~ /^\.\.?$/);
                my $dir_vms = 0;
                $dir_vms = 1 if ($dir =~ m#(?<!\^)[\[<\]:]#);
                $dir_vms = 1 if ($dir =~ /^--?$/);

                if ($dir_vms == $dir_unix) {
                    # Ambiguous, so if in $unix_rpt mode then assume UNIX.
                    $unix_mode = 1 if $unix_rpt;
                } else {
                    $unix_mode = 1 if $dir_unix;
                }
            }

            if ($unix_mode) {
                return $dir;
            } else {
                # For VMS, force it to be in directory format
	 	$rslt = vmspath($dir);
	    }
	}
    }
    return $self->canonpath($rslt);
}

=item catfile (override)

Concatenates a list of directory specifications with a filename specification
to build a path.

=cut

sub catfile {
    my $self = shift;
    my $tfile = pop();
    my $file = $self->canonpath($tfile);
    my @files = grep {defined() && length()} @_;

    my $efs = $self->_efs;
    my $unix_rpt = $self->_unix_rpt;

    # Assume VMS mode
    my $unix_mode = 0;
    my $file_unix = 0;
    my $file_vms = 0;
    if ($efs) {

        # Now we need to identify format the file is in
        # of the specification in order to merge them.
        $file_unix = 1 if ($tfile =~ m#/#);
        $file_unix = 1 if ($tfile =~ /^\.\.?$/);
        $file_vms = 1 if ($tfile =~ m#(?<!\^)[\[<\]:]#);
        $file_vms = 1 if ($tfile =~ /^--?$/);

        # We may know for sure what the format is.
        if (($file_unix != $file_vms)) {
            $unix_mode = 1 if ($file_unix && $unix_rpt);
        }
    }

    my $rslt;
    if (@files) {
	# concatenate the directories.
	my $path;
        if (@files == 1) {
           $path = $files[0];
        } else {
            if ($file_vms) {
                # We need to make sure this is in VMS mode to avoid doing
                # both a vmsify and unixfy on the same path, as that may
                # lose significant data.
                my $i = @files - 1;
                my $tdir = $files[$i];
                my $tdir_vms = 0;
                my $tdir_unix = 0;
                $tdir_vms = 1 if ($tdir =~ m#(?<!\^)[\[<\]:]#);
                $tdir_unix = 1 if ($tdir =~ m#/#);
                $tdir_unix = 1 if ($tdir =~ /^\.\.?$/);

                if (!$tdir_vms) {
                    if ($tdir_unix) { 
                        $tdir = vmspath($tdir);
                    } else {
                        $tdir =~ s/\.dir\Z(?!\n)//i;
                        $tdir = '[.' . $tdir . ']';
                    }
                    $files[$i] = $tdir;
                }
            }
            $path = $self->catdir(@files);
        }
	my $spath = $path;

        # Some thing building a VMS path in pieces may try to pass a
        # directory name in filename format, so normalize it.
	$spath =~ s/\.dir\Z(?!\n)//i;

        # if the spath ends with a directory delimiter and the file is bare,
        # then just concat them.
	if ($spath =~ /^(?<!\^)[^\)\]\/:>]+\)\Z(?!\n)/s && basename($file) eq $file) {
	    $rslt = "$spath$file";
	} else {
            if ($efs) {

                # Now we need to identify what the directory is in
                # of the specification in order to merge them.
                my $spath_unix = 0;
                $spath_unix = 1 if ($spath =~ m#/#);
                $spath_unix = 1 if ($spath =~ /^\.\.?$/);
                my $spath_vms = 0;
                $spath_vms = 1 if ($spath =~ m#(?<!\^)[\[<\]:]#);
                $spath_vms = 1 if ($spath =~ /^--?$/);

                # Assume VMS mode
                if (($spath_unix == $spath_vms) &&
                    ($file_unix == $file_vms)) {
                     # Ambigous, so if in $unix_rpt mode then assume UNIX.
                     $unix_mode = 1 if $unix_rpt;
                } else {
                     $unix_mode = 1
                         if (($spath_unix || $file_unix) && $unix_rpt);
                }

                if (!$unix_mode) {
                    if ($spath_vms) {
                        $spath = '[' . $spath . ']' if $spath =~ /^-/;
                        $rslt = vmspath($spath);
                    } else {
                        $rslt = '[.' . $spath . ']';
                    }
                    $file = vmsify($file) if ($file_unix);
                } else {
                    $spath = unixify($spath) if ($spath_vms);
                    $rslt = $spath;
                    $file = unixify($file) if ($file_vms);

                    # Unix merge may need a directory delimitor.
                    # A null path indicates root on Unix.
                    $rslt .= '/' unless ($rslt =~ m#/$#);
                }

                $rslt .= $file;
                $rslt =~ s/\]\[//;

	    } else {
		# Traditional VMS Perl mode expects that this is done.
		# Note for future maintainers:
		# This is left here for compatibility with perl scripts
		# that have come to expect this behavior, even though
		# usually the Perl scripts ported to VMS have to be
		# patched because of it changing Unix syntax file
		# to VMS format.

		$rslt = $self->eliminate_macros($spath);


	        $rslt = vmsify($rslt.((defined $rslt) &&
		    ($rslt ne '') ? '/' : '').unixify($file));
	    }
	}
    }
    else {
        # Only passed a single file?
        my $xfile = $file;

        # Traditional VMS perl expects this conversion.
        $xfile = vmsify($file) unless ($efs);

        $rslt = (defined($file) && length($file)) ? $xfile : '';
    }
    return $self->canonpath($rslt) unless $unix_rpt;

    # In Unix report mode, do not strip off redundent path information.
    return $rslt;
}


=item curdir (override)

Returns a string representation of the current directory: '[]' or '.'

=cut

sub curdir {
    my $self = shift @_;
    return '.' if ($self->_unix_rpt);
    return '[]';
}

=item devnull (override)

Returns a string representation of the null device: '_NLA0:' or '/dev/null'

=cut

sub devnull {
    my $self = shift @_;
    return '/dev/null' if ($self->_unix_rpt);
    return "_NLA0:";
}

=item rootdir (override)

Returns a string representation of the root directory: 'SYS$DISK:[000000]'
or '/'

=cut

sub rootdir {
    my $self = shift @_;
    if ($self->_unix_rpt) {
       # Root may exist, try it first.
       my $try = '/';
       my ($dev1, $ino1) = stat('/');
       my ($dev2, $ino2) = stat('.');

       # Perl falls back to '.' if it can not determine '/'
       if (($dev1 != $dev2) || ($ino1 != $ino2)) {
           return $try;
       }
       # Fall back to UNIX format sys$disk.
       return '/sys$disk/';
    }
    return 'SYS$DISK:[000000]';
}

=item tmpdir (override)

Returns a string representation of the first writable directory
from the following list or '' if none are writable:

    /tmp if C<DECC$FILENAME_UNIX_REPORT> is enabled.
    sys$scratch:
    $ENV{TMPDIR}

Since perl 5.8.0, if running under taint mode, and if $ENV{TMPDIR}
is tainted, it is not used.

=cut

my $tmpdir;
sub tmpdir {
    my $self = shift @_;
    return $tmpdir if defined $tmpdir;
    if ($self->_unix_rpt) {
        $tmpdir = $self->_tmpdir('/tmp', '/sys$scratch', $ENV{TMPDIR});
        return $tmpdir;
    }

    $tmpdir = $self->_tmpdir( 'sys$scratch:', $ENV{TMPDIR} );
}

=item updir (override)

Returns a string representation of the parent directory: '[-]' or '..'

=cut

sub updir {
    my $self = shift @_;
    return '..' if ($self->_unix_rpt);
    return '[-]';
}

=item case_tolerant (override)

VMS file specification syntax is case-tolerant.

=cut

sub case_tolerant {
    return 1;
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my (@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    return @dirs;
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    # If it's a logical name, expand it.
    $file = $ENV{$file} while $file =~ /^[\w\$\-]+\Z(?!\n)/s && $ENV{$file};
    return scalar($file =~ m!^/!s             ||
		  $file =~ m![<\[][^.\-\]>]!  ||
		  $file =~ /:[^<\[]/);
}

=item splitpath (override)

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Passing a true value for C<$no_file> indicates that the path being
split only contains directory components, even on systems where you
can usually (when not supporting a foreign syntax) tell the difference
between directories and files at a glance.

=cut

sub splitpath {
    my($self,$path, $nofile) = @_;
    my($dev,$dir,$file)      = ('','','');
    my $efs = $self->_efs;
    my $vmsify_path = vmsify($path);
    if ($efs) {
        my $path_vms = 0;
        $path_vms = 1 if ($path =~ m#(?<!\^)[\[<\]:]#);
        $path_vms = 1 if ($path =~ /^--?$/);
        if (!$path_vms) {
            return $self->SUPER::splitpath($path, $nofile);
        }
        $vmsify_path = $path;
    }

    if ( $nofile ) {
        #vmsify('d1/d2/d3') returns '[.d1.d2]d3'
        #vmsify('/d1/d2/d3') returns 'd1:[d2]d3'
        if( $vmsify_path =~ /(.*)\](.+)/ ){
            $vmsify_path = $1.'.'.$2.']';
        }
        $vmsify_path =~ /(.+:)?(.*)/s;
        $dir = defined $2 ? $2 : ''; # dir can be '0'
        return ($1 || '',$dir,$file);
    }
    else {
        $vmsify_path =~ /(.+:)?([\[<].*[\]>])?(.*)/s;
        return ($1 || '',$2 || '',$3);
    }
}

=item splitdir (override)

Split a directory specification into the components.

=cut

sub splitdir {
    my($self,$dirspec) = @_;
    my @dirs = ();
    return @dirs if ( (!defined $dirspec) || ('' eq $dirspec) );

    my $efs = $self->_efs;

    my $dir_unix = 0;
    $dir_unix = 1 if ($dirspec =~ m#/#);
    $dir_unix = 1 if ($dirspec =~ /^\.\.?$/);

    # Unix filespecs in EFS mode handled by Unix routines.
    if ($efs && $dir_unix) {
        return $self->SUPER::splitdir($dirspec);
    }

    # FIX ME, only split for VMS delimiters not prefixed with '^'.

    $dirspec =~ tr/<>/[]/;			# < and >	==> [ and ]
    $dirspec =~ s/\]\[\./\.\]\[/g;		# ][.		==> .][
    $dirspec =~ s/\[000000\.\]\[/\[/g;		# [000000.][	==> [
    $dirspec =~ s/\[000000\./\[/g;		# [000000.	==> [
    $dirspec =~ s/\.\]\[000000\]/\]/g;		# .][000000]	==> ]
    $dirspec =~ s/\.\]\[/\./g;			# foo.][bar	==> foo.bar
    while ($dirspec =~ s/(^|[\[\<\.])\-(\-+)($|[\]\>\.])/$1-.$2$3/g) {}
						# That loop does the following
						# with any amount of dashes:
						# .--.		==> .-.-.
						# [--.		==> [-.-.
						# .--]		==> .-.-]
						# [--]		==> [-.-]
    $dirspec = "[$dirspec]" unless $dirspec =~ /(?<!\^)[\[<]/; # make legal
    $dirspec =~ s/^(\[|<)\./$1/;
    @dirs = split /(?<!\^)\./, vmspath($dirspec);
    $dirs[0] =~ s/^[\[<]//s;  $dirs[-1] =~ s/[\]>]\Z(?!\n)//s;
    @dirs;
}


=item catpath (override)

Construct a complete filespec.

=cut

sub catpath {
    my($self,$dev,$dir,$file) = @_;
    
    my $efs = $self->_efs;
    my $unix_rpt = $self->_unix_rpt;

    my $unix_mode = 0;
    my $dir_unix = 0;
    $dir_unix = 1 if ($dir =~ m#/#);
    $dir_unix = 1 if ($dir =~ /^\.\.?$/);
    my $dir_vms = 0;
    $dir_vms = 1 if ($dir =~ m#(?<!\^)[\[<\]:]#);
    $dir_vms = 1 if ($dir =~ /^--?$/);

    if ($efs && (length($dev) == 0)) {
        if ($dir_unix == $dir_vms) {
            $unix_mode = $unix_rpt;
        } else {
            $unix_mode = $dir_unix;
        }
    } 

    # We look for a volume in $dev, then in $dir, but not both
    # but only if using VMS syntax.
    if (!$unix_mode) {
        $dir = vmspath($dir) if $dir_unix;
        my ($dir_volume, $dir_dir, $dir_file) = $self->splitpath($dir);
        $dev = $dir_volume unless length $dev;
        $dir = length $dir_file ? $self->catfile($dir_dir, $dir_file) :
                                  $dir_dir;
    }
    if ($dev =~ m|^/+([^/]+)|) { $dev = "$1:"; }
    else { $dev .= ':' unless $dev eq '' or $dev =~ /:\Z(?!\n)/; }
    if (length($dev) or length($dir)) {
      if ($efs) {
          if ($unix_mode) {
              $dir .= '/' unless ($dir =~ m#/$#);
          } else {
              $dir = vmspath($dir) if (($dir =~ m#/#) || ($dir =~ /^\.\.?$/));
              $dir = "[$dir]" unless $dir =~ /^[\[<]/;
          }
      } else {
          $dir = "[$dir]" unless $dir =~ /[\[<\/]/;
          $dir = vmspath($dir);
      }
    }
    $dir = '' if length($dev) && ($dir eq '[]' || $dir eq '<>');
    "$dev$dir$file";
}

=item abs2rel (override)

Attempt to convert a file specification to a relative specification.
On a system with volumes, like VMS, this may not be possible.

=cut

sub abs2rel {
    my $self = shift;
    my($path,$base) = @_;

    my $efs = $self->_efs;
    my $unix_rpt = $self->_unix_rpt;

    # We need to identify what the directory is in
    # of the specification in order to process them
    my $path_unix = 0;
    $path_unix = 1 if ($path =~ m#/#);
    $path_unix = 1 if ($path =~ /^\.\.?$/);
    my $path_vms = 0;
    $path_vms = 1 if ($path =~ m#(?<!\^)[\[<\]:]#);
    $path_vms = 1 if ($path =~ /^--?$/);

    my $unix_mode = 0;
    if ($path_vms == $path_unix) {
        $unix_mode = $unix_rpt;
    } else {
        $unix_mode = $path_unix;
    }

    my $base_unix = 0;
    my $base_vms = 0;

    if (defined $base) {
        $base_unix = 1 if ($base =~ m#/#);
        $base_unix = 1 if ($base =~ /^\.\.?$/);
        $base_vms = 1 if ($base =~ m#(?<!\^)[\[<\]:]#);
        $base_vms = 1 if ($base =~ /^--?$/);

        if ($path_vms == $path_unix) {
            if ($base_vms == $base_unix) {
                $unix_mode = $unix_rpt;
            } else {
                $unix_mode = $base_unix;
            }
        } else {
            $unix_mode = 0 if $base_vms;
        }
    }

    if ($efs) {
        if ($unix_mode) {
            # We are UNIX mode.
            $base = unixpath($base) if $base_vms;
            $base = unixify($path) if $path_vms;

            # Here VMS is different, and in order to do this right
            # we have to take the realpath for both the path and the base
            # so that we can remove the common components.

            if ($path =~ m#^/#) {
                if (defined $base) {

                    # For the shorterm, if the starting directories are
                    # common, remove them.
                    my $bq = qq($base);
                    $bq =~ s/\$/\\\$/;
                    $path =~ s/^$bq//i;
                }
                return $path;
            }

            return File::Spec::Unix::abs2rel( $self, $path, $base );

        } else {
            $base = vmspath($base) if $base_unix;
            $path = vmsify($path) if $path_unix;
        }
    }

    unless (defined $base and length $base) {
        $base = $self->_cwd();
        if ($efs) {
            $base_unix = 1 if ($base =~ m#/#);
            $base_unix = 1 if ($base =~ /^\.\.?$/);
            $base = vmspath($base) if $base_unix;
        }
    }

    for ($path, $base) { $_ = $self->canonpath($_) }

    # Are we even starting $path on the same (node::)device as $base?  Note that
    # logical paths or nodename differences may be on the "same device" 
    # but the comparison that ignores device differences so as to concatenate 
    # [---] up directory specs is not even a good idea in cases where there is 
    # a logical path difference between $path and $base nodename and/or device.
    # Hence we fall back to returning the absolute $path spec
    # if there is a case blind device (or node) difference of any sort
    # and we do not even try to call $parse() or consult %ENV for $trnlnm()
    # (this module needs to run on non VMS platforms after all).
    
    my ($path_volume, $path_directories, $path_file) = $self->splitpath($path);
    my ($base_volume, $base_directories, $base_file) = $self->splitpath($base);
    return $path unless lc($path_volume) eq lc($base_volume);

    for ($path, $base) { $_ = $self->rel2abs($_) }

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_directories );
    my $pathchunks = @pathchunks;
    unshift(@pathchunks,'000000') unless $pathchunks[0] eq '000000';
    my @basechunks = $self->splitdir( $base_directories );
    my $basechunks = @basechunks;
    unshift(@basechunks,'000000') unless $basechunks[0] eq '000000';

    while ( @pathchunks && 
            @basechunks && 
            lc( $pathchunks[0] ) eq lc( $basechunks[0] ) 
          ) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    # @basechunks now contains the directories to climb out of,
    # @pathchunks now has the directories to descend in to.
    if ((@basechunks > 0) || ($basechunks != $pathchunks)) {
      $path_directories = join '.', ('-' x @basechunks, @pathchunks) ;
    }
    else {
      $path_directories = join '.', @pathchunks;
    }
    $path_directories = '['.$path_directories.']';
    return $self->canonpath( $self->catpath( '', $path_directories, $path_file ) ) ;
}


=item rel2abs (override)

Return an absolute file specification from a relative one.

=cut

sub rel2abs {
    my $self = shift ;
    my ($path,$base ) = @_;
    return undef unless defined $path;

    my $efs = $self->_efs;
    my $unix_rpt = $self->_unix_rpt;

    # We need to identify what the directory is in
    # of the specification in order to process them
    my $path_unix = 0;
    $path_unix = 1 if ($path =~ m#/#);
    $path_unix = 1 if ($path =~ /^\.\.?$/);
    my $path_vms = 0;
    $path_vms = 1 if ($path =~ m#(?<!\^)[\[<\]:]#);
    $path_vms = 1 if ($path =~ /^--?$/);

    my $unix_mode = 0;
    if ($path_vms == $path_unix) {
        $unix_mode = $unix_rpt;
    } else {
        $unix_mode = $path_unix;
    }

    my $base_unix = 0;
    my $base_vms = 0;

    if (defined $base) {
        $base_unix = 1 if ($base =~ m#/#);
        $base_unix = 1 if ($base =~ /^\.\.?$/);
        $base_vms = 1 if ($base =~ m#(?<!\^)[\[<\]:]#);
        $base_vms = 1 if ($base =~ /^--?$/);

        # If we could not determine the path mode, see if we can find out
        # from the base.
        if ($path_vms == $path_unix) {
            if ($base_vms != $base_unix) {
                $unix_mode = $base_unix;
            }
        }
    }

    if (!$efs) {
        # Legacy behavior, convert to VMS syntax.
        $unix_mode = 0;
        if (defined $base) {
            $base = vmspath($base) if $base =~ m/\//;
        }

        if ($path =~ m/\//) {
	    $path = ( -d $path || $path =~ m/\/\z/  # educated guessing about
		       ? vmspath($path)             # whether it's a directory
		       : vmsify($path) );
        }
   }

    # Clean up and split up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        # Figure out the effective $base and clean it up.
        if ( !defined( $base ) || $base eq '' ) {
            $base = $self->_cwd;
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        if ($efs) {
            # base may have changed, so need to look up format again.
            if ($unix_mode) {
                $base_vms = 1 if ($base =~ m#(?<!\^)[\[<\]:]#);
                $base_vms = 1 if ($base =~ /^--?$/);
                $base = unixpath($base) if $base_vms;
                $base .= '/' unless ($base =~ m#/$#);
            } else {
                $base_unix = 1 if ($base =~ m#/#);
                $base_unix = 1 if ($base =~ /^\.\.?$/);
                $base = vmspath($base) if $base_unix; 
            }
        }

        # Split up paths
        my ( $path_directories, $path_file ) =
            ($self->splitpath( $path ))[1,2] ;

        my ( $base_volume, $base_directories ) =
            $self->splitpath( $base ) ;

        $path_directories = '' if $path_directories eq '[]' ||
                                  $path_directories eq '<>';
        my $sep = '' ;

        if ($efs) {
            # Merge the paths assuming that the base is absolute.
            $base_directories = $self->catdir('',
                                              $base_directories,
                                              $path_directories);
        } else {
            # Legacy behavior assumes VMS only paths
            $sep = '.'
                if ( $base_directories =~ m{[^.\]>]\Z(?!\n)} &&
                     $path_directories =~ m{^[^.\[<]}s
                ) ;
            $base_directories = "$base_directories$sep$path_directories";
            $base_directories =~ s{\.?[\]>][\[<]\.?}{.};
        }

        $path_file = '' if ($path_file eq '.') && $unix_mode;

        $path = $self->catpath( $base_volume, $base_directories, $path_file );
   }

    return $self->canonpath( $path ) ;
}


# eliminate_macros() and fixpath() are MakeMaker-specific methods
# which are used inside catfile() and catdir().  MakeMaker has its own
# copies as of 6.06_03 which are the canonical ones.  We leave these
# here, in peace, so that File::Spec continues to work with MakeMakers
# prior to 6.06_03.
# 
# Please consider these two methods deprecated.  Do not patch them,
# patch the ones in ExtUtils::MM_VMS instead.
#
# Update:  MakeMaker 6.48 is still using these routines on VMS.
# so they need to be kept up to date with ExtUtils::MM_VMS.
#
# The traditional VMS mode using ODS-2 disks depends on these routines
# being here.  These routines should not be called in when the
# C<DECC$EFS_CHARSET> or C<DECC$FILENAME_UNIX_REPORT> modes are enabled.

sub eliminate_macros {
    my($self,$path) = @_;
    return '' unless (defined $path) && ($path ne '');
    $self = {} unless ref $self;

    if ($path =~ /\s/) {
      return join ' ', map { $self->eliminate_macros($_) } split /\s+/, $path;
    }

    my $npath = unixify($path);
    # sometimes unixify will return a string with an off-by-one trailing null
    $npath =~ s{\0$}{};

    my($complex) = 0;
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#gs) { 
        if (defined $self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            if (ref $self->{$macro}) {
                if (ref $self->{$macro} eq 'ARRAY') {
                    $macro = join ' ', @{$self->{$macro}};
                }
                else {
                    print "Note: can't expand macro \$($macro) containing ",ref($self->{$macro}),
                          "\n\t(using MMK-specific deferred substitutuon; MMS will break)\n";
                    $macro = "\cB$macro\cB";
                    $complex = 1;
                }
            }
            else { ($macro = unixify($self->{$macro})) =~ s#/\Z(?!\n)##; }
            $npath = "$head$macro$tail";
        }
    }
    if ($complex) { $npath =~ s#\cB(.*?)\cB#\${$1}#gs; }
    $npath;
}

# Deprecated.  See the note above for eliminate_macros().

# Catchall routine to clean up problem MM[SK]/Make macros.  Expands macros
# in any directory specification, in order to avoid juxtaposing two
# VMS-syntax directories when MM[SK] is run.  Also expands expressions which
# are all macro, so that we can tell how long the expansion is, and avoid
# overrunning DCL's command buffer when MM[KS] is running.

# fixpath() checks to see whether the result matches the name of a
# directory in the current default directory and returns a directory or
# file specification accordingly.  C<$is_dir> can be set to true to
# force fixpath() to consider the path to be a directory or false to force
# it to be a file.

sub fixpath {
    my($self,$path,$force_path) = @_;
    return '' unless $path;
    $self = bless {}, $self unless ref $self;
    my($fixedpath,$prefix,$name);

    if ($path =~ /\s/) {
      return join ' ',
             map { $self->fixpath($_,$force_path) }
	     split /\s+/, $path;
    }

    if ($path =~ m#^\$\([^\)]+\)\Z(?!\n)#s || $path =~ m#[/:>\]]#) { 
        if ($force_path or $path =~ /(?:DIR\)|\])\Z(?!\n)/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#s)) && $self->{$prefix}) {
        my($vmspre) = $self->eliminate_macros("\$($prefix)");
        # is it a dir or just a name?
        $vmspre = ($vmspre =~ m|/| or $prefix =~ /DIR\Z(?!\n)/) ? vmspath($vmspre) : '';
        $fixedpath = ($vmspre ? $vmspre : $self->{$prefix}) . $name;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    else {
        $fixedpath = $path;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    # No hints, so we try to guess
    if (!defined($force_path) and $fixedpath !~ /[:>(.\]]/) {
        $fixedpath = vmspath($fixedpath) if -d $fixedpath;
    }

    # Trim off root dirname if it's had other dirs inserted in front of it.
    $fixedpath =~ s/\.000000([\]>])/$1/;
    # Special case for VMS absolute directory specs: these will have had device
    # prepended during trip through Unix syntax in eliminate_macros(), since
    # Unix syntax has no way to express "absolute from the top of this device's
    # directory tree".
    if ($path =~ /^[\[>][^.\-]/) { $fixedpath =~ s/^[^\[<]+//; }
    $fixedpath;
}


=back

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

An explanation of VMS file specs can be found at
L<http://h71000.www7.hp.com/doc/731FINAL/4506/4506pro_014.html#apps_locating_naming_files>.

=cut

1;
