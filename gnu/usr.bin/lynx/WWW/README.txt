                                                                        Read Me
                      WORLDWIDEWEB CERN-DISTRIBUTED CODE
                                       
   See the CERN copyright[1] .  This is the README file which you get when you
   unwrap one of our tar files. These files contain information about
   hypertext, hypertext systems, and the WorldWideWeb project. If you have
   taken this with a .tar file, you will have only a subset of the files.
   
   THIS FILE IS A VERY ABRIDGED VERSION OF THE INFORMATION AVAILABLE ON THE
   WEB.   IF IN DOUBT, READ THE WEB DIRECTLY. If you have not got ANY browser
   installed yet, do this by telnet to info.cern.ch (no username or password).
   
   Files from info.cern.ch are also mirrored on ftp.ripe.net.
   
Archive Directory structure

   Under /pub/www[2] , besides this README file, you'll find bin[3] , src[4]
   and doc[5] directories.  The main archives are as follows:
   
  bin/xxx/bbbb            Executable binaries of program bbbb for system xxx.
                         Check what's there before you bother compiling. (Note
                         HP700/8800 series is "snake")
                         
  bin/next/WorldWideWeb_v.vv.tar.Z
                         The Hypertext Browser/editor for the NeXT -- binary.
                         
  src/WWWLibrary_v.vv.tar.Z
                          The W3 Library. All source, and Makefiles for
                         selected systems.
                         
  src/WWWLineMode_v.vv.tar.Z
                          The Line mode browser - all source, and Makefiles for
                         selected systems. Requires the Library[6] .
                         
  src/WWWDaemon_v.vv.tar.Z
                          The HTTP daemon, and WWW-WAIS  gateway programs.
                         Source.  Requires the Library.
                         
  src/WWWMailRobot_v.vv.tar.Z
                          The Mail Robot.
                         
  doc/WWWBook.tar.Z       A snapshot of our internal documentation - we prefer
                         you to access this on line -- see warnings below.
                         
Basic WWW software installation from source

   This applies to the line mode client and the server.  Below, $prod means
   LineMode or Daemon depending on which you are building.
   
  GENERATED DIRECTORY STRUCTURE
  
   The tar files are all designed to be unwrapped in the same (this) directory.
   They create different parts of a common directory tree under that directory.
   There may be some duplication. They also generate a few files in this
   directory: README.*, Copyright.*, and some installation instructions (.txt).
   
   The directory structure is, for product $prod  and machine $WWW_MACH
   
  WWW/$prod/Implementation
                          Source files for a given product
                         
  WWW/$prod/Implementation/CommonMakefile
                         The machine-independent parts of the Makefile for this
                         product
                         

                                                                Read Me (65/66)
  WWW/$prod/$WWW_MACH/    Area for compiling for a given system
                         
  WWW/All/$WWW_MACH/Makefile.include
                         The machine-dependent parts of the makefile for any
                         product
                         
  WWW/All/Implementation/Makefile.product
                         A makefile which includes both parts above and so can
                         be used from any product, any machine.
                         
  COMPILATION ON ALREADY SUPPORTED PLATFORMS
  
   You must get the WWWLibrary tar file as well as the products you want and
   unwrap them all from the same directory.
   
   You must define the environmant variable WWW_MACH to be the architecure of
   your machine (sun4, decstation, rs6000, sgi, snake, etc)
   
   In directory WWW, type BUILD.
   
  COMPILATION ON NEW PLATFORMS
  
   If your machine is not on the list:
   
      Make up a new subdirectory of that name under WWW/$prod and WWW/All,
      copying the contents of a basically similar architecture's directory.
      
      Check the  WWW/All/$WWW_MACH/Makefile.include for suitable directory and
      flag definitions.
      
      Check the file tcp.h for the system-specific include file coordinates,
      etc.
      
      Send any changes you have to make back to www-request@info.cern.ch for
      inclusion into future releases.
      
      Once you have this set up, type BUILD.
      
