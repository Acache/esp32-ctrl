#include "../../firmware/desk-mate-app/lv_conf.h"
/* Same widgets/fonts/color depth as hardware; extra memory only for screenshots. */
#undef LV_MEM_SIZE
#define LV_MEM_SIZE (1024 * 1024)
#undef LV_USE_SNAPSHOT
#define LV_USE_SNAPSHOT 1
