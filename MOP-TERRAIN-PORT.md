# Porting 03's terrain engine + unified baker into 04 (5.4.8)

Working notes for the port of `src/shared/terrain`, `src/shared/Geometry`,
`DynamicCollision`, `Column`-based queries and `src/tools/extractor` from
`03-mangos_three` (4.3.4) into `04-mangos_four` (5.4.8). Not committed advice --
a record of what was established, what diverges deliberately, and what is deferred.

## Sources used

- `03-mangos_three` working tree at `06c16801a` (its own history for rationale).
- wowdev.wiki `ADT/v18`, fetched 2026-08-03 (via `ficom`; the page 403s the local
  fetcher). Quoted below where it settles a question.
- 04's own legacy extractor, `src/tools/Extractor_projects`, which is the only
  code in this workspace that has ever been pointed at a 5.4.8 client. Its four
  `TODO(MoP)` / `MOP_READINESS` markers are an audit of what 4.3.4 code gets
  wrong on MoP and are treated as findings, not as guesses.
- Prior measurement of the 5.4.8 client on `ficom` (`/mangos/client-548`),
  recorded in the assistant's memory notes.

## Established client-format deltas, 4.3.4 -> 5.4.8

### MCNK header offset 0x14 is NOT an offset any more

wowdev `ADT/v18`, MCNK header:

```c
#if version >= ~5.3
  uint64_t holes_high_res;      // only used with flags.high_res_holes
#else
/*0x014*/  uint32_t ofsHeight;
/*0x018*/  uint32_t ofsNormal;
#endif
```

`flags.high_res_holes` is bit 16 (`0x10000`); `do_not_fix_alpha_map` is bit 15.

Consequences, both measured before on a real 5.4.8 bake:

1. Following 0x14 as `ofsHeight` **crashes** the parser (maps 489, 571, 870).
   The 4.3.4 guard `offsMcvt + 8 + 145*4 <= span` is uint32 arithmetic and wraps
   on a large value, so it passes with a wild pointer.
2. Where it does not crash it **silently zeroes a map's terrain** -- map 571
   baked with a floor of exactly 0.0, which scores as "found a floor".

**Decision for 04: never read 0x14 as an offset.** MCVT is located by walking the
MCNK's sub-chunks (after the 8-byte tag+size and the 128-byte header). The 64-bit
hole map is read from 0x14 only when `high_res_holes` is set.

Both consequences were confirmed first-hand afterwards -- see "Measured against the
real 5.4.8 client" below: the scan finds MCVT in 1,153,792 of 1,153,792 chunks, and
0x14 looks like a usable offset in 264 of them.

Everything else in the 128-byte header is unchanged and is used as in 03:
`0x00` flags, `0x04` IndexX, `0x08` IndexY, `0x34` areaid, `0x3C` holes_low_res,
`0x60`/`0x64` ofs/size MCLQ, `0x68` position (so Z at `0x70`).

### Holes are 64-bit when flagged

4.3.4 has only the 16-bit `holes_low_res` (a 4x4 grid of 2x2-cell blocks). 5.3+
adds a 64-bit map, one bit per height cell, at 0x14.

04's tile format therefore stores holes as **`uint64` per chunk**, and the
low-res map is expanded into it on read (`bit(i*8+j) = (low >> ((i/2)*4 + (j/2))) & 1`).
That makes `IsHole` a straight bit test in both cases and removes the resolution
question from the runtime entirely. This is a deliberate divergence from 03's
`std::array<uint16_t, 256>`; the tile file is a per-core private format, so there
is nothing to keep byte-compatible.

Bit order follows the convention already in this codebase (first index derived
from MCNK `IndexY` and resolved against world X), which is what 03's working
baker uses; the high-res case extends it as `bit = (ix % 8) * 8 + (iy % 8)`.

**That extension is an assumption, stated here so it can be found again.** The wiki's
rule for the fine map is "read the 8 bytes as an array and test `(Holes[row] >> col) & 1`",
and reading them with `RdU64` makes byte `row` occupy bits `row*8 .. row*8+7`, so the
uint64 form needs no row inversion. What is inherited rather than proven is that "row"
means the same axis in the fine map as in the coarse one -- the coarse convention here
is 03's, validated by a working bake. If holes ever appear transposed within a chunk,
this is the line to swap, and nothing else.

