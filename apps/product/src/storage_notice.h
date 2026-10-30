#ifndef WF_PRODUCT_STORAGE_NOTICE_H
#define WF_PRODUCT_STORAGE_NOTICE_H
/* UI thread only, at existing foreground iterations. No timer or wake request. */
void wf_product_storage_notice_poll(void);
#endif
