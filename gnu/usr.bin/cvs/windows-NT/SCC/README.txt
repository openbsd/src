This is an experimental attempt to interface to the SCC API.

Note that this code doesn't (yet) do anything useful; this file
is currently for people who want to hack on the SCC interface,
not people who want a plug-in integration between any particular pair
of packages.

To install the test DLL, build scc.dll and then add the following
to the registry using the registry editor:

key/value                what to put there
HKEY_LOCAL_MACHINE
  SOFTWARE
    CVS
      CVS
        SCCServerName    Concurrent Versions System
        SCCServerPath    <full pathname of scc.dll>
    SourceCodeControlProvider
      ProviderRegKey     "SOFTWARE\CVS\CVS"
      InstalledSCCProviders
        Concurrent Versions System   "SOFTWARE\CVS\CVS"

Note that ProviderRegKey is the default source control
system, and InstalledSCCProviders list _all_ installed
source control systems.  A development environment is allowed
to use either or both, so you should set both of them.

Note also that we are using "CVS" as the supplier of CVS.
CVS is not owned by any one company, so CVS seems like the
most appropriate string to put there.

If you do this right, then "Source Control" should appear
under the "Tools" menu (for Visual C++ 4.x; the details of course may
vary for other development environments).

NOW WHAT?

Well, I haven't yet figured out _all_ the different ways
that projects work at the SCC level.  But here is what I
have done which has worked.  SPECIAL NOTE: many paths are
hardcoded in scc.c, so you will need to fix that or put
things the same place I did.  As you try the following you
will want to follow along in d:\debug.scc.

* Create a dummy project in d:\sccwork.
* On the Tools/Source Control menu, select "Share from CVS..."
* This will cause SccAddFromScc to be called, which will
  claim there are two files, foo.c and bar.c, which should
  appear as source controlled (grey) files in the file
  listing.
* Now select one of the files and pick "Get Latest Version..."
  from Tools/Source Control.  You'll get a cheezy dialog (we
  need to see why it is cheezy--by that I mean the size and
  placement are funny), and if you say OK, then SccGet will
  get called (which doesn't currently do anything).

TOOLS IMPLEMENTING THE SCC

I'm not sure whether we'll want to try to make this a comprehensive
list, but at least for the moment it seems worthwhile to list a few of
the programs which implement the Integrated Development Environment
(IDE) side of the SCC.  Some of this information is based on rumor or,
worse yet, usenet posting, so it probably should be verified before
relying on it.

* Microsoft Developer Studio 4.x Professional (not Standard, not 2.x).
* Microsoft Access V7.0
* Powersoft's Optima++, PowerJ, and Power++
  (not sure which versions, but this information was added in 1997 if
   that helps.  Someone on usenet reports 32 bit Powerbuilder version
   5.03 but not version 4, version 5.0, or 16 bit Powerbuilder.).
* Premia's CodeWright editor 
  (versions 5.00b and 5.00c; not sure about older versions).
* HAHTsite (not sure what versions).

SPECIFICATIONS OR OTHER DOCUMENTS DESCRIBING THE SCC

The only publicly available document which we are aware of is pubscc.h
in this directory.  This should be sufficient to get a start at
playing around with the SCC, and if you have done that and then
proceed to run into those areas which pubscc.h does not document well,
you are encouraged to send mail to bug-cvs@gnu.org with your
questions.

OTHER INTERFACES

There are other interfaces which interface between a development
environment (or other front-end) and a source control system.  That
is, in general terms they provide somewhat the function of the SCC,
although they may be at a somewhat different level and systems may
support/use several interfaces rather than it being an either/or thing.

If you know of other interfaces which should be added here I guess the
best place to make suggestions is bug-cvs@gnu.org (although
the following list is not intended to be particularly CVS-centric).

* The CVS remote protocol is documented in doc/cvsclient.texi in the
CVS distribution and has at least 2 implementations of the client
(jCVS and CVS command line client), in addition to having been
implemented at least once by a special-purpose perl script.

* Microsoft's OLE Automation interface.  The spec is available for
download at http://www.microsoft.com/ssafe.  I'm not sure whether this
has been implemented by other source control systems.  Metrowerks
implements this via a module which speaks the Metrowerks API out one
end and the OLE Automation interface out the other (the module runs on
Windows, not Mac).

* Symantec's Visual Cafe interface.

* Metrowerks publishes and implements the CodeWarrior IDE Version
Control System API.  I think maybe the way to get a copy of the spec
is as part of CodeWarrior but I'm not completely clear on that.

For (some) more details on these interfaces, and others, see
    http://www.cyclic.com/cvs/dev-int.html
