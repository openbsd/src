package Module::Pluggable::Object;

use strict;
use File::Find ();
use File::Basename;
use File::Spec::Functions qw(splitdir catdir curdir catfile abs2rel);
use Carp qw(croak carp confess);
use Devel::InnerPackage;
use vars qw($VERSION);

use if $] > 5.017, 'deprecate';

$VERSION = '4.6';


sub new {
    my $class = shift;
    my %opts  = @_;

    return bless \%opts, $class;

}

### Eugggh, this code smells 
### This is what happens when you keep adding patches
### *sigh*


sub plugins {
    my $self = shift;
    my @args = @_;

    # override 'require'
    $self->{'require'} = 1 if $self->{'inner'};

    my $filename   = $self->{'filename'};
    my $pkg        = $self->{'package'};

    # Get the exception params instantiated
    $self->_setup_exceptions;

    # automatically turn a scalar search path or namespace into a arrayref
    for (qw(search_path search_dirs)) {
        $self->{$_} = [ $self->{$_} ] if exists $self->{$_} && !ref($self->{$_});
    }

    # default search path is '<Module>::<Name>::Plugin'
    $self->{'search_path'} ||= ["${pkg}::Plugin"]; 

    # default error handler
    $self->{'on_require_error'} ||= sub { my ($plugin, $err) = @_; carp "Couldn't require $plugin : $err"; return 0 };
    $self->{'on_instantiate_error'} ||= sub { my ($plugin, $err) = @_; carp "Couldn't instantiate $plugin: $err"; return 0 };

    # default whether to follow symlinks
    $self->{'follow_symlinks'} = 1 unless exists $self->{'follow_symlinks'};

    # check to see if we're running under test
    my @SEARCHDIR = exists $INC{"blib.pm"} && defined $filename && $filename =~ m!(^|/)blib/! && !$self->{'force_search_all_paths'} ? grep {/blib/} @INC : @INC;

    # add any search_dir params
    unshift @SEARCHDIR, @{$self->{'search_dirs'}} if defined $self->{'search_dirs'};

    # set our @INC up to include and prefer our search_dirs if necessary
    my @tmp = @INC;
    unshift @tmp, @{$self->{'search_dirs'} || []};
    local @INC = @tmp if defined $self->{'search_dirs'};

    my @plugins = $self->search_directories(@SEARCHDIR);
    push(@plugins, $self->handle_innerpackages($_)) for @{$self->{'search_path'}};
    
    # return blank unless we've found anything
    return () unless @plugins;

    # remove duplicates
    # probably not necessary but hey ho
    my %plugins;
    for(@plugins) {
        next unless $self->_is_legit($_);
        $plugins{$_} = 1;
    }

    # are we instantiating or requring?
    if (defined $self->{'instantiate'}) {
        my $method = $self->{'instantiate'};
        my @objs   = ();
        foreach my $package (sort keys %plugins) {
            next unless $package->can($method);
            my $obj = eval { $package->$method(@_) };
            $self->{'on_instantiate_error'}->($package, $@) if $@;
            push @objs, $obj if $obj;           
        }
        return @objs;
    } else { 
        # no? just return the names
        my @objs= sort keys %plugins;
        return @objs;
    }
}

sub _setup_exceptions {
    my $self = shift;

    my %only;   
    my %except; 
    my $only;
    my $except;

    if (defined $self->{'only'}) {
        if (ref($self->{'only'}) eq 'ARRAY') {
            %only   = map { $_ => 1 } @{$self->{'only'}};
        } elsif (ref($self->{'only'}) eq 'Regexp') {
            $only = $self->{'only'}
        } elsif (ref($self->{'only'}) eq '') {
            $only{$self->{'only'}} = 1;
        }
    }
        

    if (defined $self->{'except'}) {
        if (ref($self->{'except'}) eq 'ARRAY') {
            %except   = map { $_ => 1 } @{$self->{'except'}};
        } elsif (ref($self->{'except'}) eq 'Regexp') {
            $except = $self->{'except'}
        } elsif (ref($self->{'except'}) eq '') {
            $except{$self->{'except'}} = 1;
        }
    }
    $self->{_exceptions}->{only_hash}   = \%only;
    $self->{_exceptions}->{only}        = $only;
    $self->{_exceptions}->{except_hash} = \%except;
    $self->{_exceptions}->{except}      = $except;
        
}

