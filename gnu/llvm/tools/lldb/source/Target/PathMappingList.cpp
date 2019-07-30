//===-- PathMappingList.cpp -------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// C Includes
// C++ Includes
#include <climits>
#include <cstring>

// Other libraries and framework includes
// Project includes
#include "lldb/Host/PosixApi.h"
#include "lldb/Target/PathMappingList.h"
#include "lldb/Utility/FileSpec.h"
#include "lldb/Utility/Status.h"
#include "lldb/Utility/Stream.h"

using namespace lldb;
using namespace lldb_private;

//----------------------------------------------------------------------
// PathMappingList constructor
//----------------------------------------------------------------------
PathMappingList::PathMappingList()
    : m_pairs(), m_callback(nullptr), m_callback_baton(nullptr), m_mod_id(0) {}

PathMappingList::PathMappingList(ChangedCallback callback, void *callback_baton)
    : m_pairs(), m_callback(callback), m_callback_baton(callback_baton),
      m_mod_id(0) {}

PathMappingList::PathMappingList(const PathMappingList &rhs)
    : m_pairs(rhs.m_pairs), m_callback(nullptr), m_callback_baton(nullptr),
      m_mod_id(0) {}

const PathMappingList &PathMappingList::operator=(const PathMappingList &rhs) {
  if (this != &rhs) {
    m_pairs = rhs.m_pairs;
    m_callback = nullptr;
    m_callback_baton = nullptr;
    m_mod_id = rhs.m_mod_id;
  }
  return *this;
}

PathMappingList::~PathMappingList() = default;

void PathMappingList::Append(const ConstString &path,
                             const ConstString &replacement, bool notify) {
  ++m_mod_id;
  m_pairs.push_back(pair(path, replacement));
  if (notify && m_callback)
    m_callback(*this, m_callback_baton);
}

void PathMappingList::Append(const PathMappingList &rhs, bool notify) {
  ++m_mod_id;
  if (!rhs.m_pairs.empty()) {
    const_iterator pos, end = rhs.m_pairs.end();
    for (pos = rhs.m_pairs.begin(); pos != end; ++pos)
      m_pairs.push_back(*pos);
    if (notify && m_callback)
      m_callback(*this, m_callback_baton);
  }
}

void PathMappingList::Insert(const ConstString &path,
                             const ConstString &replacement, uint32_t index,
                             bool notify) {
  ++m_mod_id;
  iterator insert_iter;
  if (index >= m_pairs.size())
    insert_iter = m_pairs.end();
  else
    insert_iter = m_pairs.begin() + index;
  m_pairs.insert(insert_iter, pair(path, replacement));
  if (notify && m_callback)
    m_callback(*this, m_callback_baton);
}

bool PathMappingList::Replace(const ConstString &path,
                              const ConstString &replacement, uint32_t index,
                              bool notify) {
  iterator insert_iter;
  if (index >= m_pairs.size())
    return false;
  ++m_mod_id;
  m_pairs[index] = pair(path, replacement);
  if (notify && m_callback)
    m_callback(*this, m_callback_baton);
  return true;
}

bool PathMappingList::Remove(size_t index, bool notify) {
  if (index >= m_pairs.size())
    return false;

  ++m_mod_id;
  iterator iter = m_pairs.begin() + index;
  m_pairs.erase(iter);
  if (notify && m_callback)
    m_callback(*this, m_callback_baton);
  return true;
}

// For clients which do not need the pair index dumped, pass a pair_index >= 0
// to only dump the indicated pair.
void PathMappingList::Dump(Stream *s, int pair_index) {
  unsigned int numPairs = m_pairs.size();

  if (pair_index < 0) {
    unsigned int index;
    for (index = 0; index < numPairs; ++index)
      s->Printf("[%d] \"%s\" -> \"%s\"\n", index,
                m_pairs[index].first.GetCString(),
                m_pairs[index].second.GetCString());
  } else {
    if (static_cast<unsigned int>(pair_index) < numPairs)
      s->Printf("%s -> %s", m_pairs[pair_index].first.GetCString(),
                m_pairs[pair_index].second.GetCString());
  }
}

void PathMappingList::Clear(bool notify) {
  if (!m_pairs.empty())
    ++m_mod_id;
  m_pairs.clear();
  if (notify && m_callback)
    m_callback(*this, m_callback_baton);
}

