#ifndef UQM_SESSION_ACTIVITY_H_
#define UQM_SESSION_ACTIVITY_H_
#include "libs/compiler.h"
typedef struct UQM_SESSION_ACTIVITY { UWORD last, next; } UQM_SESSION_ACTIVITY;
#ifdef __cplusplus
extern "C" {
#endif
UWORD *GetLastSessionActivity(void);
UWORD *GetNextSessionActivity(void);
#ifdef __cplusplus
}
#endif
#endif