NeXTStep Browser/Editor

   The browser for the NeXT is those files contained in the application
   directory WWW/Next/Implementation/WorldWideWeb.app and is compiled. When you
   install the app, you may want to configure the default page,
   WorldWideWeb.app/default.html. These must point to some useful information!
   You should keep it up to date with pointers to info on your site and
   elsewhere. If you use the CERN home page note there is a link at the bottom
   to the master copy on our server.   You should set up the address of your
   local news server with
   
                      dwrite WorldWideWeb NewsHost  news

   replacing the last word with the actual address of your news host. See
   Installation instructions[7] .
   
Line Mode browser

   Binaries of this for some systems are available in /pub/www/bin/ . The
   binaries can be picked up, set executable, and run immediately.
   
   If there is no binary, see "Installation from source" above.
   
    (See Installation notes[8] ).  Do the same thing (in the same directory) to
   the WWWLibrary_v.cc.tar.Z file to get the common library.
   

                                                               Read Me (65/130)
   You will have an ASCII printable manual in the file
   WWW/LineMode/Defaults/line-mode-guide.txt which you can print out at this
   stage. This is a frozen copy of some of the online documentation.
   
   Whe you install the browser, you may configure a default page. This is
   /usr/local/lib/WWW/default.html for the line mode browser. This must point
   to some useful information! You should keep it up to date with pointers to
   info on your site and elsewhere. If you use the CERN home page note there is
   a link at the bottom to the master copy on our server.
   
   Some basic documentation on the browser is delivered with the home page in
   the directory WWW/LineMode/Defaults. A separate tar file of that directory
   (WWWLineModeDefaults.tar.Z) is available if you just want to update that.
   
   The rest of the documentation is in hypertext, and so wil be readable most
   easily with a browser. We suggest that after installing the browser, you
   browse through the basic documentation so that you are aware of the options
   and customisation possibilities for example.
   
Server

   The server can be run very simply under the internet  daemon, to export a
   file directory tree as a browsable hypertext tree.  Binaries are avilable
   for some platofrms, otherwise follow instructions above for compiling and
   then go on to " Installing the basic W3 server[9] ".
   
XMosaic

   XMosaic is an X11/Motif  W3 browser.
   
   The sources and binaries are distributed separately from
   FTP.NCSA.UIUC.EDU[10] , in  /Web/xmosaic[11] .  Binaries are available for
   some platforms.  If you have to build from source, check the README in the
   distribution.
   
   The binaries can be picked up, uncompressed, set "executable" and run
   immediately.
   
Viola browser for X11

   Viola is an X11 application for reading global hypertext.  If a binary is
   available from your machine, in /pub/www/bin/.../viola*, then take that and
   also the Viola "apps" tar file which contains the scripts you will need.
   
   To generate this from source, you will need both the W3 library and the
   Viola source files.  There is an Imakefile with the viola source directory.
   You will need to generate the XPA and XPM libraries and the W3 library
   befere you make viola itself.
   
Documentation

   In the /pub/www/doc[12] directory are a number articles, preprints and
   guides on the web.
   
   See the online WWW bibliography[13] for a list of these and other articles,
   books, etc. and also the list of WWW Manuals[14] available in text and
   postscript form.
   
General

   Your comments will of course be most appreciated, on code, or information on
   the web which is out of date or misleading. If you write your own hypertext
   and make it available by anonymous ftp or using a server, tell us and we'll
   put some pointers to it in ours. Thus spreads the web...

                                                               Read Me (66/195)
                                                                Tim Berners-Lee
                                                                               
                                                           WorldWideWeb project
                                                                               
                                              CERN, 1211 Geneva 23, Switzerland
                                                                               
          Tel: +41 22 767 3755; Fax: +41 22 767 7155; email: timbl@info.cern.ch
                                                                               
   
