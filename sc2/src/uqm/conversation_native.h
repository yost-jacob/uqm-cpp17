#ifndef UQM_CONVERSATION_NATIVE_H
#define UQM_CONVERSATION_NATIVE_H
#ifdef __cplusplus
extern "C" {
#endif
// Native inventory/transport boundary; requires the active campaign binding.
// The caller owns story transitions and supplies whether a starbase is available.
void ApplyChmmrBombRefit(int starbaseAvailable);
#ifdef __cplusplus
}
#endif
#endif
