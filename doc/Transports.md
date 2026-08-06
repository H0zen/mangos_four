# Transports — notes for whoever works on them next

This is not a specification and not a mandate. It is a record of what was measured, what each
of two competing models predicts, and which experiments tell them apart. Nothing here asks to
be believed; all of it asks to be checked, and the last section lists what would sink it.

If you have arrived here to fix a transport bug, the useful question is probably not *which
line is wrong*. It is **which of the two models below the surrounding code is written in**,
because most transport bugs are one bug seen from a different angle, and a point fix inside the
wrong model tends to move the symptom rather than remove it.

> **Reading this without the tree.** Every link below points at
> [`H0zen/mangos_four`](https://github.com/H0zen/mangos_four), branch `transports`, pinned to
> commit [`c07d822`](https://github.com/H0zen/mangos_four/tree/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e)
> so the line anchors stay on the lines they were written for. To see what the branch looks like
> now instead, swap the SHA in any of these URLs for `transports`. Nothing in this document asks
> you to take a quotation on trust — all of it is one click away.

---

## 1. What the server actually knows about where a ship is

Read [`Transports.cpp:880`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/WorldHandlers/Transports.cpp#L880) before anything else:

```cpp
m_timer = uint32(GameTime::GetAbsoluteTimeMS() % m_period);
while (((m_timer - m_curr->first) % m_pathTime) > ((m_next->first - m_curr->first) % m_pathTime))
```

The hull's server-side pose is a **waypoint index chosen by a modulo clock**. It is not
integrated, not interpolated, not corrected against anything. It advances in discrete jumps
between taxi path nodes.

The client does something different. It has `TransportAnimation.dbc` and it interpolates the
hull continuously along it. This tree does load that DBC — see
[`GameObject.cpp:248`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/GameObject.cpp#L248) and
[`ObjectMgrGameObjectInfo.cpp:322`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/ObjectMgrGameObjectInfo.cpp#L322) — but
only to *validate* that a transport template has frames at all. Grep it: no call site uses it
to compute a pose. So the two sides are running different animations of the same ship, and
nothing reconciles them.

That is worth sitting with, because it is the whole of the matter. **The server's idea of a
hull's world position and the client's idea are two different numbers**, and the gap is not a
small constant. It is largest exactly where it hurts: at speed, on turns, and between two
widely-spaced nodes.

A reasonable objection at this point is "so tighten the estimate". It is worth working out what
tightening would have to achieve. To make `hull_world ⊗ local_offset` trustworthy, the server
would have to agree with the client's interpolation *at every instant a range check runs* — not
on average, not within a yard. Anything less and the composed position of a boarded unit is
wrong by an amount nobody can bound, which is a different problem from being wrong by a known
amount.

---

## 2. Three things the client will tell you if you ask it

These are cheap to verify and they constrain the design more than any amount of reading server
source.

**(a) `Map.dbc` already contains transport rows.** Map 584 is
`Transport: Menethil to Theramore`. The client shipped with the idea that a ship is a map; it
is not an invention of this tree.

Be careful how much weight you put on this, though — the keying is *not* 1:1 and
[`vessels.txt`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/tools/extractor/vessels.txt) works through why: some rows name a route
rather than a hull (two opposite runs would share one deck), entry 186238 has two rows, and
five route rows match no transport at all. So the rows establish the *idea*, not a ready-made
id scheme. Something has to decide, per vessel, whether to adopt the client's id or mint one.

**(b) None of those rows has terrain.** From the extractor's own notes in
[`vessels.txt`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/tools/extractor/vessels.txt): *"NO transport row in Map.dbc has client
terrain — 0 of 49, against a bake that produced geometry for every ordinary map around them."*
The hull only ever existed as a game object model. So the client knows the map id exists and
cannot load it.

**(c) `SMSG_UPDATE_OBJECT` carries a `uint16` map id, and the client dereferences it.** On
2026-08-05 a GM standing on a deck typed `.npc add 6086` and his client died — `ERROR #132`,
`ACCESS_VIOLATION` reading `[NULL+0x1A0]`. The packet:

```text
48 02 | 01 00 00 00 | 02 | F7 8B3A82 C617 30F1
^^^^^   blockCount   CREATE  guid F13017C600823A8B (entry 6086)
map 0x0248 = 584
```

Every other `SMSG_UPDATE_OBJECT` in that capture carried map 0. The client answered this one
with its shutdown triple and no logout request. See
[`Player::GetClientMapId`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/Player.cpp#L5828).

Put (a), (b) and (c) together and a fairly tight constraint falls out: the vessel map id is
usable server-side and **must never appear on the wire**. That is a narrow, testable rule, and
it is the single easiest thing to get wrong.

---

## 3. Two models, and what each predicts

**Model A — the ship is a game object; a passenger has a world position.** A boarded unit's
authoritative position is `hull_world ⊗ local_offset`, recomposed each tick. This is what
MaNGOS and its derivatives do, and what most transport code you will read assumes.

**Model B — the ship is a map; deck-local coordinates *are* the coordinates.** A boarded unit
lives on the vessel's map. Nothing is composed and nothing is rotated. Ship↔world is a map
boundary, crossed by the same machinery that crosses any other.

They are not stylistic variants; they make different, checkable predictions.

| | Model A predicts | Model B predicts |
| --- | --- | --- |
| Two units on the same deck, ranges between them | error grows with ship speed and turn rate | exact, and independent of the ship |
| Melee/spell range aboard a ship at speed | intermittent out-of-range | unaffected |
| Grid cell of a boarded unit | churns as the hull moves | fixed while he stands still |
| LoS between two units on one deck | consults continent geometry under the hull | consults the hull |
| Pathfinding on a deck | needs a mesh that moves | ordinary static mesh |
| Cost of a fix for any one of the above | one special case per consumer | none — the consumers are unchanged |

The last row is the one that decides it in practice, and it is also the honest explanation for
why this has never converged upstream. Under Model A, *every* consumer of a position — range,
LoS, threat, spell targeting, visibility, pathing, `.gps`, chat range, loot range — needs to
know about transports. Under Model B, none of them does, because the frame they are handed is
already the right one.

### Discriminating experiments

Cheap, and they do not require agreeing with anything written above:

1. Stand two characters on a moving deck a fixed distance apart. Log
   `GetDistance()` each tick. Under A it oscillates with the hull's waypoint jumps; under B it
   is constant. This one test is decisive and takes ten minutes.
2. Aboard a ship at speed, log a boarded creature's `GetPositionX/Y/Z()` and compare it with
   where the client draws it. The size of the disagreement is the size of the problem.
3. Take any existing transport bug report in any MaNGOS-lineage tracker and ask whether it is a
   distinct defect or the same composition error observed through a different consumer. The
   answer is informative either way.

---

## 4. If the second reading survives your testing

What follows is not "the design". It is the list of seams that turn out to exist once a vessel
is a map, offered so that whoever gets there next does not have to rediscover them one client
crash at a time. Each has a concrete failure signature.

### 4.1 The map id is server-side only

`Player::GetClientMapId()` returns the map the **client is rendering** — the world map the ship
sails — while `GetMapId()` returns the deck. Every packet carrying a map id needs the former.
The ones that do in this tree: `UpdateData` construction (all sites),
`Map::SendInitSelf`'s `sp.mapId`, and the visibility notifier.

*Failure signature:* immediate `ERROR #132` in the client, `ACCESS_VIOLATION` on a small
constant offset from null, on the exact tick some object is created for a passenger. If you see
that, dump the first two bytes of the `SMSG_UPDATE_OBJECT` body before theorising.

### 4.2 The self create block is the login packet

`Map::SendInitSelf` builds the 18414 self create block. The client treats receipt of its own
create block as *entering the world*: it runs the enter-world path and shows the loading
screen. Re-sending it to a client that never left produces the login screen on the map the
player is already standing on.

This matters at the gangway, because moving a player between the world map and the deck map is
a server-side re-file, not a client transition. In this tree that is the `introduce` parameter
on [`Map::Add(Player*)`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/WorldHandlers/Map.h#L181): true for login and for the far
side of a map crossing, false for boarding and for stepping ashore.

*Failure signature:* a loading screen when walking up a gangplank, on the same map, identical
to the login screen. Confirmable from a packet log — look for a ~30 KB `SMSG_UPDATE_OBJECT`
immediately followed by `CMSG_LOAD_SCREEN`.

### 4.3 Login and logout

A character saved aboard is saved with the deck map id and deck-local coordinates. On login his
client must be sent to the **world** map the ship sails — it has no terrain for the other — and
he himself must be added to the deck map. Those are two different destinations for one login,
which is why a `BoardingMap()` distinct from `GetMap()` exists at all
([`Player.cpp:5842`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/Player.cpp#L5842)).

*Failure signature:* logging in aboard drops the character into the sea, or into an infinite
loading screen, or onto a hull at the origin.

### 4.4 The map crossing

A ferry between two continents genuinely changes the client's map, so the passengers'
**clients** must be transferred while the passengers themselves never leave the deck map. The
transfer packet names the map being left, so it has to be sent *before* the hull moves.

*Failure signature:* passengers arrive on the far continent standing in water at the ship's
last known pose, or the ship arrives without them.

### 4.5 NPCs on a deck

They are ordinary creatures on an ordinary map. `.npc add` aboard writes the deck map id into
the spawn row and the deck-local position into the coordinates — nothing special happens, which
is the point. Two things do need care:

- Their create blocks must name the vessel as the movement parent, derived from *the map the
  unit stands on* rather than from movement state, because a creature has no client to speak
  for it. See the `Transport::VesselOf` branch in
  [`ObjectUpdate.cpp`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/ObjectUpdate.cpp#L548).
- Observers ashore cannot reach them by any distance sweep, since they are on a different map.
  They need a relay.

There is also a client-side landmine worth knowing about: certain `CreatureTypeFlags`
combinations kill every client that can see a creature the moment its transport moves — see
`CREATURE_TYPEFLAGS_TRANSPORT_FORBIDDEN` in
[`SharedDefines.h`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Server/SharedDefines.h#L3274). That note is explicit about what is
proven (1.12, 3.3.5a) and what is inference (18414); treat it accordingly.

### 4.6 The create block is not the only packet that names a position

This is the seam that cost the most, and it is worth reading even if you disagree with
everything else here, because the trap it depends on is not specific to transports.

`MoveSplineInit` derived a unit's movement parent from `unit.GetTransportInfo()` — the *old*
model's passenger roster. Under a vessel-as-map design nothing files a deck creature there any
more; the only surviving caller of `BoardPassenger` is
[`Vehicle.cpp:257`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/Vehicle.cpp#L257),
so `TransportInfo` has quietly become the vehicle system's alone.

**The old accessor did not disappear. It compiles, it is called, and it returns `NULL`.** So
`SMSG_MONSTER_MOVE` went out with an empty transport guid while its path points were deck
coordinates, and the client read them as world ones.

Eighteen such packets in one capture, each starting at a deck-local position:

```text
50 2B C3 40  E7 3F 81 C0     z = 6.0990   x = -4.039
60 24 C3 40  83 35 09 C1     z = 6.0983   x = -8.575
```

`z` matches to four decimals the deck height in the spawn row `.npc add` had written.

Two things about this are worth more than the fix:

- **The ship does not have to be moving.** Screenshots of it moored showed the same behaviour,
  which rules out the tempting story that the hull sails away from a fixed point. A five-yard
  wander target on a deck, read as a world coordinate, is a point near the map origin —
  thousands of yards away — so the creature walks off in a straight line and never arrives. If
  you find yourself explaining a transport symptom by the ship's motion, check whether it
  reproduces at the dock first.
- **A create block that is right proves nothing about the rest.** The creature was re-created
  correctly at every seam and snapped back onto the deck, then left again immediately, because
  the spline re-sent on visibility gain
  ([`Unit::SendCurrentSplineTo`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/Unit.cpp#L6314))
  had the same hole. Three separate emitters needed the same derivation:
  [`MoveSplineInit::Launch`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/movement/MoveSplineInit.cpp#L92),
  `MoveSplineInit::Stop`, and that one.

The general shape, which is the transferable part: when a frame changes, **inventory every
packet that carries a position or names a parent, and check each one separately.** Fixing the
create block is the visible half; the movement stream is the half that is still wrong
afterwards, and it fails in a way that looks like an AI or pathing bug rather than a framing
one.

*Failure signature:* a boarded creature leaves the deck in a straight line and shrinks into the
distance; re-appears aboard whenever it is re-created, then leaves again.

*Known open at time of writing:*
[`Unit::SendHeartBeat`](https://github.com/H0zen/mangos_four/blob/c07d822d95f5f0b8e30dd75f77b34f2ba71ed24e/src/game/Object/Unit.cpp#L449)
serialises `m_movementInfo` raw, and a deck creature's carries no transport data either — the
parent is derived in this model, never stored. It is reached from the minion transfer. Left
alone deliberately: unproven, and touching `MovementInfo` serialisation deserves its own
evidence. Recorded here rather than fixed on a guess.

### 4.7 Minions, pets and totems

A pet whose master boards has to follow across a map boundary, which is the same move it makes
following him through a portal. Reconciling this once per tick, rather than hooking a boarding
event, is worth considering: there are half a dozen ways a minion comes to be standing on a
deck (master walks aboard, summoned at sea, master logs in mid-voyage, totem dropped on the
forecastle) and only one way to be sure none was missed.

A totem is a reasonable exception — it stays on whatever it was planted on.

*Failure signature:* a pet swimming up through the hull, or standing on the pier while its
master sails away.

### 4.8 Visibility across the boundary

A vessel and the shore she sails past are two maps that must see each other. Distance tests
across that boundary flicker, because the two sides disagree about where the hull is — the
problem from §1, arriving by a different door.

An edge-triggered rule avoids it: **you have a vessel, and everyone on her, for as long as you
share the map she sails.** Membership, not distance. It is worth checking whether that holds up
under your own testing, because it removes the flicker rather than damping it.

*Failure signature:* the ship and her crew popping in and out as she passes, or a man on a pier
watching a ship come in with an empty deck.

### 4.9 The deck needs terrain

The hull's baked model becomes the map's terrain, so height, collision and pathing aboard are
answered by the ordinary engines. A vessel whose hull was never baked has no deck: it must
carry nobody, rather than dropping whoever steps on into nothing.

*Failure signature:* `map <id> has no baked terrain` at start-up, then players falling through
the ship.

---

## 5. Symptom table

Offered as a shortcut, not as doctrine. Each row is a guess to test first, not a diagnosis.

| Symptom | First thing to check |
| --- | --- |
| Client dies with `ERROR #132` when something appears aboard | map id in the `SMSG_UPDATE_OBJECT` body |
| Login-style loading screen on the same map | a self create block re-sent to a client already in the world |
| Range/melee failing intermittently only aboard | a world position being composed somewhere |
| Ship or crew flickering from the shore | a distance test across the map boundary |
| Boarded creature walks off the deck in a straight line and shrinks away | the spline packets, not the create block — does `SMSG_MONSTER_MOVE` name a parent? |
| Any of the above reproducing while the ship is moored | you are not looking at a motion bug; it is a framing one |
| Pet on the wrong side of the gangplank | minion reconciliation |
| Character logs in swimming | login destination vs. client destination |
| Every client watching one particular NPC dies when the ship starts | that creature's `CreatureTypeFlags` |

---

## 6. What would falsify all of this

Stated plainly, because a document that cannot be wrong is not worth reading:

- **A server-side hull pose that provably agrees with the client's instant by instant.** If the
  server can be made to reproduce `TransportAnimation.dbc` interpolation exactly, and that can
  be demonstrated rather than asserted, then composition becomes sound and §3's whole table
  collapses. The bar is agreement at every tick a range check runs, not on average.
- **A measurement showing the composition error is bounded and small.** Experiment 1 in §3
  produces this number. If it comes back near zero on a ship at speed and on turns, the argument
  from §1 is wrong and should be struck.
- **A client that tolerates an unknown map id** in `SMSG_UPDATE_OBJECT`. §2(c) is one crash on
  one build; more data would either harden it or overturn it.
- **A transport map row in `Map.dbc` that does carry terrain.** That would mean the client
  expects to load these maps after all, and §2(b) would need rewriting.

If you disprove one of these, please amend this file rather than deleting it — a wrong claim
with a date on it is more useful to the next reader than a gap.

---

## 7. A closing suggestion about method

Twenty years of accreted workarounds read as requirements once you have stared at them long
enough, and "this is how the tree does it" is not evidence. The transport code in any
MaNGOS-lineage repo is unusually dense with this: it is a large body of work built to make one
unsound composition survive contact with every consumer of a position, and it very nearly does.

Two habits seem to help more than any particular conclusion in this file:

1. **Prefer the formulation that makes the defect unwritable over the one that avoids it.** If a
   position aboard cannot be composed because there is nothing to compose it with, the class of
   bug is gone rather than guarded.
2. **When a comment in this tree names a concrete symptom** — "removing this causes X", "kills
   every client" — treat it as evidence about the system and disprove it with evidence, not by
   failing to derive a mechanism from the source. Several of the notes referenced above were
   written by someone who had already paid for the information.

Neither of those requires agreeing with §3. They are just cheaper than the alternative.