### MH2O is unchanged; the LiquidObject indirection is not optional

`SMLiquidChunk` / `SMLiquidInstance` / `mh2o_chunk_attributes` are identical in
4.3.4 and 5.4.8. Two rules the wiki states that 03 only half-applies:

- `liquid_object_or_lvf >= 42` is a `LiquidObject.dbc` id, resolved
  `LiquidObject -> LiquidType -> LiquidMaterial -> LVF`. 03 substitutes a
  heuristic ("ocean rows are depth-only") because it did not load those two DBCs.
- "The above four members [x_offset, y_offset, width, height] are only used if
  `liquid_object_or_lvf <= 41`. Otherwise they are assumed 0, 0, 8, 8."
- "if `offset_vertex_data` = 0 and `liquid_type != 2`, then let LVF = 2".

04 loads `LiquidObject.dbc` and `LiquidMaterial.dbc` and does the real chain.

### `_obj1.adt`

Exists since 5.0.1 and holds LOD doodad batches. Measured on `Northrend_30_16`:
its MWMO/MWID/MODF are byte-identical to `_obj0`'s while its MDDF holds 36
doodads against `_obj0`'s 936. It adds no building the root set lacks, so
collision comes from `_obj0` only and `_obj1` is never opened.

MODF is 64 bytes and MDDF 36 bytes in 5.4.8, unchanged from Cataclysm.

### Map.dbc

`nsiiisissififfiiiii` in 5.4.8 against Cata's `nsiiiisissififfiiiii` -- one `i`
fewer early, so every field after index 4 shifts. `Directory` is index 1 in both.

### Archives

5.4.8 groups archives by CONTENT TYPE, not by world region: `base-Win`, `world`,
`model`, `texture`, `itemtexture`, `interface`, `misc`, `expansion1..4`, `sound`,
plus `locale-<loc>.MPQ`. Eighteen `wow-update-base-*` builds (16016 -> 18273) and
eighteen matching `wow-update-<loc>-*`, same PTCH machinery as 4.3.4.

### The update chain must patch ITSELF, not only the bases

Attaching every update to the base archives is not enough. A file added after
release exists in **no base**, so the base chain cannot resolve it: it appears as a
whole file in one update and is then patched **incrementally** by the later ones.
`Read()` rejects a `PTCH` blob and walks on, which for such a file lands on the
**oldest whole copy** -- a beta.

Measured on the 5.4.8 client by serving every file BOTH ways and comparing:
**1374 files across 29 maps** resolve differently -- 417 ADTs, 283 WMOs, 180 M2s, 21
`.db2`, 6 `.dbc`, and **nothing regresses**. All post-5.0 content: Siege of Orgrimmar,
Throne of Thunder, Isle of Thunder, Deepwind Gorge, every scenario.
`GoldRushBG_31_32.adt` served the 16516 beta (304212 B, a flat plate at z=0/100/145,
2 KB of objects) instead of the shipped map (475741 B, 100 KB of objects).

**Count this by serving the files, not by reasoning about the archives.** Two indirect
estimates were tried first and BOTH overcounted: 28062 (every file whose newest copy is
a delta -- ignores that most have a whole copy in a base, which carries the chain) and
then 3906 and 2924 from two different narrowings of that. The only honest instrument is
running both wirings and diffing what each one hands back.

Fix: open the updates ascending and also attach each onto every EARLIER update
handle. 153 extra attachments for 18 updates, ~330 MB RSS, seconds.

## Deliberate divergences from 03 (do NOT "align" these away)

| what | 03 | 04 | why |
| --- | --- | --- | --- |
| MCVT location | `ofsHeight` at MCNK 0x14 | sub-chunk scan | 0x14 is `holes_high_res` in 5.3+ |
| tile holes | `uint16` per chunk | `uint64` per chunk | 5.3+ high-res hole map |
| LVF for `>= 42` | ocean-row heuristic | LiquidObject DBC chain | MoP relies on it far more |
| zone resolution | `entry->ParentAreaID` once | `MopTerrain::ResolveRootAreaId` | MoP nests areas deeper; 04 already has this and it must survive the port |
| MH2O rect for `>= 42` | stored rect honoured | forced to 0,0,8,8 | the client's rule; a no-op on real data, see the test |

