#ifndef WF_PRODUCT_RUNTIME_RECORD_H
#define WF_PRODUCT_RUNTIME_RECORD_H
#include "runtime_record.h"
/* 0 only for an absent key; SIZE_MAX for read/truncation failure. */
typedef size_t (*wf_runtime_read_t)(unsigned,uint8_t *,size_t,void *);
typedef bool (*wf_runtime_write_t)(unsigned,const uint8_t *,void *);
bool wf_product_runtime_start(wf_runtime_read_t,wf_runtime_write_t,void *,uint64_t now,uint32_t mode,uint32_t raw);
/* Thread context only. Observe consumes a copy; it does not sample devices or do I/O. */
void wf_product_runtime_observe(const wf_runtime_observation_t *);
/* Storage-worker context; performs checkpoint Flash I/O outside the model lock. */
void wf_product_runtime_process(void *unused);
bool wf_product_runtime_failed(void);
void wf_product_runtime_disable_storage(void);
bool wf_product_runtime_status(wf_runtime_summary_t *current,wf_runtime_summary_t *saved,bool *saved_valid,bool *pending);
#endif
