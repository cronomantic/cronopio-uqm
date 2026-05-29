/* vid_cron.h — Cronopio graphics backend for UQM (see vid_cron.c). */
#ifndef CRON_VID_CRON_H
#define CRON_VID_CRON_H

#include <stdint.h>

void      cron_vid_init    (void);
void      cron_vid_present (void);
uint32_t *cron_vid_screen  (int which);   /* 0=MAIN 1=EXTRA 2=TRANSITION */
int       cron_vid_width   (void);
int       cron_vid_height  (void);

#endif /* CRON_VID_CRON_H */