## What the port actually did, beyond copying 03

- **`ModelIgnoreFlags` survived.** 03 deleted the spell-side "ignore M2 doodads" LoS
  filter along with the vmap library and left a note that a `ModelKind::Mesh` skip would
  be the way back. 04 implements exactly that: the flag moved to
  `terrain/ICollisionModel.hpp`, `FusedTerrain::NearestHitFraction` takes it, and a
  baked tile already distinguishes `ModelKind::Mesh` (an M2 hull) from
  `ModelKind::Wmo`. Deleting it would have been a silent behaviour regression in a
  feature 04 deliberately has.
- **`.debug los` was rewritten, not dropped.** It reports the blocking fraction and the
  point it lands on. It no longer names the blocking model: a baked tile holds geometry,
  not model paths, so a name printed there would be invented.
- **`VMapFactory`'s two unrelated statics went where they belong**: the config id-list
  parser to `shared/Utilities/IdList.h`, the spell LoS exemption set to
  `game/WorldHandlers/LineOfSightExemptions`, and `VMAP_DISABLE_*` to
  `DisableMgr::CollisionDisableFlags`.
- **`GridMapLiquidData::CreatureTypeFlags` is now `type_flags`.** The field holds
  `MAP_LIQUID_TYPE_*` bits and never had anything to do with `CreatureInfo`'s field of
  the same name; five call sites.
- **`vmap.enableLOS` / `vmap.enableHeight` are gone rather than ignored.** One fused
  tile carries terrain and collision, so there is nothing left that could be switched
  off independently.

## The one that would have failed silently: mmtile axes

`MoveMap.cpp` opened `mmaps/%04u%02i%02i.mmtile` with `(mapId, x, y)`. The new baker
names it with `(gy, gx)` -- Recast X is world Y, so navmesh tile coordinates are
swapped relative to the grid (`NavMeshBuilder.hpp` states this; `NavMeshBuilder.cpp`
writes `navTileX = gy`). Left alone, every navmesh tile would have failed to open on a
non-symmetric map and the server would simply have had no pathfinding, logging only a
DEBUG_FILTER line nobody has on.

03 fixed this in `[Core] Open mmtiles with the axes the baker named them by (#311)`; the
same change is now in 04, along with the `std::string` filename helpers that fix the
buffer measured from the format string rather than from the widest map id.

## The bake, scored

Baked maps 489, 870, 530, 0, 1 and 571 -- 4507 tiles, 4.5 GB, plus 5530 game-object
models and 159 MB of DBCs. No tile failed.

Scored with `mangos-height-check` against **20,560 ground-only creature spawns** taken
from `four_world` (`InhabitType & 1 AND (InhabitType & 6) = 0` -- a creature that can
neither fly nor swim is standing on the floor, so its `position_z` IS the expected
floor):

```text
map      probes    <=0.5      <=2      <=5     <=20      >20 NO FLOOR   mean|d|
0          6238     6120       75       32        1        2        8      0.13
1         10225     9802      194      153       38       22       16      0.29
530        3443     3202       94      137        7        3        0      0.98
571         585      526       24       34        1        0        0      0.30
870           69       58        7        2        0        2        0      4.21

ALL       20560    19708      394      358       47       29       24      0.37
```

**97.89% within 2 yards** of the authored Z; 24 probes of 20,560 (0.12%) found no floor
at all. The 29 gross misses are individual spawns, not a map or a format: map 870's mean
of 4.21 is two outliers over 69 probes, and the single worst -- 1254 yards on Outland --
sits over a void where the terrain really is at -1189.

For contrast, the earlier 5.4.8 experiment recorded in the assistant's notes scored
**57% within 2 yards** on a 7-point corpus of player positions.

### Confounders, separated rather than averaged in

`mangos-height-check` now partitions the score three ways, because a corpus recorded
from a running client lies in three directions and all three inflate a score:

