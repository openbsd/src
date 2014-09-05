#!perl
#===============================================================================
#
# Makefile.PL
#
# DESCRIPTION
#   Makefile creation script.
#
# COPYRIGHT
#   Copyright (C) 2014 Steve Hay.  All rights reserved.
#
# LICENCE
#   You may distribute under the terms of either the GNU General Public License
#   or the Artistic License, as specified in the LICENCE file.
#
#===============================================================================

use 5.008001;

use strict;
use warnings;

use ExtUtils::MakeMaker 6.64;
use ExtUtils::MakeMaker qw(WriteMakefile);

#===============================================================================
# INITIALIZATION
#===============================================================================

our($CfgFile, $CfgPath);

BEGIN {
    $CfgFile = 'libnet.cfg';
    $CfgPath = "Net/$CfgFile";
}

#===============================================================================
# MAIN PROGRAM
#===============================================================================

MAIN: {
    my %prereq_pms = ();
    $prereq_pms{'Convert::EBCDIC'} = '0.06' if $^O eq 'os390';

    WriteMakefile(
        NAME     => 'Net',
        DISTNAME => 'libnet',
        ABSTRACT => 'Collection of network protocol modules',
        AUTHOR   => 'Graham Barr <gbarr@pobox.com>, Steve Hay <shay@cpan.org>',
        LICENSE  => 'perl_5',
        VERSION  => '1.27',

        META_MERGE => {
            'meta-spec' => {
                version => 2
            },

            resources => {
                repository => {
                    type => 'git',
                    url  => 'https://github.com/steve-m-hay/perl-libnet.git'
                }
            },

            optional_features => {
                APOP => {
                    description => 'APOP support',
                    prereqs => {
                        runtime => {
                            requires => {
                                'Digest::MD5' => '0'
                            }
                        }
                    }
                },

                AUTH => {
                    description => 'AUTH support',
                    prereqs => {
                        runtime => {
                            requires => {
                                'Authen::SASL' => '0',
                                'MIME::Base64' => '0'
                            }
                        }
                    }
                }
            }
        },

        MIN_PERL_VERSION => '5.008001',

        CONFIGURE_REQUIRES => {
            'ExtUtils::MakeMaker' => '6.64',
            'Getopt::Std'         => '0',
            'IO:File'             => '0',
            'perl'                => '5.008001',
            'strict'              => '0',
            'vars'                => '0',
            'warnings'            => '0'
        },

        TEST_REQUIRES => {
            'Cwd' => '0'
        },

        PREREQ_PM => {
            %prereq_pms,
            'Carp'           => '0',
            'Errno'          => '0',
            'Exporter'       => '0',
            'Fcntl'          => '0',
            'File::Basename' => '0',
            'FileHandle'     => '0',
            'IO::Select'     => '0',
            'IO::Socket'     => '1.05',
            'POSIX'          => '0',
            'Socket'         => '1.3',
            'Symbol'         => '0',
            'Time::Local'    => '0',
            'strict'         => '0',
            'vars'           => '0'
        },

        INSTALLDIRS => 'perl',

        realclean => {
            FILES => $CfgFile
        },

        dist => {
            PREOP   => 'find $(DISTVNAME) -type d -print|xargs chmod 0755 && ' .
                       'find $(DISTVNAME) -type f -print|xargs chmod 0644',
            TO_UNIX => 'find $(DISTVNAME) -type f -print|xargs dos2unix'
        }
    );
}

#===============================================================================
# MAKEMAKER OVERRIDES
#===============================================================================

sub MY::post_initialize {
    my $self = shift;
    return '' if $self->{PERL_CORE};
    system(($^O eq 'VMS' ? 'mcr ': ()), $^X, 'Configure') unless -f $CfgFile;
    $self->{PM}{$CfgFile} = $self->catfile('$(INST_LIBDIR)',$CfgPath);
    return '';
}

#===============================================================================
