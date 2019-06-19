//===- FileSystemStatCache.h - Caching for 'stat' calls ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Defines the FileSystemStatCache interface.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_BASIC_FILESYSTEMSTATCACHE_H
#define LLVM_CLANG_BASIC_FILESYSTEMSTATCACHE_H

#include "clang/Basic/LLVM.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/FileSystem.h"
#include <cstdint>
#include <ctime>
#include <memory>
#include <string>
#include <utility>

namespace clang {

namespace vfs {

class File;
class FileSystem;

} // namespace vfs

// FIXME: should probably replace this with vfs::Status
struct FileData {
  std::string Name;
  uint64_t Size = 0;
  time_t ModTime = 0;
  llvm::sys::fs::UniqueID UniqueID;
  bool IsDirectory = false;
  bool IsNamedPipe = false;
  bool InPCH = false;

  // FIXME: remove this when files support multiple names
  bool IsVFSMapped = false;

  FileData() = default;
};

/// Abstract interface for introducing a FileManager cache for 'stat'
/// system calls, which is used by precompiled and pretokenized headers to
/// improve performance.
class FileSystemStatCache {
  virtual void anchor();

protected:
  std::unique_ptr<FileSystemStatCache> NextStatCache;

public:
  virtual ~FileSystemStatCache() = default;

  enum LookupResult {
    /// We know the file exists and its cached stat data.
    CacheExists,

    /// We know that the file doesn't exist.
    CacheMissing
  };

  /// Get the 'stat' information for the specified path, using the cache
  /// to accelerate it if possible.
  ///
  /// \returns \c true if the path does not exist or \c false if it exists.
  ///
  /// If isFile is true, then this lookup should only return success for files
  /// (not directories).  If it is false this lookup should only return
  /// success for directories (not files).  On a successful file lookup, the
  /// implementation can optionally fill in \p F with a valid \p File object and
  /// the client guarantees that it will close it.
  static bool get(StringRef Path, FileData &Data, bool isFile,
                  std::unique_ptr<vfs::File> *F, FileSystemStatCache *Cache,
                  vfs::FileSystem &FS);

  /// Sets the next stat call cache in the chain of stat caches.
  /// Takes ownership of the given stat cache.
  void setNextStatCache(std::unique_ptr<FileSystemStatCache> Cache) {
    NextStatCache = std::move(Cache);
  }

  /// Retrieve the next stat call cache in the chain.
  FileSystemStatCache *getNextStatCache() { return NextStatCache.get(); }

  /// Retrieve the next stat call cache in the chain, transferring
  /// ownership of this cache (and, transitively, all of the remaining caches)
  /// to the caller.
  std::unique_ptr<FileSystemStatCache> takeNextStatCache() {
    return std::move(NextStatCache);
  }

protected:
  // FIXME: The pointer here is a non-owning/optional reference to the
  // unique_ptr. Optional<unique_ptr<vfs::File>&> might be nicer, but
  // Optional needs some work to support references so this isn't possible yet.
  virtual LookupResult getStat(StringRef Path, FileData &Data, bool isFile,
                               std::unique_ptr<vfs::File> *F,
                               vfs::FileSystem &FS) = 0;

  LookupResult statChained(StringRef Path, FileData &Data, bool isFile,
                           std::unique_ptr<vfs::File> *F, vfs::FileSystem &FS) {
    if (FileSystemStatCache *Next = getNextStatCache())
      return Next->getStat(Path, Data, isFile, F, FS);

    // If we hit the end of the list of stat caches to try, just compute and
    // return it without a cache.
    return get(Path, Data, isFile, F, nullptr, FS) ? CacheMissing : CacheExists;
  }
};

/// A stat "cache" that can be used by FileManager to keep
/// track of the results of stat() calls that occur throughout the
/// execution of the front end.
class MemorizeStatCalls : public FileSystemStatCache {
public:
  /// The set of stat() calls that have been seen.
  llvm::StringMap<FileData, llvm::BumpPtrAllocator> StatCalls;

  using iterator =
      llvm::StringMap<FileData, llvm::BumpPtrAllocator>::const_iterator;

  iterator begin() const { return StatCalls.begin(); }
  iterator end() const { return StatCalls.end(); }

  LookupResult getStat(StringRef Path, FileData &Data, bool isFile,
                       std::unique_ptr<vfs::File> *F,
                       vfs::FileSystem &FS) override;
};

} // namespace clang

#endif // LLVM_CLANG_BASIC_FILESYSTEMSTATCACHE_H
