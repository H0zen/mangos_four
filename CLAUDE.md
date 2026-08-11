# CLAUDE.md

Context for AI assistants — the Claude GitHub App (`@claude`) and contributors using Claude — working in
this repo. Humans: also read [`doc/CodingStandard.md`](doc/CodingStandard.md).

## Project

**MangosFour** — The Mists of Pandaria World of Warcraft **5.4.8** server (C++, MySQL/MariaDB), client build
**18414**. Compatibility target is **5.4.8 only**; do **not** introduce 6.x/Warlords of Draenor or
later-expansion assumptions. The client’s data files are tagged 18273 while the wire protocol is 18414 — see
the `EXPECTED_MANGOSD_CLIENT_BUILD` / `EXPECTED_MANGOSD_WIRE_BUILD` note in `src/game/Server/SharedDefines.h`.

- **Database changes go in the separate `mangosfour/database` repo**, not here — as transactional, idempotent
  `Rel##_##_###_*.sql` migrations that chain via `db_version`.
- Clone/update **recursively**: `src/modules/SD3` and `src/modules/Eluna` are submodules. Never shallow-update
  a submodule to a non-tip pinned SHA.
- Two further submodules live here: `dep` (→ `mangos/mangosDeps`) and `src/realmd` (→ `mangos/realmd`).
- Less-obvious locations: scripting in `src/modules/` (Eluna = Lua, SD3 = C++, Bots = playerbots). The
  `src/game/` tree is under an ongoing **decomp cohesion-split** (large classes like `Player`/`Unit`/`SpellEffects`
  are being broken into topical `*.cpp` files, e.g. `UnitCombat.cpp`, `UnitAura.cpp`,
  `SpellEffectSkillEnchantPet.cpp`, `ObjectMgrCreatures.cpp`); locate code by symbol/string, not a fixed file,
  because methods move between files.

## Build & test

**C++17** — strict (`-std=c++17`, GNU extensions off); C code is C11. CMake ≥ 3.18; GCC/Clang
(Linux/macOS/BSD) or MSVC ≥ 2015 (Windows). The exact flags CI builds with:

```sh
git clone --recursive https://github.com/mangosfour/server.git && cd server
sudo apt-get install -y git cmake make build-essential \
  libssl-dev libbz2-dev default-libmysqlclient-dev libreadline-dev   # Debian/Ubuntu deps
mkdir -p _build _install && cd _build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../_install \
  -DBUILD_TOOLS=1 -DBUILD_MANGOSD=1 -DBUILD_REALMD=1 -DSOAP=1 \
  -DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD3=1 -DPLAYERBOTS=0 \
  -DWITH_TESTS=1 -DWITH_NET_TESTS=0 \
  -DPCH=0
make -j"$(nproc)" && make install -j"$(nproc)"
```

Windows: use the EasyBuild helper. **A PR MUST keep CI green:** the Linux build compiles with **both** GCC and
Clang, Windows builds on AppVeyor, and Codacy/CodeFactor gate quality. `PLAYERBOTS` defaults **OFF**; only
enable it deliberately. A full `make install` also installs the extractor tool, so `BUILD_TOOLS`
targets must build before installing.

## Code style

Source of truth: [`doc/CodingStandard.md`](doc/CodingStandard.md). Non-default rules:

- **4-space indent, never tabs**; ~80-column lines.
- **Allman braces**, and **YOU MUST brace single-statement blocks** — even one-line `if`/`for`/`while`. Do
  not de-brace existing ones. (Exception: do not brace `switch`/`case` bodies.)
- **One space before `(`, none inside**: `if (x)`, not `if( x )`.
- Doxygen: `///` above a member, `///<` trailing, `/** ... */` multi-line.

## Logging

Console output is rendered on a dedicated off-thread writer (`src/shared/Log/ConsoleLogWriter`) so the
world/map-update threads never block on console I/O. Two rules follow:

- **Never write to stdout directly** (`printf`/`fprintf`/`std::cout`, progress bars, ad-hoc notices) for
  console output — route it through `Log::ConsoleEmitRaw` so stdout has a single owner and lines can't tear
  against, or overtake, the writer's output.
- **Gate high-volume runtime debug** with `DEBUG_FILTER_LOG(LOG_FILTER_*, …)` (or `DETAIL_`/`BASIC_`),
  reusing an existing `LogFilters` bit where one fits (e.g. `LOG_FILTER_GRID_ADD`, `LOG_FILTER_DB_SCRIPTS`,
  `LOG_FILTER_MAP_LOADING`). All filters ship **default-on (suppressed)**; set a `LogFilter_*` key to `0` to
  see a category. **Never filter `outError`/`outErrorDb`** — errors must always show.

Recommended runtime mode: `LogLevel=1` (quiet console) + `LogFileLevel=3` (buffered full file). Packet
logging is opt-in via `PacketLoggingEnabled` (off by default).

## MoP 5.4.8 wire specifics

Three build-18414 rules that are invisible from the code alone and each cause silent, hard-to-diagnose
failures:

- **Update fields are re-indexed for the client.** `src/game/Object/ObjectUpdate.cpp` maps engine field
  constants to 18414 client indices (`UNIT_NPC_EMOTESTATE` → 89, `UNIT_FIELD_HOVERHEIGHT` → 154). Setting a
  field the client cares about without adding it to that projection means the value never reaches the client,
  even though `SetUInt32Value` succeeded and the server state is correct.
- **`IsEnterWorldConverted` is the send gate, not `DefS`/`DefC`.** `WorldSession::SendPacket` drops any opcode
  missing from that switch while `m_suppressWorldSends` is raised. `DefS`/`DefC` in `Opcodes.cpp` is **logging
  metadata only** — it names a packet in the log, it does not admit it. An unadmitted packet is built, silently
  dropped, and the command still reports success.
- **CMSG and SMSG are separate opcode number spaces.** The same value legitimately means different things per
  direction — `0x09D3` is both `CMSG_MOVE_GRAVITY_DISABLE_ACK` and `SMSG_SET_PCT_SPELL_MODIFIER`. Always join
  on numeric value **and** direction; matching on value alone invents bugs that do not exist.

When establishing a packet layout, the client binary outranks decoded payload bytes, which outrank corpus
aggregates, which outrank reference forks. A fork value is a hypothesis, never a conclusion. Payload length
proves only `1 + popcount(mask)` for a bit-packed body — it cannot distinguish byte order, so a layout is only
verified by a byte-exact fixture built from a real captured body.

## Review focus (for `@claude`)

Prioritise: **(1)** correctness/safety in `src/game/` handlers and anything touching live world/DB state;
**(2)** coding-standard conformance above; **(3)** build/CI impact (GCC *and* Clang, Windows/AppVeyor); **(4)**
DB-migration correctness (use the `mangosfour/database` pattern). Keep feedback concrete and minimal-diff; flag
correctness/standard issues, not style preferences the standard doesn't cover.
