use 5.008;
use ExtUtils::MakeMaker;
use ExtUtils::Constant 0.11 'WriteConstants';
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'XS::APItest',
    'VERSION_FROM'	=> 'APItest.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'APItest.pm', # retrieve abstract from module
       AUTHOR     => 'Tim Jenness <t.jenness@jach.hawaii.edu>, Christian Soeller <csoelle@mph.auckland.ac.nz>, Hugo van der Sanden <hv@crypt.compulink.co.uk>') : ()),
    'C'                 => ['exception.c', 'core.c', 'notcore.c'],
    'OBJECT'            => '$(BASEEXT)$(OBJ_EXT) $(O_FILES)',
    'LIBS'		=> [''], # e.g., '-lm'
    'DEFINE'		=> '', # e.g., '-DHAVE_SOMETHING'
    'INC'		=> '-I.', # e.g., '-I. -I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # 'OBJECT'		=> '$(O_FILES)', # link all the C files too
    realclean => {FILES	=> 'const-c.inc const-xs.inc'},
);

WriteConstants(
    PROXYSUBS => 1,
    NAME => 'XS::APItest',
    NAMES => [qw(HV_DELETE HV_DISABLE_UVAR_XKEY HV_FETCH_ISSTORE
		 HV_FETCH_ISEXISTS HV_FETCH_LVALUE HV_FETCH_JUST_SV
		 G_SCALAR G_ARRAY G_VOID G_DISCARD G_EVAL G_NOARGS
		 G_KEEPERR G_NODEBUG G_METHOD G_FAKINGEVAL),
	      {name=>"G_WANT", default=>["IV", "G_ARRAY|G_VOID"]}],
	       );

sub MY::install { "install ::\n"  };
