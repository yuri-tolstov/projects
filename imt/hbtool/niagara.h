/******************************************************************************/
/* File:   hbtool/niagara.h                                                   */
/******************************************************************************/
#ifndef __NIAGARA_H__
#define __NIAGARA_H__

#include <stdint.h>

#define NIAGARA_API_VERSION	"1.0.0"
/******************************************************************************/
/* Hardware Modules                                                           */
/******************************************************************************/
/* NOTE: The list is based on the hardware modules age,
 * the youngest -- on the top of the list.*/
enum {
   IMTHW_N808,
   IMTHW_N804,
   IMTHW_MAX,
   IMTHW_UNDEF = -1
};

/*Bypass segments.*/
enum {
   N808_SEG_1,
   N808_SEG_2,
   N808_SEG_3,
   N808_SEG_4,
   N808_SEG_MAX
};

/******************************************************************************/
/* Niagara API                                                                */
/******************************************************************************/
typedef struct niagara_ops_s {
   int (*probe)(void);
   int (*numseg_get)(void);
   int (*timeout_get)(int seg);
   int (*timeout_set)(int seg, char *v);
   int (*startup_wait_get)(int seg);
   int (*startup_wait_set)(int seg, char *v);
   int (*startup_wait_ovr_get)(int seg);
   int (*startup_wait_ovr_set)(int seg, char *v);
   int (*default_mode_get)(int seg);
   int (*default_mode_set)(int seg, char *v);
   int (*poweroff_mode_get)(int seg);
   int (*poweroff_mode_set)(int seg, char *v);
   int (*current_mode_get)(int seg);
   int (*current_mode_set)(int seg, char *v);
   int (*status_get)(int seg);
   int (*oem_id_get)(int seg);
   int (*product_rev_get)(int seg);
   int (*product_id_get)(int seg);
   int (*fw_rev_get)(int seg);
   int (*kick_heartbeat)(int seg);
} niagara_ops_t;

typedef struct niagara_hwini_s {
   int (*init)(niagara_ops_t *ops);
} niagara_hwini_t;

int n804_init(niagara_ops_t *ops);
int n808_init(niagara_ops_t *ops);

#if 0
/******************************************************************************/
/* Usage.                                                                     */
/******************************************************************************/
niagara_hwini_t ifn[IMTHW_MAX] = {
   {.init = n808_init},
   {.init = n804_init},
};
niagara_ops_t hwm[IMTHW_MAX];
int hwi;
int rc;

...
for (hwi = IMTHW_N804; hwi < IMTHW_MAX; hwi++) {
   ifn[hwi].init(&hwm[hwi]);
   rc = hwm[hwi].probe();
   if (rc == 0) break;
}
...
hwm[hwi].current_mode_set(seg, "1");
...
/******************************************************************************/
#endif

/******************************************************************************/
#endif /*__NIAGARA_H__*/