sub _is_legit {
    my $self   = shift;
    my $plugin = shift;
    my %only   = %{$self->{_exceptions}->{only_hash}||{}};
    my %except = %{$self->{_exceptions}->{except_hash}||{}};
    my $only   = $self->{_exceptions}->{only};
    my $except = $self->{_exceptions}->{except};
    my $depth  = () = split '::', $plugin, -1;

    return 0 if     (keys %only   && !$only{$plugin}     );
    return 0 unless (!defined $only || $plugin =~ m!$only!     );

    return 0 if     (keys %except &&  $except{$plugin}   );
    return 0 if     (defined $except &&  $plugin =~ m!$except! );
    
    return 0 if     defined $self->{max_depth} && $depth>$self->{max_depth};
    return 0 if     defined $self->{min_depth} && $depth<$self->{min_depth};

    return 1;
}

sub search_directories {
    my $self      = shift;
    my @SEARCHDIR = @_;

    my @plugins;
    # go through our @INC
    foreach my $dir (@SEARCHDIR) {
        push @plugins, $self->search_paths($dir);
    }
    return @plugins;
}


sub search_paths {
    my $self = shift;
    my $dir  = shift;
    my @plugins;

    my $file_regex = $self->{'file_regex'} || qr/\.pm$/;


    # and each directory in our search path
    foreach my $searchpath (@{$self->{'search_path'}}) {
        # create the search directory in a cross platform goodness way
        my $sp = catdir($dir, (split /::/, $searchpath));

        # if it doesn't exist or it's not a dir then skip it
        next unless ( -e $sp && -d _ ); # Use the cached stat the second time

        my @files = $self->find_files($sp);

        # foreach one we've found 
        foreach my $file (@files) {
            # untaint the file; accept .pm only
            next unless ($file) = ($file =~ /(.*$file_regex)$/); 
            # parse the file to get the name
            my ($name, $directory, $suffix) = fileparse($file, $file_regex);

            next if (!$self->{include_editor_junk} && $self->_is_editor_junk($name));

            $directory = abs2rel($directory, $sp);

            # If we have a mixed-case package name, assume case has been preserved
            # correctly.  Otherwise, root through the file to locate the case-preserved
            # version of the package name.
            my @pkg_dirs = ();
            if ( $name eq lc($name) || $name eq uc($name) ) {
                my $pkg_file = catfile($sp, $directory, "$name$suffix");
                open PKGFILE, "<$pkg_file" or die "search_paths: Can't open $pkg_file: $!";
                my $in_pod = 0;
                while ( my $line = <PKGFILE> ) {
                    $in_pod = 1 if $line =~ m/^=\w/;
                    $in_pod = 0 if $line =~ /^=cut/;
                    next if ($in_pod || $line =~ /^=cut/);  # skip pod text
                    next if $line =~ /^\s*#/;               # and comments
                    if ( $line =~ m/^\s*package\s+(.*::)?($name)\s*;/i ) {
                        @pkg_dirs = split /::/, $1 if defined $1;;
                        $name = $2;
                        last;
                    }
                }
                close PKGFILE;
            }

            # then create the class name in a cross platform way
            $directory =~ s/^[a-z]://i if($^O =~ /MSWin32|dos/);       # remove volume
            my @dirs = ();
            if ($directory) {
                ($directory) = ($directory =~ /(.*)/);
                @dirs = grep(length($_), splitdir($directory)) 
                    unless $directory eq curdir();
                for my $d (reverse @dirs) {
                    my $pkg_dir = pop @pkg_dirs; 
                    last unless defined $pkg_dir;
                    $d =~ s/\Q$pkg_dir\E/$pkg_dir/i;  # Correct case
                }
            } else {
                $directory = "";
            }
            my $plugin = join '::', $searchpath, @dirs, $name;

            next unless $plugin =~ m!(?:[a-z\d]+)[a-z\d]!i;

            $self->handle_finding_plugin($plugin, \@plugins)
        }

        # now add stuff that may have been in package
        # NOTE we should probably use all the stuff we've been given already
        # but then we can't unload it :(
        push @plugins, $self->handle_innerpackages($searchpath);
    } # foreach $searchpath

    return @plugins;
}

