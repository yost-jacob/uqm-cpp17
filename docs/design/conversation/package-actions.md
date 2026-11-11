# Positional package actions

An instruction names an action and supplies its positional arguments. The graph
runner does not interpret inventory or story rules; the bound operation does.
All live graphs and the Hayes/Melnorme reference examples use the `action` tag.
The old `verb` spelling is accepted for compatibility, never together with `action`.

```json
{"action":"transact","args":[{"credits":100},{"fuel":10}]}
{"action":"transact","args":[{},{"crew":100,"items.sun_device":1}]}
{"action":"set","args":[{"MET_ARILOU":true},{"ARILOU_VISITS":2}]}
```

`transact` takes player-gives first and player-receives second. Either package may
be empty, supporting purchases, gifts, confiscation and barter with one vocabulary.
Both cannot be empty. Amounts are positive integers; zero/negative/floating-point
quantities are rejected. Values can also use the existing read-only expression
syntax, for example `{"get":["local","fuel_offer"]}`. The binder validates names
and constructs the callable; invocation resolves every value before mutation.

The live inventory currently registers `crew`, `fuel`, `ru`, `credits`, `biomass`,
`landers`, eight `minerals.*` categories, and twenty unique `items.*` entries. Fuel
amounts are whole displayed units; native hundredths are preserved, never rounded
away. Exact delivery fails if only a fractional requested unit fits. Merchant fill
quotes need a precise-unit extension before the Melnorme migration.

Mineral categories are `common`, `corrosive`, `base_metal`, `noble`, `rare_earth`,
`precious`, `radioactive`, and `exotic`. For example `minerals.radioactive`.

Items are `sun_device`, `rosy_sphere`, `artifact_2`, `artifact_3`, `utwig_bomb`,
`maidens`, `talking_pet`, `aqua_helix`, `clear_spindle`, `umgah_broadcaster`,
`taalo_protector`, `egg_case_0`, `egg_case_1`, `egg_case_2`, `syreen_shuttle`,
`vux_beast`, `portal_spawner`, `portal_key`, `burvix_broadcaster`, and `moonbase`.
They have capacity one; compound item states such as Ultron repairs need their
own operation rather than being treated as arbitrary counted items.

## Inventory and story are separate

`PlayerEntity` owns `Inventory`, which owns `Fleet` and its optional flagship.
SIS resources and packed device/credit fields remain the authoritative storage
while C coexists. There is no second inventory map to synchronize with saves.
The native inventory has no JSON dependency and applies no narrative effects.
Legacy UI capacity functions delegate to the same rules as the native inventory.

Inventory validates the complete request, checks that the player owns the given
assets, stages removals/additions and capacity checks, then commits. Received
assets cannot finance the same transaction's consideration. Cargo categories
share storage capacity; cash accounts and integer fields have explicit limits.
Failure leaves both packages unapplied and consumes no RNG. Default fulfillment
is exact, not clamped. Existing clamped legacy adjustments remain distinct until
a caller explicitly chooses how to construct a smaller exchange.

The action returns `ok`, `insufficient`, `no_capacity`, or `unavailable`. Failures
include `asset` identifying the rejected resource. Decision nodes inspect the
stored result; they never attempt the transaction again. Replay and stale-input
protection remain the graph runner's responsibility.

`set` targets narrative fields only. Each argument is a nonempty field object;
multiple fields in one object are also valid. Duplicate keys across arguments,
unknown fields, inventory fields, and out-of-range values fail validation. Every
expression observes the state before the batch: assignments are not sequential
instructions. The backend validates the entire batch before any packed-bit write.
Local conversation memory continues to use `state.set` with the local scope.

Prices, hiring/slavery accounting, technology effects and other story consequences
belong to callers or named story helpers. An inventory exchange and a following
story update are separate commits. A one-time reward spanning both domains needs
a helper that defines the intended retry/availability policy; ordinary graph
sequencing is not a cross-domain transaction manager.

## Remaining migration

The fleet now belongs to Inventory, but ship packages/factory staging are still
pending. Unknown ship/package keys fail binding. The remaining Druuge, Melnorme and
operational starbase native handlers stay in place until their real operations
are implemented. Merchant quotes, partial fuel, rescue plans, report preparation
and acknowledgement, and migration of old item/currency flag operations remain
explicit work. The first live package use is Hayes's bounded replacement-lander
operation and its following story assignment; the rest of that aid sequence keeps
its existing clamping and dialogue timing.
