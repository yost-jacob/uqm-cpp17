/*
 *  Copyright 2006  Serge van den Boom <svdb@stack.nl>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef LIBS_CALLBACK_ALARM_H_
#define LIBS_CALLBACK_ALARM_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32 AlarmTime;
typedef struct Alarm Alarm;
typedef void *AlarmCallbackArg;
typedef void (*AlarmCallback)(AlarmCallbackArg arg);

/* Initialize the scheduler once before use. Callers serialize all operations;
 * callbacks run on the thread calling Alarm_processOne. */
void Alarm_init(void);

/* Release all pending alarms without invoking callbacks or freeing their
 * borrowed arguments. Invalidates every pending handle; requires initialization. */
void Alarm_uninit(void);

/* Schedule callback at absolute SDL tick time ms. callback must be non-NULL;
 * arg is borrowed (NULL is allowed) and must be valid when callback executes.
 * Return a borrowed cancellation handle valid only while the alarm is pending.
 * Equal deadlines run in insertion order. Allocation failure terminates without
 * propagating C++ exceptions into C. The scheduler must be initialized. */
Alarm *Alarm_addAbsoluteMs(uint32 ms, AlarmCallback callback, AlarmCallbackArg arg);

/* As above, with a delay from the current SDL tick. Tick arithmetic retains
 * the existing unsigned 32-bit behavior; this does not change the clock policy. */
Alarm *Alarm_addRelativeMs(uint32 ms, AlarmCallback callback, AlarmCallbackArg arg);

/* Cancel and release a pending alarm. The handle must be non-NULL and still
 * pending; never cancel the executing alarm or reuse a dispatched/canceled
 * handle. The borrowed argument is not freed. */
void Alarm_remove(Alarm *alarm);

/* Dispatch at most one due alarm, returning whether a callback ran. Its handle
 * becomes invalid before callback invocation. Callbacks may cancel other alarms,
 * add alarms, or call Alarm_processOne recursively; they must not throw. */
bool Alarm_processOne(void);

/* Return the earliest absolute SDL tick deadline, or UINT32_MAX if empty.
 * Despite the legacy name, this is not a remaining duration. Requires init. */
uint32 Alarm_timeBeforeNextMs(void);

#ifdef __cplusplus
}
#endif
#endif /* LIBS_CALLBACK_ALARM_H_ */