sub _is_editor_junk {
    my $self = shift;
    my $name = shift;

    # Emacs (and other Unix-y editors) leave temp files ending in a
    # tilde as a backup.
    return 1 if $name =~ /~$/;
    # Emacs makes these files while a buffer is edited but not yet
    # saved.
    return 1 if $name =~ /^\.#/;
    # Vim can leave these files behind if it crashes.
    return 1 if $name =~ /\.sw[po]$/;

    return 0;
}

sub handle_finding_plugin {
    my $self    = shift;
    my $plugin  = shift;
    my $plugins = shift;
    my $no_req  = shift || 0;
    
    return unless $self->_is_legit($plugin);
    unless (defined $self->{'instantiate'} || $self->{'require'}) {
        push @$plugins, $plugin;
        return;
    } 

    $self->{before_require}->($plugin) || return if defined $self->{before_require};
    unless ($no_req) {
        my $tmp = $@;
        my $res = eval { $self->_require($plugin) };
        my $err = $@;
        $@      = $tmp;
        if ($err) {
            if (defined $self->{on_require_error}) {
                $self->{on_require_error}->($plugin, $err) || return; 
            } else {
                return;
            }
        }
    }
    $self->{after_require}->($plugin) || return if defined $self->{after_require};
    push @$plugins, $plugin;
}

sub find_files {
    my $self         = shift;
    my $search_path  = shift;
    my $file_regex   = $self->{'file_regex'} || qr/\.pm$/;


    # find all the .pm files in it
    # this isn't perfect and won't find multiple plugins per file
    #my $cwd = Cwd::getcwd;
    my @files = ();
    { # for the benefit of perl 5.6.1's Find, localize topic
        local $_;
        File::Find::find( { no_chdir => 1, 
                            follow   => $self->{'follow_symlinks'}, 
                            wanted   => sub { 
                             # Inlined from File::Find::Rule C< name => '*.pm' >
                             return unless $File::Find::name =~ /$file_regex/;
                             (my $path = $File::Find::name) =~ s#^\\./##;
                             push @files, $path;
                           }
                      }, $search_path );
    }
    #chdir $cwd;
    return @files;

}

sub handle_innerpackages {
    my $self = shift;
    return () if (exists $self->{inner} && !$self->{inner});

    my $path = shift;
    my @plugins;

    foreach my $plugin (Devel::InnerPackage::list_packages($path)) {
        $self->handle_finding_plugin($plugin, \@plugins, 1);
    }
    return @plugins;

}


sub _require {
    my $self   = shift;
    my $pack   = shift;
    eval "CORE::require $pack";
    die ($@) if $@;
    return 1;
}


1;

=pod

=head1 NAME

Module::Pluggable::Object - automatically give your module the ability to have plugins

=head1 SYNOPSIS


Simple use Module::Pluggable -

    package MyClass;
    use Module::Pluggable::Object;
    
    my $finder = Module::Pluggable::Object->new(%opts);
    print "My plugins are: ".join(", ", $finder->plugins)."\n";

=head1 DESCRIPTION

Provides a simple but, hopefully, extensible way of having 'plugins' for 
your module. Obviously this isn't going to be the be all and end all of
solutions but it works for me.

Essentially all it does is export a method into your namespace that 
looks through a search path for .pm files and turn those into class names. 

Optionally it instantiates those classes for you.

This object is wrapped by C<Module::Pluggable>. If you want to do something
odd or add non-general special features you're probably best to wrap this
and produce your own subclass.

=head1 OPTIONS

See the C<Module::Pluggable> docs.

=head1 AUTHOR

Simon Wistow <simon@thegestalt.org>

=head1 COPYING

Copyright, 2006 Simon Wistow

Distributed under the same terms as Perl itself.

=head1 BUGS

None known.

=head1 SEE ALSO

L<Module::Pluggable>

=cut 