```text
  over liquid   : 798 of 20536 (3.9%), mean|d| 0.45
  on model floor: 2804 of 20536 (13.7%)
  expected == z : 20536 of 20560 (99.9%)

dry ground only: 98.01% within 2 yards over 19738 probes, mean|d| 0.37
```

- **Over liquid.** A *client* settles a swimming body at the SURFACE, so such a probe
  can never match a floor. A *database* spawn is on the floor under the water, which is
  why these still score 0.45 here. The tool flags them either way; the headline number
  is now the dry one.
- **On a model floor.** 13.7% of the corpus lands on baked WMO geometry rather than the
  heightmap -- a different half of the engine (placement transform + model raycast) from
  terrain interpolation. Mixing them hides which half is wrong. That this share passes
  is real evidence for the placement code, and it was previously invisible.
- **`expected == z`.** 99.9% here, and that is CORRECT for a database corpus: the query
  selects `position_z` twice. For a client-recorded corpus the same line means the probe
  was read before the client finished correcting, and it would agree with any terrain
  model whatsoever. The tool says which reading applies rather than crying wolf.

For a client corpus the input wants to be `name, map, x, y, sent_z, settled_z` so the
movement check has both numbers to compare.

### What this corpus does NOT prove

**`four_world` carries many spawns inherited from Cataclysm**, so a large share of these
probes were authored against 4.3.4 terrain rather than 5.4.8. That is survivable for the
score -- the old continents' heightmaps barely moved between the two -- but it changes
what the number means in two directions:

- On maps 0, 1, 530 and 571 it is a **lower bound** on the baker. Where Mists actually
  re-authored terrain, a Cata-era spawn *should* disagree, so an unknown slice of the
  2% that miss is stale database rather than bad geometry.
- On **map 870 it proves almost nothing**: 69 probes, all Spirit Healers, for a continent
  that did not exist before Mists.

The instrument that would settle Pandaria is a set of positions read out of a running
5.4.8 client. Until that is scored, "the bake is right for Mists-authored terrain" is
an extrapolation from the old continents, not a measurement.

### Then the client corpus was scored, and it found the archive defect

2164 positions recorded from a running 5.4.8 client, over 111 maps, baked against all
288: **97.44% within 2 yards**, dry-only 97.84%, mean 1.48, 15 NO FLOOR.

One map carried nearly all of it. **1105 GoldRushBG: mean 94.95, 13 of 40 NO FLOOR,
not one probe within 0.5 yd**, and its bake logged `SOME FAILED`. Chasing that single
map is what exposed the update-chain defect above -- the parser, the placement
transforms, the tile axes and the hole map were all cleared first, and the answer was
that the *bytes were from the wrong build of the file*.

After the fix, rebaked whole (**9724 tiles, no map with a failed tile**) and rescored on
the same corpus:

```text
             probes    <=0.5      <=2      <=5     <=20      >20 NO FLOOR   mean|d|
before         2164     1994       67       36       27       25       15      1.48
after          2164     2113       21       14        8        6        2      0.29
```

- **98.70% within 2 yards**, dry-only **99.14%**, mean **0.29**.
- NO FLOOR down from 15 to **2** (0.09%).
- Map 1105 alone: **100% within 0.5 yd, mean 0.00**, from mean 94.95.
- Only two maps retain any error at all: 870 (6 of 291 probes) and 1008 (1 of 6).

This is the measurement that the four_world section above asked for, and it now covers
Pandaria properly: map 870 has 291 client probes against the 69 Cata-era spawns.

### And the liquid, which is what the ocean fix was about

Over the same 20,560 probes: 19,221 dry, 729 water, 557 ocean, 28 slime, 1 magma.

- **All 557 ocean surfaces sit at exactly 0.00** -- sea level, as they must.
- **No surface anywhere exceeds 100,000 in magnitude.** That is the direct test for the
  LiquidObject-42 defect: a depth block read as floats produces ~1e11, and there is none.
- Slime resolves as slime at -64.48 (Undercity), not as swimmable water -- the
  LiquidType family classification survives the port.
