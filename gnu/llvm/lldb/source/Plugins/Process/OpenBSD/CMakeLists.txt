add_lldb_library(lldbPluginProcessOpenBSD PLUGIN
  NativeProcessOpenBSD.cpp
  NativeRegisterContextOpenBSD.cpp
  NativeRegisterContextOpenBSD_x86_64.cpp
  NativeRegisterContextOpenBSD_arm64.cpp
  NativeThreadOpenBSD.cpp

  LINK_LIBS
    lldbCore
    lldbHost
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbPluginProcessPOSIX
    lldbPluginProcessUtility
  LINK_COMPONENTS
    Support
  )
