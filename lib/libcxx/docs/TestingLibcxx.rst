==============
Testing libc++
==============

.. contents::
  :local:

Getting Started
===============

libc++ uses LIT to configure and run its tests. The primary way to run the
libc++ tests is by using make check-libcxx. However since libc++ can be used
in any number of possible configurations it is important to customize the way
LIT builds and runs the tests. This guide provides information on how to use
LIT directly to test libc++.

Please see the `Lit Command Guide`_ for more information about LIT.

.. _LIT Command Guide: http://llvm.org/docs/CommandGuide/lit.html

Setting up the Environment
--------------------------

After building libc++ you must setup your environment to test libc++ using
LIT.

#. Create a shortcut to the actual lit executable so that you can invoke it
   easily from the command line.

   .. code-block:: bash

     $ alias lit='python path/to/llvm/utils/lit/lit.py'

#. Tell LIT where to find your build configuration.

   .. code-block:: bash

     $ export LIBCXX_SITE_CONFIG=path/to/build-libcxx/test/lit.site.cfg

Example Usage
-------------

Once you have your environment set up and you have built libc++ you can run
parts of the libc++ test suite by simply running `lit` on a specified test or
directory. For example:

.. code-block:: bash

  $ cd path/to/src/libcxx
  $ lit -sv test/std/re # Run all of the std::regex tests
  $ lit -sv test/std/depr/depr.c.headers/stdlib_h.pass.cpp # Run a single test
  $ lit -sv test/std/atomics test/std/threads # Test std::thread and std::atomic

Sometimes you'll want to change the way LIT is running the tests. Custom options
can be specified using the `--param=<name>=<val>` flag. The most common option
you'll want to change is the standard dialect (ie -std=c++XX). By default the
test suite will select the newest C++ dialect supported by the compiler and use
that. However if you want to manually specify the option like so:

.. code-block:: bash

  $ lit -sv test/std/containers # Run the tests with the newest -std
  $ lit -sv --param=std=c++03 test/std/containers # Run the tests in C++03

Occasionally you'll want to add extra compile or link flags when testing.
You can do this as follows:

.. code-block:: bash

  $ lit -sv --param=compile_flags='-Wcustom-warning'
  $ lit -sv --param=link_flags='-L/custom/library/path'

Some other common examples include:

.. code-block:: bash

  # Specify a custom compiler.
  $ lit -sv --param=cxx_under_test=/opt/bin/g++ test/std

  # Enable warnings in the test suite
  $ lit -sv --param=enable_warnings=true test/std

  # Use UBSAN when running the tests.
  $ lit -sv --param=use_sanitizer=Undefined


LIT Options
===========

:program:`lit` [*options*...] [*filenames*...]

Command Line Options
--------------------

To use these options you pass them on the LIT command line as --param NAME or
--param NAME=VALUE. Some options have default values specified during CMake's
configuration. Passing the option on the command line will override the default.

.. program:: lit

.. option:: --cxx_under_test=<path/to/compiler>

  Specify the compiler used to build the tests.

.. option:: std=<standard version>

  **Values**: c++98, c++03, c++11, c++14, c++1z

  Change the standard version used when building the tests.

.. option:: --libcxx_site_config=<path/to/lit.site.cfg>

  Specify the site configuration to use when running the tests.  This option
  overrides the enviroment variable LIBCXX_SITE_CONFIG.

.. option:: --libcxx_headers=<path/to/headers>

  Specify the libc++ headers that are tested. By default the headers in the
  source tree are used.

.. option:: --cxx_library_root=<path/to/lib/>

  Specify the directory of the libc++ library to be tested. By default the
  library folder of the build directory is used. This option cannot be used
  when use_system_lib is provided.


.. option:: --cxx_runtime_root=<path/to/lib/>

  Specify the directory of the libc++ library to use at runtime. This directory
  is not added to the linkers search path. This can be used to compile tests
  against one version of libc++ and run them using another. The default value
  for this option is `cxx_library_root`. This option cannot be used
  when use_system_lib is provided.

.. option:: --use_system_lib=<bool>

  **Default**: False

  Enable or disable testing against the installed version of libc++ library.
  Note: This does not use the installed headers.

.. option:: --use_lit_shell=<bool>

  Enable or disable the use of LIT's internal shell in ShTests. If the
  environment variable LIT_USE_INTERNAL_SHELL is present then that is used as
  the default value. Otherwise the default value is True on Windows and False
  on every other platform.

.. option:: --no_default_flags=<bool>

  **Default**: False

  Disable all default compile and link flags from being added. When this
  option is used only flags specified using the compile_flags and link_flags
  will be used.

.. option:: --compile_flags="<list-of-args>"

  Specify additional compile flags as a space delimited string.
  Note: This options should not be used to change the standard version used.

.. option:: --link_flags="<list-of-args>"

  Specify additional link flags as a space delimited string.

.. option:: --debug_level=<level>

  **Values**: 0, 1

  Enable the use of debug mode. Level 0 enables assertions and level 1 enables
  assertions and debugging of iterator misuse.

.. option:: use_sanitizer=<sanitizer name>

  **Values**: Memory, MemoryWithOrigins, Address, Undefined

  Run the tests using the given sanitizer. If LLVM_USE_SANITIZER was given when
  building libc++ then that sanitizer will be used by default.

.. option:: color_diagnostics

  Enable the use of colorized compile diagnostics. If the color_diagnostics
  option is specified or the environment variable LIBCXX_COLOR_DIAGNOSTICS is
  present then color diagnostics will be enabled.


Environment Variables
---------------------

.. envvar:: LIBCXX_SITE_CONFIG=<path/to/lit.site.cfg>

  Specify the site configuration to use when running the tests.
  Also see :option:`libcxx_site_config`.

.. envvar:: LIBCXX_COLOR_DIAGNOSTICS

  If ``LIBCXX_COLOR_DIAGNOSTICS`` is defined then the test suite will attempt
  to use color diagnostic outputs from the compiler.
  Also see :option:`color_diagnostics`.
