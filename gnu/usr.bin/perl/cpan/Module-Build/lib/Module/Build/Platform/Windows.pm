package Module::Build::Platform::Windows;

use strict;
use vars qw($VERSION);
$VERSION = '0.4205';
$VERSION = eval $VERSION;

use Config;
use File::Basename;
use File::Spec;

use Module::Build::Base;

use vars qw(@ISA);
@ISA = qw(Module::Build::Base);


sub manpage_separator {
    return '.';
}

sub have_forkpipe { 0 }

sub _detildefy {
  my ($self, $value) = @_;
  $value =~ s,^~(?= [/\\] | $ ),$ENV{HOME},x
    if $ENV{HOME};
  return $value;
}

sub ACTION_realclean {
  my ($self) = @_;

  $self->SUPER::ACTION_realclean();

  my $basename = basename($0);
  $basename =~ s/(?:\.bat)?$//i;

  if ( lc $basename eq lc $self->build_script ) {
    if ( $self->build_bat ) {
      $self->log_verbose("Deleting $basename.bat\n");
      my $full_progname = $0;
      $full_progname =~ s/(?:\.bat)?$/.bat/i;

      # Voodoo required to have a batch file delete itself without error;
      # Syntax differs between 9x & NT: the later requires a null arg (???)
      require Win32;
      my $null_arg = (Win32::IsWinNT()) ? '""' : '';
      my $cmd = qq(start $null_arg /min "\%comspec\%" /c del "$full_progname");

      open(my $fh, '>>', "$basename.bat")
        or die "Can't create $basename.bat: $!";
      print $fh $cmd;
      close $fh ;
    } else {
      $self->delete_filetree($self->build_script . '.bat');
    }
  }
}

sub make_executable {
  my $self = shift;

  $self->SUPER::make_executable(@_);

  foreach my $script (@_) {

    # Native batch script
    if ( $script =~ /\.(bat|cmd)$/ ) {
      $self->SUPER::make_executable($script);
      next;

    # Perl script that needs to be wrapped in a batch script
    } else {
      my %opts = ();
      if ( $script eq $self->build_script ) {
        $opts{ntargs}    = q(-x -S %0 --build_bat %*);
        $opts{otherargs} = q(-x -S "%0" --build_bat %1 %2 %3 %4 %5 %6 %7 %8 %9);
      }

      my $out = eval {$self->pl2bat(in => $script, update => 1, %opts)};
      if ( $@ ) {
        $self->log_warn("WARNING: Unable to convert file '$script' to an executable script:\n$@");
      } else {
        $self->SUPER::make_executable($out);
      }
    }
  }
}

