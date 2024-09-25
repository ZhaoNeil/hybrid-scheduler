// Copyright 2021 Google LLC
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "experiments/shared/prio_table_helper.h"

#include "lib/base.h"

namespace ghost_test {

void PrioTableHelper::GetWorkClass(uint32_t wcid, ghost::work_class& wc) const {
  CheckWorkClassInRange(wcid);

  wc = *table_.work_class(wcid);
}

void PrioTableHelper::SetWorkClass(uint32_t wcid, const ghost::work_class& wc) {
  CHECK_EQ(wcid, wc.id);
  CheckWorkClassInRange(wcid);

  *table_.work_class(wcid) = wc;
}

void PrioTableHelper::CopySchedItem(ghost::sched_item& dst,
                                    const ghost::sched_item& src) const {
  dst.sid = src.sid;
  dst.wcid = src.wcid;
  dst.gpid = src.gpid;
  dst.flags = src.flags;
  dst.deadline = src.deadline;
}

void PrioTableHelper::GetSchedItem(uint32_t sid, ghost::sched_item& si) const {
  CheckSchedItemInRange(sid);

  CopySchedItem(si, *table_.sched_item(sid));
}

void PrioTableHelper::SetSchedItem(uint32_t sid, const ghost::sched_item& si) {
  CHECK_EQ(sid, si.sid);
  CheckSchedItemInRange(si.sid);
  CheckWorkClassInRange(si.wcid);

  ghost::sched_item* curr = table_.sched_item(sid);
  uint32_t begin = curr->seqcount.write_begin();
  CopySchedItem(*curr, si);
  curr->seqcount.write_end(begin);
  MarkUpdatedTableIndex(curr->sid);
}

PrioTableHelper::PrioTableHelper(uint32_t num_sched_items,
                                 uint32_t num_work_classes)
    : table_(num_sched_items, num_work_classes,
             ghost::PrioTable::StreamCapacity::kStreamCapacity83) {
  CHECK(num_sched_items == 0 || num_work_classes >= 1);
}

void PrioTableHelper::MarkRunnability(uint32_t sid, bool runnable) {
  CheckSchedItemInRange(sid);

  ghost::sched_item* si = table_.sched_item(sid);
  uint32_t begin = si->seqcount.write_begin();
  if (runnable) {
    si->flags |= SCHED_ITEM_RUNNABLE;
  } else {
    si->flags &= ~SCHED_ITEM_RUNNABLE;
  }
  si->seqcount.write_end(begin);
  MarkUpdatedTableIndex(si->sid);
}

void PrioTableHelper::MarkRunnable(uint32_t sid) {
  MarkRunnability(sid, /*runnable=*/true);
}

void PrioTableHelper::MarkIdle(uint32_t sid) {
  MarkRunnability(sid, /*runnable=*/false);
}

void PrioTableHelper::WaitUntilRunnable(uint32_t sid) const {
  CheckSchedItemInRange(sid);

  ghost::sched_item* si = table_.sched_item(sid);
  std::atomic<uint32_t>* flags =
      reinterpret_cast<std::atomic<uint32_t>*>(&si->flags);
  while ((flags->load(std::memory_order_acquire) & SCHED_ITEM_RUNNABLE) == 0) {
    ghost::Pause();
  }
}

}  // namespace ghost_test