bool PathMappingList::RemapPath(const ConstString &path,
                                ConstString &new_path) const {
  const char *path_cstr = path.GetCString();
  // CLEANUP: Convert this function to use StringRefs internally instead
  // of raw c-strings.
  if (!path_cstr)
    return false;

  const_iterator pos, end = m_pairs.end();
  for (pos = m_pairs.begin(); pos != end; ++pos) {
    const size_t prefixLen = pos->first.GetLength();

    if (::strncmp(pos->first.GetCString(), path_cstr, prefixLen) == 0) {
      std::string new_path_str(pos->second.GetCString());
      new_path_str.append(path.GetCString() + prefixLen);
      new_path.SetCString(new_path_str.c_str());
      return true;
    }
  }
  return false;
}

bool PathMappingList::RemapPath(llvm::StringRef path,
                                std::string &new_path) const {
  if (m_pairs.empty() || path.empty())
    return false;

  const_iterator pos, end = m_pairs.end();
  for (pos = m_pairs.begin(); pos != end; ++pos) {
    if (!path.consume_front(pos->first.GetStringRef()))
      continue;

    new_path = pos->second.GetStringRef();
    new_path.append(path);
    return true;
  }
  return false;
}

bool PathMappingList::ReverseRemapPath(const ConstString &path,
                                       ConstString &new_path) const {
  const char *path_cstr = path.GetCString();
  if (!path_cstr)
    return false;

  for (const auto &it : m_pairs) {
    // FIXME: This should be using FileSpec API's to do the path appending.
    const size_t prefixLen = it.second.GetLength();
    if (::strncmp(it.second.GetCString(), path_cstr, prefixLen) == 0) {
      std::string new_path_str(it.first.GetCString());
      new_path_str.append(path.GetCString() + prefixLen);
      new_path.SetCString(new_path_str.c_str());
      return true;
    }
  }
  return false;
}

bool PathMappingList::FindFile(const FileSpec &orig_spec,
                               FileSpec &new_spec) const {
  if (!m_pairs.empty()) {
    char orig_path[PATH_MAX];
    const size_t orig_path_len =
        orig_spec.GetPath(orig_path, sizeof(orig_path));
    if (orig_path_len > 0) {
      const_iterator pos, end = m_pairs.end();
      for (pos = m_pairs.begin(); pos != end; ++pos) {
        const size_t prefix_len = pos->first.GetLength();

        if (orig_path_len >= prefix_len) {
          if (::strncmp(pos->first.GetCString(), orig_path, prefix_len) == 0) {
            new_spec.SetFile(pos->second.GetCString(), false);
            new_spec.AppendPathComponent(orig_path + prefix_len);
            if (new_spec.Exists())
              return true;
          }
        }
      }
    }
  }
  new_spec.Clear();
  return false;
}

bool PathMappingList::Replace(const ConstString &path,
                              const ConstString &new_path, bool notify) {
  uint32_t idx = FindIndexForPath(path);
  if (idx < m_pairs.size()) {
    ++m_mod_id;
    m_pairs[idx].second = new_path;
    if (notify && m_callback)
      m_callback(*this, m_callback_baton);
    return true;
  }
  return false;
}

bool PathMappingList::Remove(const ConstString &path, bool notify) {
  iterator pos = FindIteratorForPath(path);
  if (pos != m_pairs.end()) {
    ++m_mod_id;
    m_pairs.erase(pos);
    if (notify && m_callback)
      m_callback(*this, m_callback_baton);
    return true;
  }
  return false;
}

PathMappingList::const_iterator
PathMappingList::FindIteratorForPath(const ConstString &path) const {
  const_iterator pos;
  const_iterator begin = m_pairs.begin();
  const_iterator end = m_pairs.end();

  for (pos = begin; pos != end; ++pos) {
    if (pos->first == path)
      break;
  }
  return pos;
}

PathMappingList::iterator
PathMappingList::FindIteratorForPath(const ConstString &path) {
  iterator pos;
  iterator begin = m_pairs.begin();
  iterator end = m_pairs.end();

  for (pos = begin; pos != end; ++pos) {
    if (pos->first == path)
      break;
  }
  return pos;
}

bool PathMappingList::GetPathsAtIndex(uint32_t idx, ConstString &path,
                                      ConstString &new_path) const {
  if (idx < m_pairs.size()) {
    path = m_pairs[idx].first;
    new_path = m_pairs[idx].second;
    return true;
  }
  return false;
}

uint32_t PathMappingList::FindIndexForPath(const ConstString &path) const {
  const_iterator pos;
  const_iterator begin = m_pairs.begin();
  const_iterator end = m_pairs.end();

  for (pos = begin; pos != end; ++pos) {
    if (pos->first == path)
      return std::distance(begin, pos);
  }
  return UINT32_MAX;
}