# This routine was copied almost verbatim from the 'pl2bat' utility
# distributed with perl. It requires too much voodoo with shell quoting
# differences and shortcomings between the various flavors of Windows
# to reliably shell out
sub pl2bat {
  my $self = shift;
  my %opts = @_;

  # NOTE: %0 is already enclosed in doublequotes by cmd.exe, as appropriate
  $opts{ntargs}    = '-x -S %0 %*' unless exists $opts{ntargs};
  $opts{otherargs} = '-x -S "%0" %1 %2 %3 %4 %5 %6 %7 %8 %9' unless exists $opts{otherargs};

  $opts{stripsuffix} = '/\\.plx?/' unless exists $opts{stripsuffix};
  $opts{stripsuffix} = ($opts{stripsuffix} =~ m{^/([^/]*[^/\$]|)\$?/?$} ? $1 : "\Q$opts{stripsuffix}\E");

  unless (exists $opts{out}) {
    $opts{out} = $opts{in};
    $opts{out} =~ s/$opts{stripsuffix}$//oi;
    $opts{out} .= '.bat' unless $opts{in} =~ /\.bat$/i or $opts{in} =~ /^-$/;
  }

  my $head = <<EOT;
    \@rem = '--*-Perl-*--
    \@echo off
    if "%OS%" == "Windows_NT" goto WinNT
    perl $opts{otherargs}
    goto endofperl
    :WinNT
    perl $opts{ntargs}
    if NOT "%COMSPEC%" == "%SystemRoot%\\system32\\cmd.exe" goto endofperl
    if %errorlevel% == 9009 echo You do not have Perl in your PATH.
    if errorlevel 1 goto script_failed_so_exit_with_non_zero_val 2>nul
    goto endofperl
    \@rem ';
EOT

  $head =~ s/^\s+//gm;
  my $headlines = 2 + ($head =~ tr/\n/\n/);
  my $tail = "\n__END__\n:endofperl\n";

  my $linedone  = 0;
  my $taildone  = 0;
  my $linenum   = 0;
  my $skiplines = 0;

  my $start = $Config{startperl};
  $start = "#!perl" unless $start =~ /^#!.*perl/;

  open(my $in, '<', "$opts{in}") or die "Can't open $opts{in}: $!";
  my @file = <$in>;
  close($in);

  foreach my $line ( @file ) {
    $linenum++;
    if ( $line =~ /^:endofperl\b/ ) {
      if (!exists $opts{update}) {
        warn "$opts{in} has already been converted to a batch file!\n";
        return;
      }
      $taildone++;
    }
    if ( not $linedone and $line =~ /^#!.*perl/ ) {
      if (exists $opts{update}) {
        $skiplines = $linenum - 1;
        $line .= "#line ".(1+$headlines)."\n";
      } else {
	$line .= "#line ".($linenum+$headlines)."\n";
      }
	$linedone++;
    }
    if ( $line =~ /^#\s*line\b/ and $linenum == 2 + $skiplines ) {
      $line = "";
    }
  }

  open(my $out, '>', "$opts{out}") or die "Can't open $opts{out}: $!";
  print $out $head;
  print $out $start, ( $opts{usewarnings} ? " -w" : "" ),
             "\n#line ", ($headlines+1), "\n" unless $linedone;
  print $out @file[$skiplines..$#file];
  print $out $tail unless $taildone;
  close($out);

  return $opts{out};
}


sub _quote_args {
  # Returns a string that can become [part of] a command line with
  # proper quoting so that the subprocess sees this same list of args.
  my ($self, @args) = @_;

  my @quoted;

  for (@args) {
    if ( /^[^\s*?!\$<>;|'"\[\]\{\}]+$/ ) {
      # Looks pretty safe
      push @quoted, $_;
    } else {
      # XXX this will obviously have to improve - is there already a
      # core module lying around that does proper quoting?
      s/"/\\"/g;
      push @quoted, qq("$_");
    }
  }

  return join " ", @quoted;
}


sub split_like_shell {
  # As it turns out, Windows command-parsing is very different from
  # Unix command-parsing.  Double-quotes mean different things,
  # backslashes don't necessarily mean escapes, and so on.  So we
  # can't use Text::ParseWords::shellwords() to break a command string
  # into words.  The algorithm below was bashed out by Randy and Ken
  # (mostly Randy), and there are a lot of regression tests, so we
  # should feel free to adjust if desired.

  (my $self, local $_) = @_;

  return @$_ if defined() && UNIVERSAL::isa($_, 'ARRAY');

  my @argv;
  return @argv unless defined() && length();

  my $arg = '';
  my( $i, $quote_mode ) = ( 0, 0 );

  while ( $i < length() ) {

    my $ch      = substr( $_, $i  , 1 );
    my $next_ch = substr( $_, $i+1, 1 );

    if ( $ch eq '\\' && $next_ch eq '"' ) {
      $arg .= '"';
      $i++;
    } elsif ( $ch eq '\\' && $next_ch eq '\\' ) {
      $arg .= '\\';
      $i++;
    } elsif ( $ch eq '"' && $next_ch eq '"' && $quote_mode ) {
      $quote_mode = !$quote_mode;
      $arg .= '"';
      $i++;
    } elsif ( $ch eq '"' && $next_ch eq '"' && !$quote_mode &&
	      ( $i + 2 == length()  ||
		substr( $_, $i + 2, 1 ) eq ' ' )
	    ) { # for cases like: a"" => [ 'a' ]
      push( @argv, $arg );
      $arg = '';
      $i += 2;
    } elsif ( $ch eq '"' ) {
      $quote_mode = !$quote_mode;
    } elsif ( $ch eq ' ' && !$quote_mode ) {
      push( @argv, $arg ) if $arg;
      $arg = '';
      ++$i while substr( $_, $i + 1, 1 ) eq ' ';
    } else {
      $arg .= $ch;
    }

    $i++;
  }

  push( @argv, $arg ) if defined( $arg ) && length( $arg );
  return @argv;
}


# system(@cmd) does not like having double-quotes in it on Windows.
# So we quote them and run it as a single command.
sub do_system {
  my ($self, @cmd) = @_;

  my $cmd = $self->_quote_args(@cmd);
  my $status = system($cmd);
  if ($status and $! =~ /Argument list too long/i) {
    my $env_entries = '';
    foreach (sort keys %ENV) { $env_entries .= "$_=>".length($ENV{$_})."; " }
    warn "'Argument list' was 'too long', env lengths are $env_entries";
  }
  return !$status;
}

# Copied from ExtUtils::MM_Win32
sub _maybe_command {
    my($self,$file) = @_;
    my @e = exists($ENV{'PATHEXT'})
          ? split(/;/, $ENV{PATHEXT})
	  : qw(.com .exe .bat .cmd);
    my $e = '';
    for (@e) { $e .= "\Q$_\E|" }
    chop $e;
    # see if file ends in one of the known extensions
    if ($file =~ /($e)$/i) {
	return $file if -e $file;
    }
    else {
	for (@e) {
	    return "$file$_" if -e "$file$_";
	}
    }
    return;
}


1;

__END__

=head1 NAME

Module::Build::Platform::Windows - Builder class for Windows platforms

=head1 DESCRIPTION

The sole purpose of this module is to inherit from
C<Module::Build::Base> and override a few methods.  Please see
L<Module::Build> for the docs.

=head1 AUTHOR

Ken Williams <kwilliams@cpan.org>, Randy W. Sims <RandyS@ThePierianSpring.org>

=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