- Water levels are ordinary and varied (18.27, 21.23, 288.09, 1313.66, -12.62 ...),
  which is what a real heightmap looks like and what a mis-read depth map does not.

## The test suite, which 04 did not have

04 had no `src/tests` at all. The terrain and parser subset is now ported from 03 --
`ClientParserTest`, `TerrainModelTest`, `TileSerializerTest`, `ModelMapTest`,
`NavBinningTest`, `DynamicCollisionTest`, `PlacementTest`, plus the harness. They read
synthetic byte buffers through `MemoryArchive`, so they need no client and no database
and can run where the 5.4.8 corpus cannot. **107 tests, 0 failed** on clang.

Three cases were ADDED for the deltas this port introduced, because measuring a client
once proves a thing today and a test proves it after the client is gone:

- `AdtMcvtFoundWhenHeaderOffsetIsGarbage` -- writes `0xDEADBEEF` over the old
  ofsHeight/ofsNormal pair and requires the heights to arrive anyway.
- `AdtHighResHoleMapIsReadWhenFlagged` -- marks the diagonal, which no 2x2-block map
  can express, and requires it back cell for cell.
- `AdtMh2oLiquidObjectCoversTheWholeChunkWhateverTheRectSays` -- the 0,0,8,8 rule.

`AdtHolesAndAreaId` was rewritten to assert holes cell-by-cell rather than as a raw
`uint16` mask, since 04 stores the fine map.

### One test disagreed, and it was worth stopping over

`AdtMh2oNonOceanLiquidObjectKeepsHeights` failed: 03 gives the layer a 2x2 rectangle,
and 04's "a LiquidObject covers the whole chunk" rule overrides it. Both behaviours are
defensible from the file alone, so the measurement decided it -- **all 707,582
LiquidObject instances in the 5.4.8 client store 0, 0, 8, 8**, so the rule is a no-op on
real data and the test was exercising a file the client never writes.

Resolved by splitting the two questions rather than weakening either: that test now uses
a full-chunk rectangle and still asserts what it was written to assert (a non-ocean
LiquidObject keeps its heights), and the rectangle rule gets its own test that records
why it exists. `AdtMh2oHonoursSubRectAndExistsBitmap` -- the sub-rectangle case that
*does* occur, on a plain vertex format -- was untouched and still passes.

## SETTLED BY MEASUREMENT: #250's Recast tuning, three of its five values restored

`[Mmap] Fix navmesh tile corruption from double border removal (#250)` did two things to
the old `Movemap-Generator`. When `NavMeshBuilder` replaced it in "United cores" (#306),
only one came across.

**Carried, and better than carried.** The double border removal itself is gone because
there is no manual "remove padding" loop in the new builder at all -- it never subtracts
`borderSize` a second time, so the corruption #250 describes (polygons shifted 1.33 yd,
tile-edge vertices underflowing to ~17000 yd, Detour unable to stitch neighbours) cannot
occur. Unwritable rather than fixed.

**Not carried: every tuning value #250 changed alongside it.** 04 matches 03 here
exactly -- this is not something the port introduced:

| parameter | pre-#250 | #250 set | `NavMeshBuilder` today |
| --- | --- | --- | --- |
| `walkableClimb` | 2\|4 | 3\|6 | **4** |
| `maxSimplificationError` | 2.0 | 1.8 | **2.0** |
| `detailSampleDist` | `cs*64` | `cs*16` | **`cs*64`** |
| `detailSampleMaxError` | `ch*2` | `ch*1` | **`ch*2`** |
| `rcMedianFilterWalkableArea` | absent | added | **absent** |

The stated symptoms for these are not architecture-specific: the detail mesh sampled
every ~17 yd "flattened hillsides into ramps", and the median filter removed "unwalkable
speckle noise that fragmented regions and truncated paths".

**Not changed on the strength of the commit message.** #250's evidence was measured on
4.3.4 meshes with the old builder; the new one bins triangles into sub-tiles and its
header says its defaults "reproduce the values the server's PathFinder was tuned
against". Changing five Recast parameters with no 5.4.8 measurement is guessing.

### How it is being settled

