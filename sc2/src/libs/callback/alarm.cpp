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

#include <cassert>
#include <exception>
#include <map>
#include <memory>
#include <utility>
#include "alarm.h"

#include <SDL_timer.h>

using Schedule = std::multimap<AlarmTime, std::unique_ptr<Alarm>>;

struct Alarm {
    Schedule::iterator position;
    AlarmCallback callback;
    AlarmCallbackArg arg;
};

namespace {
Schedule pending;
bool initialized = false;
}

void
Alarm_init(void) {
    assert(!initialized && pending.empty());
    initialized = true;
}

void
Alarm_uninit(void) {
    assert(initialized);
    pending.clear();
    initialized = false;
}

Alarm *
Alarm_addAbsoluteMs(const uint32 ms, const AlarmCallback callback,
        AlarmCallbackArg arg) try {
    assert(initialized && callback);
    auto alarm = std::make_unique<Alarm>();
    alarm->callback = callback;
    alarm->arg = arg;
    const auto position = pending.emplace(ms, std::move(alarm));
    position->second->position = position;
    return position->second.get();
} catch (...) {
    std::terminate();
}

Alarm *
Alarm_addRelativeMs(const uint32 ms, const AlarmCallback callback,
        AlarmCallbackArg arg) {
    return Alarm_addAbsoluteMs(SDL_GetTicks() + ms, callback, arg);
}

void
Alarm_remove(Alarm *alarm) {
    assert(initialized && alarm);
    pending.erase(alarm->position);
}

bool
Alarm_processOne(void) try {
    assert(initialized);
    const auto position = pending.begin();
    if (position == pending.end() || SDL_GetTicks() < position->first)
        return false;

    // Remove before invoking user code. A local owner keeps callback data alive
    // even if the callback changes or clears the scheduler, or dispatches again.
    auto alarm = std::move(position->second);
    pending.erase(position);
    alarm->callback(alarm->arg);
    return true;
} catch (...) {
    std::terminate();
}

uint32
Alarm_timeBeforeNextMs(void) {
    assert(initialized);
    return pending.empty() ? UINT32_MAX : pending.begin()->first;
}
