#include "uqm/clock.h"

/* Exercise real typed EVENT storage and read-only queries from C. */
int game_queue_c_contract(void) {
    QUEUE queue = {0};
    const QUEUE *read_only = &queue;
    HLINK a, b;
    EVENT *event;
    int ok;
    InitQueue(&queue, 2, QUEUE_EVENT);
    a = AllocLink(&queue);
    b = AllocLink(&queue);
    event = (EVENT *)LockLink(&queue, a);
    event->day_index = 12;
    PutQueue(&queue, a);
    InsertQueue(&queue, b, a);
    ok = GetLinkSize(read_only) == sizeof(EVENT) && CountLinks(read_only) == 2
            && GetHeadLink(read_only) == b && GetTailLink(read_only) == a
            && _GetSuccLink(b) == a && _GetPredLink(a) == b
            && event->day_index == 12;
    RemoveQueue(&queue, b);
    FreeLink(&queue, b);
    ok = ok && AllocLink(&queue) == b;
    UninitQueue(&queue);
    return ok && GetHeadLink(read_only) == NULL && queue.storage == NULL;
}