"Flattened hillsides into ramps" is a measurable claim. The probe samples points across
a map, asks the baked terrain for the floor and the navmesh for its height at the same
place, and reports the deviation **binned by local slope** -- because flat ground
discriminates nothing (any mesh matches a plane) and the whole claim is about slope.

The slope estimate is taken from the terrain itself over one yard, and near a building
it will read a wall as slope. That noise is identical in every run, so it cannot favour
any of them -- which is the point of doing this as an A/B rather than as an absolute
score.

### The result: #250 was right about the geometry and irrelevant about the rest

Map 870, 684 mmtiles, ~30,300 sampled points per run. **A** = values as they stood,
**B** = all five of #250's, **C** = only `maxSimplificationError`, `detailSampleDist`
and `detailSampleMaxError`.

```text
slope band        A mean  B mean  C mean     A >1yd  B >1yd  C >1yd
flat   (<5 deg)    0.269   0.264   0.265       0.9%    0.4%    0.4%
gentle (5-15)      0.703   0.518   0.512      17.6%    3.5%    3.1%
slope  (15-30)     0.903   0.587   0.584      27.9%    5.0%    5.0%
steep  (30-50)     1.151   0.714   0.714      40.5%   11.5%   10.9%

mmap size on disk   326M    372M    372M
```

- **#250's claim holds and is large.** Points wrong by over a yard fall roughly 5x on
  gentle slope and 3.5x on steep. On flat ground the runs are indistinguishable, exactly
  as they must be -- which is also the check that the instrument is not flattering
  anyone.
- **C is B.** Every gain comes from the three geometry values. `walkableClimb` 4 -> 6 and
  `rcMedianFilterWalkableArea` change nothing this probe can see.
- **So three landed, two did not.** `walkableClimb` governs what step an agent may climb
  and the median filter governs region fragmentation; both are connectivity, which this
  probe does not measure. Landing them on the strength of a height score would be
  exactly the guess this section was written to avoid. They stay out until something
  measures a path.
- Cost is +14% on disk and nothing at runtime -- Detour reads whatever it is given.

Coverage was equal throughout (3196 / 3164 / 3195 sampled points with terrain but no
navmesh polygon within 3 yd), so the accuracy gain does not come from the mesh quietly
covering less ground.

## Known gaps, recorded rather than fixed

- **A baked tile carries no client build.** The old `.map` header had `buildMagic`, which
  `GridMap` checked against `EXPECTED_MANGOSD_CLIENT_BUILD` (18273 for this client). The
  tile format has magic and version but no build, so a tile baked from a different client
  would load without complaint. The DBC stamp check (`component.wow-<locale>.txt`) still
  runs and is the stronger of the two, which is why this is recorded and not patched.
- `linux/getmangos.sh` now drives the single baker; the three-way "DBC / Vmaps / Mmaps"
  checklist is gone because the components feed each other and are baked in order.

## Verified

`game`, `mangos-extractor` and `mangos-height-check` build clean on **clang 19.1.7 /
FreeBSD** (ficom), zero errors. Not yet built on GCC (licom) or MSVC.

Two latent faults the port exposed rather than caused, both fixed:

- `ObjectMgrPhases.cpp` used `sConditionStorage` without including `SQLStorages.h`; it
  had been reaching it transitively through the vmap header chain.
- `Map.cpp` used `GameObjectModel` through `DynamicCollision.h`'s forward declaration.

### Measured against the real 5.4.8 client, not assumed

A throwaway probe walked every ADT of maps 0, 1, 489, 530, 571 and 870 -- 4507 tiles,
1,153,792 MCNKs, 707,582 MH2O instances. Three assumptions were up for test and all
three are now settled:

| map | tiles | MCNK | MCVT found by scan | 0x14 looked like a valid offset | high-res holes | low-res holes |
| --- | --- | --- | --- | --- | --- | --- |
| 0 Azeroth | 839 | 214784 | **214784** | 1 | 2816 | 616 |
| 1 Kalimdor | 1011 | 258816 | **258816** | 1 | 18432 | 454 |
| 489 PVPZone03 | 16 | 4096 | **4096** | 4 | 1536 | 0 |
| 530 Expansion01 | 826 | 211456 | **211456** | 245 | 9216 | 769 |
| 571 Northrend | 1131 | 289536 | **289536** | 1 | 2816 | 714 |
| 870 HawaiiMainLand | 684 | 175104 | **175104** | 13 | 29184 | 327 |

