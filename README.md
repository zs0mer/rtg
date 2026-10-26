# BACKBLAST

A fast, endless **terminal recoil platformer**. No walking. No jumping. Just a
ridiculously overpowered shotgun, momentum, and a melting reactor below you.

## Jump in

From this directory, **`nix run .`** builds and launches the game. No external
libraries, assets, network services, or account required. The first Nix build
needs network access to fetch the toolchain; subsequent runs use the cache.

For development, enter **`nix develop`**, then configure with
**`cmake -S . -B build -G Ninja`**, build with **`cmake --build build`**, and play
with **`./build/backblast`**. Run tests with **`ctest --test-dir build --output-on-failure`**.
All compilation and testing should happen in the declared Nix environment.

Use a UTF-8, true-color terminal supporting SGR mouse events (kitty, Alacritty,
WezTerm, foot, xterm, or the VS Code integrated terminal). Minimum size is
**72 columns × 24 rows**; **110 × 38 or larger** is recommended. Reduce your font
size for a wider view. Mouse reporting must be enabled if using a multiplexer;
use a direct terminal if it intercepts clicks. This is a terminal game, not a GUI.

## Controls

| Input | Action |
| --- | --- |
| Mouse | Aim the shotgun |
| Hold left mouse button | Fire repeatedly |
| Enter / click | Start immediately |
| P | Pause / resume |
| R / Enter after death | Instantly retry the same course |
| Q / Ctrl-C | Quit |

**Aim DOWN to fly UP.** Shooting pushes you opposite your aim. Aim diagonally
down to cross gaps, sideways to ricochet off walls, and up to brake an ascent or
dive onto a platform. There are deliberately no movement keys or jump button.

Wall bounces retain 78% of sideways speed and shave off 15% of upward speed.
Falling speed is unaffected by wall contact.
The arena is 100 world units wide, with a centered starting ledge and enemy
spawns spread across the full width.

- **Four rechargeable shells, up to six with kills.** Each enemy kill instantly
  adds three shells, capped at six. Safe landings add two shells per touchdown,
  also capped at six; standing on a platform does not repeatedly grant ammo.
  Ammo pickups refill to four without removing bonus ammo. Passive reload restores one shell every
  0.75 seconds, only while below four. Firing cancels reload progress and starts
  the timer over. Chain kills to build an ammo reserve.
- **Enemies drop no items.** Brute kills also instantly restore one hull point,
  capped at three. Pickups generated on platforms remain available.
- **Cyan ledges** are one-way: fly through from below, land from above.
  Platform friction gradually slows horizontal motion instead of stopping it
  instantly. Airborne momentum is preserved when launching off a ledge.
- **Red ledges** have spikes. **Red drones** drift; hunters pursue and shoot.
  Enemy density increases gradually with altitude, with a slower ramp toward
  late-run pressure. Regular enemies keep their normal size at every altitude;
  difficulty ramps through spawn frequency instead of size.
  Rare 12-HP violet brutes only spawn above **1,000m**.
  Shotgun pellets have a small extra collision margin to make near-grazes fairer;
  enemy projectile hitboxes are unchanged.
- **Gold crosses** refill shells. **Green crosses** repair hull.
- **Three hull points**, with brief invulnerability after damage.
- **Meltdown rises after four seconds.** Keep climbing; old terrain disappears.
- **Score is your highest altitude in whole meters.** Kills and pickups do not
  add score; enemy kills reward ammo and brute kills also repair hull.
- Large **HP crosses** and **AMMO bars** sit in opposite bottom corners without
  numeric counters, using fully filled blocks like the platforms. A graphical
  reload bar sits beneath ammo. Best is top-left; current height is top-center.
  Each cross represents one HP and each tall bar one shell.
- A large **STREAK** counter on the right counts consecutive destroyed enemies.
  Its countdown bar drains between kills; after two seconds without a kill,
  the streak resets to zero. Every kill refreshes the timer.
- The background uses your terminal's default background, including its
  transparency if configured. The game does not change terminal opacity.
- Your best height is saved under `$XDG_STATE_HOME/backblast/best-height`, or
  `$HOME/.local/state/backblast/best-height`. Old points-based records are left
  untouched and are not mixed with height records. Quit whenever you like.

Optional **`--seed 42`** makes a repeatable course to compete on; R always retries
the current seed. Omit the option for a randomly generated course each launch.
Competition is score-chasing; there is no online multiplayer or leaderboard.

## Under the hood

C++20, fixed-step 120 Hz physics, ~60 FPS ANSI true-color half-block rendering,
swept pellet hits and landing detection, wall restitution, particle bursts,
screen kick, deterministic generation, bounded live-world storage, and raw
POSIX terminal input. Physics dimensions remain constant across terminal sizes.
The game restores mouse reporting, cursor, alternate screen, and terminal input
on normal exit, Ctrl-C, SIGTERM, and SIGHUP. Resizing below the minimum freezes
the run until the terminal is large enough again.

CMake tests cover recoil direction, cooldowns, airborne reloads, safe landings,
one-way platforms, wall bounce, combat, invulnerability, death, seeded generation,
world culling, and fragmented mouse input. `nix build .` builds a package and
runs the same checks.

For a real pseudo-terminal integration check, run
**`python3 tests/terminal_smoke.py build/backblast`** inside **`nix develop`**.
It exercises mouse firing, pause/resume, resizing (including wide terminals),
retry, score persistence, clean quit, and SIGINT terminal restoration.