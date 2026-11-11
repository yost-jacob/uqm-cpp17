#include "uqm/grpinfo.h"

/* Exercise real typed IP_GROUP storage and read-only queries from C. */
int game_queue_c_contract(void) {
    QUEUE queue = {0};
    const QUEUE *read_only = &queue;
    HLINK a, b;
    IP_GROUP *event;
    int ok;
    InitQueue(&queue, 2, QUEUE_IP_GROUP);
    a = AllocLink(&queue);
    b = AllocLink(&queue);
    event = (IP_GROUP *)LockLink(&queue, a);
    event->race_id = 12;
    PutQueue(&queue, a);
    InsertQueue(&queue, b, a);
    ok = GetLinkSize(read_only) == sizeof(IP_GROUP) && CountLinks(read_only) == 2
            && GetHeadLink(read_only) == b && GetTailLink(read_only) == a
            && _GetSuccLink(b) == a && _GetPredLink(a) == b
            && event->race_id == 12;
    RemoveQueue(&queue, b);
    FreeLink(&queue, b);
    ok = ok && AllocLink(&queue) == b;
    UninitQueue(&queue);
    return ok && GetHeadLink(read_only) == NULL && queue.storage == NULL;
}
