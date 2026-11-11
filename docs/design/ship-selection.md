# Combat participation and next-ship selection

RuntimeShipRoster now owns match availability and last-battle outcomes alongside
its participant pools. Pool size is not availability: retired/X-marked entries
can remain, SIS selection forfeits further escorts, and infinite fleets recycle
one record. beginBattle initializes availability from prepared participants;
retirement and selection update it explicitly.

Availability transfers with RuntimeRosterTransfer. The suspended encounter gets
the final counts back before aftermath, instead of reading process-global state
left behind by its child. Unrelated modes retain their own counts. Scene reset
and participant clear preserve last-battle outcome metadata, because aftermath
can still need it after assets/slots are released (including final battles).
The next beginBattle replaces that metadata from its new participants.

Native C++ callers use remaining/setRemaining/retire. The C retirement and melee
picker backends borrow GetBattleCounters() for the currently selected roster.
There is no global battle_counter array or macro alias. That C buffer must not
be retained across mode switches or roster destruction.

ship_selection.cpp now owns next-vessel policy with explicit roster, player and
random-stream dependencies. Native selection returns optional reference borrows;
no vessel is copied or newly owned. Previous participants must belong to the
appropriate roster side, and raw picker results are checked by identity before
being exposed as native borrows.

The retained rules are:

- navigation selects the first player-side vessel;
- Super Melee requires availability on both sides and delegates its UI choice;
- campaign construction puts SIS last; a sole initial SIS needs no dialog;
- selecting SIS ends further escort choices;
- a destroyed final SIS records defeat, while withdrawal deducts five fuel units
  with a zero floor;
- finite enemy fleets advance in queue order;
- infinite fleets reuse the same participant address, restore crew and consume
  exactly one captain-name RNG draw for a replacement incarnation.

The GetEncounterStarShip C policy entry is removed. pickship.c retains drawing
and modal input only; PickCampaignStarShip is its UI buffer boundary. GetNextStarShip
is the existing spawn/callback adapter: it routes the active context to native
policy and turns the resulting borrow into the existing spawn handle. It does not
create a campaign player for arcade. The remaining C callbacks/menus, packed
participant records and captain-resource indices are compatibility machinery.

Regression coverage uses the real participant pools and native player/SIS state,
substituting only picker UI. It checks fixed identity/order, invalid/foreign UI
handles, SIS selection/retirement, withdrawal bounds, infinite-fleet RNG consumption,
mode-local counters and outcome handoff after scene/participant cleanup.
