use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.

WriteMakefile(
    'NAME'		=> 'threads::shared',
    'VERSION_FROM'	=> 'shared.pm', # finds $VERSION
	      'PM' => {
		  'shared.pm'    => '$(INST_LIBDIR)/shared.pm',
	      },
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
      (ABSTRACT_FROM => 'shared.pm', # retrieve abstract from module
       AUTHOR     => 'Arthur Bergman <arthur@contiller.se>') : ()),
    'MAN3PODS'		=> {},  # Pods will be built by installman
    'LIBS'		=> [''], # e.g., '-lm'
    'DEFINE'		=> '', # e.g., '-DHAVE_SOMETHING'
	# Insert -I. if you add *.h files later:
    'INC'		=> '', # e.g., '-I/usr/include/other'
	# Un-comment this if you add C files to link with later:
    # 'OBJECT'		=> '$(O_FILES)', # link all the C files too

);
