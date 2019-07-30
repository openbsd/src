//===-- BreakpointResolverFileLine.h ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_BreakpointResolverFileLine_h_
#define liblldb_BreakpointResolverFileLine_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/BreakpointResolver.h"

namespace lldb_private {

//----------------------------------------------------------------------
/// @class BreakpointResolverFileLine BreakpointResolverFileLine.h
/// "lldb/Breakpoint/BreakpointResolverFileLine.h"
/// @brief This class sets breakpoints by file and line.  Optionally, it will
/// look for inlined
/// instances of the file and line specification.
//----------------------------------------------------------------------

class BreakpointResolverFileLine : public BreakpointResolver {
public:
  BreakpointResolverFileLine(Breakpoint *bkpt, const FileSpec &resolver,
                             uint32_t line_no, lldb::addr_t m_offset,
                             bool check_inlines, bool skip_prologue,
                             bool exact_match);

  static BreakpointResolver *
  CreateFromStructuredData(Breakpoint *bkpt,
                           const StructuredData::Dictionary &data_dict,
                           Status &error);

  StructuredData::ObjectSP SerializeToStructuredData() override;

  ~BreakpointResolverFileLine() override;

  Searcher::CallbackReturn SearchCallback(SearchFilter &filter,
                                          SymbolContext &context, Address *addr,
                                          bool containing) override;

  Searcher::Depth GetDepth() override;

  void GetDescription(Stream *s) override;

  void Dump(Stream *s) const override;

  /// Methods for support type inquiry through isa, cast, and dyn_cast:
  static inline bool classof(const BreakpointResolverFileLine *) {
    return true;
  }
  static inline bool classof(const BreakpointResolver *V) {
    return V->getResolverID() == BreakpointResolver::FileLineResolver;
  }

  lldb::BreakpointResolverSP CopyForBreakpoint(Breakpoint &breakpoint) override;

protected:
  void FilterContexts(SymbolContextList &sc_list);

  friend class Breakpoint;
  FileSpec m_file_spec;   // This is the file spec we are looking for.
  uint32_t m_line_number; // This is the line number that we are looking for.
  bool m_inlines; // This determines whether the resolver looks for inlined
                  // functions or not.
  bool m_skip_prologue;
  bool m_exact_match;

private:
  DISALLOW_COPY_AND_ASSIGN(BreakpointResolverFileLine);
};

} // namespace lldb_private

#endif // liblldb_BreakpointResolverFileLine_h_
