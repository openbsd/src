package FileCache;

our $VERSION = '1.021';

=head1 NAME

FileCache - keep more files open than the system permits

=head1 SYNOPSIS

    use FileCache;
    # or
    use FileCache maxopen => 16;

    cacheout $path;
    print $path @data;

    cacheout $mode, $path;
    print $path @data;

=head1 DESCRIPTION

The C<cacheout> function will make sure that there's a filehandle open
for reading or writing available as the pathname you give it. It
automatically closes and re-opens files if you exceed  your system's
maximum number of file descriptors, or the suggested maximum.

=over

=item cacheout EXPR

The 1-argument form of cacheout will open a file for writing (C<< '>' >>)
on it's first use, and appending (C<<< '>>' >>>) thereafter.

=item cacheout MODE, EXPR

The 2-argument form of cacheout will use the supplied mode for the initial
and subsequent openings. Most valid modes for 3-argument C<open> are supported
namely; C<< '>' >>, C<< '+>' >>, C<< '<' >>, C<< '<+' >>, C<<< '>>' >>>,
C< '|-' > and C< '-|' >

=head1 CAVEATS

If you use cacheout with C<'|-'> or C<'-|'> you should catch SIGPIPE
and explicitly close the filehandle., when it is closed from the
other end some cleanup needs to be done.

While it is permissible to C<close> a FileCache managed file,
do not do so if you are calling C<FileCache::cacheout> from a package other
than which it was imported, or with another module which overrides C<close>.
If you must, use C<FileCache::cacheout_close>.

=head1 BUGS

F<sys/param.h> lies with its C<NOFILE> define on some systems,
so you may have to set maxopen (I<$FileCache::cacheout_maxopen>) yourself.

=cut

require 5.006;
use Carp;
use strict;
no strict 'refs';
use vars qw(%saw $cacheout_maxopen);
# These are not C<my> for legacy reasons.
# Previous versions requested the user set $cacheout_maxopen by hand.
# Some authors fiddled with %saw to overcome the clobber on initial open.
my %isopen;
my $cacheout_seq = 0;

sub import {
    my ($pkg,%args) = @_;
    *{caller(1).'::cacheout'} = \&cacheout;
    *{caller(1).'::close'}    = \&cacheout_close;

    # Truth is okay here because setting maxopen to 0 would be bad
    return $cacheout_maxopen = $args{maxopen} if $args{maxopen} ;
    if (open(PARAM,'/usr/include/sys/param.h')) {
      local ($_, $.);
      while (<PARAM>) {
	$cacheout_maxopen = $1 - 4
	  if /^\s*#\s*define\s+NOFILE\s+(\d+)/;
      }
      close PARAM;
    }
    $cacheout_maxopen ||= 16;
}

# Open in their package.

sub cacheout_open {
    open(*{caller(1) . '::' . $_[1]}, $_[0], $_[1]);
}

# Close in their package.

sub cacheout_close {
    fileno(*{caller(1) . '::' . $_[0]}) &&
      CORE::close(*{caller(1) . '::' . $_[0]});
    delete $isopen{$_[0]};
}

# But only this sub name is visible to them.
 
sub cacheout {
    croak "Not enough arguments for cacheout"  unless @_;
    croak "Too many arguments for cacheout" if scalar @_ > 2;
    my($mode, $file)=@_;
    ($file, $mode) = ($mode, $file) if scalar @_ == 1;
    # We don't want children
    croak "Invalid file for cacheout" if $file =~ /^\s*(?:\|\-)|(?:\-\|)\s*$/;
    croak "Invalid mode for cacheout" if $mode &&
      ( $mode !~ /^\s*(?:>>)|(?:\+?>)|(?:\+?<)|(?:\|\-)|(?:\-\|)\s*$/ );

    unless( $isopen{$file}) {
      if( scalar keys(%isopen) > $cacheout_maxopen -1 ) {
	my @lru = sort {$isopen{$a} <=> $isopen{$b};} keys(%isopen);
	&cacheout_close($_) for splice(@lru, $cacheout_maxopen / 3);
      }
      $mode ||=  ( $saw{$file} = ! $saw{$file} ) ? '>': '>>';
      cacheout_open($mode, $file) or croak("Can't create $file: $!");
    }
    $isopen{$file} = ++$cacheout_seq;
}

1;
