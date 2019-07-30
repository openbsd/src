//===--------------------- TildeExpressionResolver.h ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_UTILITY_TILDE_EXPRESSION_RESOLVER_H
#define LLDB_UTILITY_TILDE_EXPRESSION_RESOLVER_H

#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSet.h"

namespace llvm {
template <typename T> class SmallVectorImpl;
}

namespace lldb_private {
class TildeExpressionResolver {
public:
  virtual ~TildeExpressionResolver();

  /// \brief Resolve a Tilde Expression contained according to bash rules.
  ///
  /// \param Expr Contains the tilde expression to resolve.  A valid tilde
  ///             expression must begin with a tilde and contain only non
  ///             separator characters.
  ///
  /// \param Output Contains the resolved tilde expression, or the original
  ///               input if the tilde expression could not be resolved.
  ///
  /// \returns true if \p Expr was successfully resolved, false otherwise.
  virtual bool ResolveExact(llvm::StringRef Expr,
                            llvm::SmallVectorImpl<char> &Output) = 0;

  /// \brief Auto-complete a tilde expression with all matching values.
  ///
  /// \param Expr Contains the tilde expression prefix to resolve.  See
  ///             ResolveExact() for validity rules.
  ///
  /// \param Output Contains all matching home directories, each one
  ///               itself unresolved (i.e. you need to call ResolveExact
  ///               on each item to turn it into a real path).
  ///
  /// \returns true if there were any matches, false otherwise.
  virtual bool ResolvePartial(llvm::StringRef Expr,
                              llvm::StringSet<> &Output) = 0;

  /// \brief Resolve an entire path that begins with a tilde expression,
  /// replacing the username portion with the matched result.
  bool ResolveFullPath(llvm::StringRef Expr,
                       llvm::SmallVectorImpl<char> &Output);
};

class StandardTildeExpressionResolver : public TildeExpressionResolver {
public:
  bool ResolveExact(llvm::StringRef Expr,
                    llvm::SmallVectorImpl<char> &Output) override;
  bool ResolvePartial(llvm::StringRef Expr, llvm::StringSet<> &Output) override;
};
}

#endif // #ifndef LLDB_UTILITY_TILDE_EXPRESSION_RESOLVER_H
