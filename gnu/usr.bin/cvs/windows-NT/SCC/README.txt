This is an experimental attempt to interface to the SCC API.

Note that this code doesn't (yet) do anything useful; this file
is currently for people who want to help hack on our SCC interface,
not people who want to use it.

To install it, build scc.dll and then add the following
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
under the "Tools" menu.

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

* Microsoft Developer Studio 4.x Professional (not Standard, not 2.x)
* Powersoft's Optima++
* CodeWright editor