1. **The sub-chunk scan finds MCVT in 100% of chunks** -- 1,153,792 of 1,153,792.
   Following 0x14 as `ofsHeight` would have produced a merely *plausible-looking*
   offset in **264** chunks out of 1.15 million, and a wild pointer in all the rest.
   The 4.3.4 reader does not "mostly work" on Mists data; it never works.
2. **The 64-bit hole map is the dominant form.** 64,000 chunks carry `high_res_holes`
   against 2,880 carrying only the 16-bit map -- so a low-res-only reader would miss
   about 95% of Mists' holes. Widening the tile's hole field was necessary, not tidy.
3. **Every LiquidObject instance stores the rectangle 0, 0, 8, 8.** All 707,582 of
   them, across all six maps, `obj_nonfull = 0`. Applying the client's documented
   "assume 0, 0, 8, 8" rule is therefore a no-op on real data and cannot flood a chunk.
   The assumption is discharged.

**And one thing the probe found that was not being looked for:** on maps 0, 1, 530 and
870 *every single* MH2O instance is a LiquidObject (`mh2o_inst == obj_inst`), while
Northrend -- Wrath-era data carried forward -- has only 4895 of 226,929. Mists-authored
liquid uses the LiquidObject indirection universally. 03's ocean-row heuristic would
have mis-resolved the vertex format of essentially every liquid instance on every map
Mists authored, so loading `LiquidObject.dbc` and `LiquidMaterial.dbc` was not an
optional refinement.

### The defect the measurement caught: LiquidObject 42 is not a DB lookup

The DBC chain was written from the wiki and looked right. Measuring it against the data
said otherwise.

`LiquidObject.dbc` loads with 1501 rows and `LiquidMaterial.dbc` with 7, and the chain
resolves every row to LVF 0 (1415 rows) or LVF 1 (86) -- **never** to LVF 2. Depth-only
liquid therefore cannot come out of the chain at all. Yet the most common instance in
the game is `(liquid_type 2, liquid_object 42)`, 452,957 of them, and the vertex block
of every one of the 93,536 that has one **measures exactly 1 byte per corner** -- a
depth map.

The chain sent it the other way: row 42's `LiquidTypeID` is 0, so the fallback took the
instance's own type 2, whose material is 1, whose LVF is 0 -- height-first. That reads
uint8 depths as floats and bakes ocean surfaces around 1e11, a value nothing downstream
rejects because it is a perfectly ordinary float.

The client never asks the DB for it. `Liquid::RegisterLiquidObject` short-circuits:
`LO = 42 || LT = 14 -> oceanLiquidObject`. Object 42 IS the ocean. That special case is
now in `LiquidObjectStore::VertexFormat` ahead of the lookup, and in the no-DBC fallback
in `AdtParser.cpp`. After the fix `(type 2, obj 42)` resolves to LVF 2, and the non-ocean
objects still resolve to 0 and 1 -- matching the 5 and 8 bytes per corner measured for
them.

Two more things fell out of the same measurement and are worth having written down:

- `min_height_level` is **exactly 0.0** in all 452,957 LiquidObject-ocean instances and
  all 222,034 plain-LVF-2 ocean instances. So the wiki's "case 2 is always at 0.0, not
  `*_height_level`" and the parser's "use `minHeight` when there is no heightmap" are
  the same statement on this data, and the parser needs no change.
- 359,421 of the ocean instances carry **no vertex block at all**, which is why the
  `!hasVertexData && liquidType != 2` rule must NOT fire for ocean: it would be reading
  a rule meant to catch missing data as if it described the format.

## Deferred / not in scope

- `TransportMap` (vessels) is 03-only and is NOT part of this port. `ModelTileSource`
  and `VESSEL_MAP_BASE` come across with the terrain library because they are part of
  it, but nothing in 04 uses them yet.
- 03's `src/tests` (`ModelMapTest`, `TileSerializerTest`) and `src/tools/height-check`
  are not ported in this pass.
