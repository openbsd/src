//===-- BreakpointList.cpp --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/Breakpoint/BreakpointList.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;

BreakpointList::BreakpointList(bool is_internal)
    : m_mutex(), m_breakpoints(), m_next_break_id(0),
      m_is_internal(is_internal) {}

BreakpointList::~BreakpointList() {}

break_id_t BreakpointList::Add(BreakpointSP &bp_sp, bool notify) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  // Internal breakpoint IDs are negative, normal ones are positive
  bp_sp->SetID(m_is_internal ? --m_next_break_id : ++m_next_break_id);

  m_breakpoints.push_back(bp_sp);
  if (notify) {
    if (bp_sp->GetTarget().EventTypeHasListeners(
            Target::eBroadcastBitBreakpointChanged))
      bp_sp->GetTarget().BroadcastEvent(Target::eBroadcastBitBreakpointChanged,
                                        new Breakpoint::BreakpointEventData(
                                            eBreakpointEventTypeAdded, bp_sp));
  }
  return bp_sp->GetID();
}

bool BreakpointList::Remove(break_id_t break_id, bool notify) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  bp_collection::iterator pos = GetBreakpointIDIterator(break_id); // Predicate
  if (pos != m_breakpoints.end()) {
    BreakpointSP bp_sp(*pos);
    m_breakpoints.erase(pos);
    if (notify) {
      if (bp_sp->GetTarget().EventTypeHasListeners(
              Target::eBroadcastBitBreakpointChanged))
        bp_sp->GetTarget().BroadcastEvent(
            Target::eBroadcastBitBreakpointChanged,
            new Breakpoint::BreakpointEventData(eBreakpointEventTypeRemoved,
                                                bp_sp));
    }
    return true;
  }
  return false;
}

void BreakpointList::RemoveInvalidLocations(const ArchSpec &arch) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->RemoveInvalidLocations(arch);
}

void BreakpointList::SetEnabledAll(bool enabled) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->SetEnabled(enabled);
}

void BreakpointList::RemoveAll(bool notify) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  ClearAllBreakpointSites();

  if (notify) {
    bp_collection::iterator pos, end = m_breakpoints.end();
    for (pos = m_breakpoints.begin(); pos != end; ++pos) {
      if ((*pos)->GetTarget().EventTypeHasListeners(
              Target::eBroadcastBitBreakpointChanged)) {
        (*pos)->GetTarget().BroadcastEvent(
            Target::eBroadcastBitBreakpointChanged,
            new Breakpoint::BreakpointEventData(eBreakpointEventTypeRemoved,
                                                *pos));
      }
    }
  }
  m_breakpoints.erase(m_breakpoints.begin(), m_breakpoints.end());
}

class BreakpointIDMatches {
public:
  BreakpointIDMatches(break_id_t break_id) : m_break_id(break_id) {}

  bool operator()(const BreakpointSP &bp) const {
    return m_break_id == bp->GetID();
  }

private:
  const break_id_t m_break_id;
};

BreakpointList::bp_collection::iterator
BreakpointList::GetBreakpointIDIterator(break_id_t break_id) {
  return std::find_if(m_breakpoints.begin(),
                      m_breakpoints.end(),            // Search full range
                      BreakpointIDMatches(break_id)); // Predicate
}

BreakpointList::bp_collection::const_iterator
BreakpointList::GetBreakpointIDConstIterator(break_id_t break_id) const {
  return std::find_if(m_breakpoints.begin(),
                      m_breakpoints.end(),            // Search full range
                      BreakpointIDMatches(break_id)); // Predicate
}

BreakpointSP BreakpointList::FindBreakpointByID(break_id_t break_id) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  BreakpointSP stop_sp;
  bp_collection::iterator pos = GetBreakpointIDIterator(break_id);
  if (pos != m_breakpoints.end())
    stop_sp = *pos;

  return stop_sp;
}

const BreakpointSP
BreakpointList::FindBreakpointByID(break_id_t break_id) const {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  BreakpointSP stop_sp;
  bp_collection::const_iterator pos = GetBreakpointIDConstIterator(break_id);
  if (pos != m_breakpoints.end())
    stop_sp = *pos;

  return stop_sp;
}

bool BreakpointList::FindBreakpointsByName(const char *name,
                                           BreakpointList &matching_bps) {
  Status error;
  if (!name)
    return false;

  if (!BreakpointID::StringIsBreakpointName(llvm::StringRef(name), error))
    return false;

  for (BreakpointSP bkpt_sp : Breakpoints()) {
    if (bkpt_sp->MatchesName(name)) {
      matching_bps.Add(bkpt_sp, false);
    }
  }
  return true;
}

void BreakpointList::Dump(Stream *s) const {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  s->Printf("%p: ", static_cast<const void *>(this));
  s->Indent();
  s->Printf("BreakpointList with %u Breakpoints:\n",
            (uint32_t)m_breakpoints.size());
  s->IndentMore();
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->Dump(s);
  s->IndentLess();
}

BreakpointSP BreakpointList::GetBreakpointAtIndex(size_t i) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  BreakpointSP stop_sp;
  bp_collection::iterator end = m_breakpoints.end();
  bp_collection::iterator pos;
  size_t curr_i = 0;
  for (pos = m_breakpoints.begin(), curr_i = 0; pos != end; ++pos, ++curr_i) {
    if (curr_i == i)
      stop_sp = *pos;
  }
  return stop_sp;
}

const BreakpointSP BreakpointList::GetBreakpointAtIndex(size_t i) const {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  BreakpointSP stop_sp;
  bp_collection::const_iterator end = m_breakpoints.end();
  bp_collection::const_iterator pos;
  size_t curr_i = 0;
  for (pos = m_breakpoints.begin(), curr_i = 0; pos != end; ++pos, ++curr_i) {
    if (curr_i == i)
      stop_sp = *pos;
  }
  return stop_sp;
}

void BreakpointList::UpdateBreakpoints(ModuleList &module_list, bool added,
                                       bool delete_locations) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->ModulesChanged(module_list, added, delete_locations);
}

void BreakpointList::UpdateBreakpointsWhenModuleIsReplaced(
    ModuleSP old_module_sp, ModuleSP new_module_sp) {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->ModuleReplaced(old_module_sp, new_module_sp);
}

void BreakpointList::ClearAllBreakpointSites() {
  std::lock_guard<std::recursive_mutex> guard(m_mutex);
  for (const auto &bp_sp : m_breakpoints)
    bp_sp->ClearAllBreakpointSites();
}

void BreakpointList::GetListMutex(
    std::unique_lock<std::recursive_mutex> &lock) {
  lock = std::unique_lock<std::recursive_mutex>(m_mutex);
}
