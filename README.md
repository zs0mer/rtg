# Zapy

```text
     / /  ______   ___    ____  __  __
    / /  /___  /  /   |  / __ \/ / / /
   / /___   / /  / /| | / /_/ / /_/ /
  /___  /  / /__/ ___ |/ ____/\__, /
     / /  /____/_/  |_/_/    /____/
    /_/
          HIGH VOLTAGE. NO BRAKES.
```

A fast, endless **terminal recoil platformer**. No walking. No jumping. Just a
ridiculously overpowered shotgun, momentum, and a melting reactor below you.

## Jump in

From this directory, **`nix run .`** builds and launches the game. No external
libraries, assets, network services, or account required. The first Nix build
needs network access to fetch the toolchain; subsequent runs use the cache.

For development, enter **`nix develop`**, then configure with
**`cmake -S . -B build -G Ninja`**, build with **`cmake --build build`**, and play
with **`./build/zapy`**. Run tests with **`ctest --test-dir build --output-on-failure`**.
All compilation and testing should happen in the declared Nix environment.

Use a UTF-8, true-color terminal supporting SGR mouse events (kitty, Alacritty,
WezTerm, foot, xterm, or the VS Code integrated terminal). Minimum size is
**72 columns × 24 rows**; **110 × 38 or larger** is recommended. Reduce your font
size for a wider view. Mouse reporting must be enabled if using a multiplexer;
use a direct terminal if it intercepts clicks. This is a terminal game, not a GUI.

## Controls

| Input                  | Action                      |
| ---------------------- | --------------------------- |
| Mouse                  | Aim the shotgun             |
| Hold left mouse button | Fire repeatedly             |
| Enter / click          | Start immediately           |
| P                      | Pause / resume              |
| R / Enter after death  | Start a fresh random course |
| Q / Ctrl-C             | Quit                        |
| T / ]                  | Next neon style             |
| [                      | Previous neon style         |

## Neon styles

Ten coordinated true-color palettes recolor platforms, walls, enemies, brutes,
pellets, particles, lava, crosshair, and HUD: **reactor**, **synthwave**, **acid**,
**glacier**, **inferno**, **sakura**, **ultraviolet**, **outrun**, **matrix**, and
**prism**. Cycle with **T** at any time, including on the start, pause, and death
screens. The current style appears in the bottom control strip.

Themes automatically advance every **750m of maximum height**, smoothly blending
all colors during the final **200m** before each milestone (550–750m,
1,300–1,500m, and so on). The palette holds steady between fades and cycles through
all ten styles. Falling does not reverse the transition. Retry returns to your
selected starting style; automatic changes do not overwrite that preference.

Launch with **`nix run . -- --theme synthwave`**, or list available styles with
**`nix run . -- --list-themes`**. Cycling saves your preference alongside your
height record; the next launch restores it. Styles preserve the terminal's
transparent background and never change physics, enemies, or scoring.

Platforms, hazards, regular enemies, and brutes have theme-specific colors;
enemy bodies, incoming markers, and hostile shots follow theme fades. Ammo stays warm gold/yellow
and health stays vivid neon pink/magenta in every style, including during fades.
Pickup colors match their HUD indicators; shapes and labels stay the same.

## Gameplay

**Shotgun modes:** touch the first big glowing circle at a random height
between **750–1,000m** to randomly switch to either of the other two modes (equal
chances). Each circle also spawns at a random horizontal position inside the arena.
More circles appear as you climb, each a random **1,000–1,500m** above the last,
whether you touch or skip the previous circle. A wide, white marker at
the top edge points to a circle when its lower edge is up to **120 world units** above
the view—four times the 30-unit enemy warning range, which is unchanged. It grows
from **1 to 3 to 5 to 7 pixels** wide across four equal 30-unit distance bands
(**120–90**, **90–60**, **60–30**, and **30–0** world units above the view),
and dims during recharge.
The circle recharges for 1.25 seconds between changes. Only player contact
activates it, including falling into it; shots pass through without changing modes.
Staying inside does not repeatedly change modes. Touching circles is the normal
mode-switch control.
The current mode appears beside the bottom HUD. Each blast costs one ammo and
retains the same firing cooldown. Damage per pellet is **1** for Normal, **5** for
Longshot, and **1** for Scatter. All guns share a base recoil of **25 units/s** at
speeds up to **25**, decreasing linearly to **15 units/s at 57** and staying at 15
above 57. This uses combined speed before firing, regardless of aim direction.
Mode multipliers apply after this taper: Longshot has **0.75×**
horizontal recoil and **1.1×** vertical recoil. Scatter fires with
**0.85×** recoil; if its pellets kill an enemy, an extra **0.85×** recoil impulse
in the original recoil direction brings that blast's total to **1.70×**. The kill
impulse retains the tapered recoil calculated when that blast was fired.
The bonus triggers once per unboosted blast, even if it kills multiple enemies,
and still applies if you switch modes before the pellets hit. Each Scatter kill
also starts or refreshes a **1-second boost**: subsequent Scatter shots fire with
**1.70×** recoil immediately, without another recoil bonus when they kill. The
boost does not stack, does not affect other modes, and expires after 1 second
without a Scatter kill. There is no hard speed cap: above **57 world units/s**,
deceleration is recalculated every update as **(current speed − 57) / 1**.
Braking grows with excess speed and weakens as you approach 57, with no fixed
deadline or hard clamp. This applies in every direction, including falls.
The left-side **velocity meter** shows your combined speed and marks the 57 threshold.
While you hold a speed above 60, a **heat shield** charges on your character's leading
face: an ember at the nose that spreads into a small cap and heats from red to orange,
pulsing faster as it nears ignition. After **3 continuous seconds above 60** it ignites
white-hot. The shield is a thin glowing cap hugging the body, shedding two short plasma
streamers along the flanks and fading ember particles behind; it turns with your
direction of travel.
Its **2.5-unit radius** destroys nearby regular enemies, awarding **three ammo per kill** (up to six),
and blocks their bullets. Shield kills count toward kills/streaks but do not grant
Scatter recoil boosts. **Shielded brute contact consumes the shield and costs one HP**
(unless invulnerable): horizontal motion bounces away from the brute, with an extra
**80-unit/s horizontal kick**. Vertical momentum is unchanged. The brute survives,
and the shield must charge again. **Bumping into a volatile** detonates it, costs
one HP (unless invulnerable), and throws you upward even if you were falling. With
the shield active, the upward launch is twice as strong. **Brute bullets, spikes and meltdown remain dangerous**. Once active, the shield disappears after **1 continuous second below 60**, resetting
its charge; the cap visibly cools towards ember red as that second runs out. When the shield is lost, to expiry or a brute, it flies apart in shards that are swept back past you. Hitting **60 or higher** during that grace period cancels the countdown. Retry clears the shield and both timers.

The black gun has a glowing, mode-colored muzzle emitter. Normal fire uses neon
cyan energy; Longshot uses deep electric blue; Scatter keeps its warm gold color.
Pellet trails, muzzle flashes, impact sparks, aiming reticle, and mode label use
these colors. Fired pellets and their effects retain their firing mode's color
when you switch modes. Normal and Longshot colors stay fixed across themes.

- **NORMAL:** 10 pellets, standard spread and roughly 29–38 units of range.
- **LONGSHOT:** 2 tightly grouped pellets, roughly 140–149 units of range,
  with a generous 2.5-unit hit radius (versus 0.55 for other player pellets).
- **SCATTER:** 24 pellets, a wider ±0.55-radian spread (about ±31.5°) but only
  roughly 13–18 units of range.

Switching modes does not refill ammo or bypass the firing cooldown. Retry resets
the mode to normal.

**Aim DOWN to fly UP.** Shooting pushes you opposite your aim. Aim diagonally
down to cross gaps, sideways to ricochet off walls, and up to brake an ascent or
dive onto a platform. There are deliberately no movement keys or jump button.

### Tricks of the climb

These enemies add new opportunities to a recoil-only run, without new controls.

- **Volatiles.** Enemies flashing yellow and red drift like drones but explode
  when destroyed by any means. The blast (radius **11**) kills regular enemies,
  deals **6** to brutes, erases hostile shots, and sets off other volatiles a beat
  later, so chains ripple outward. Remote blasts never cost hull: within range they
  throw you up and away, harder the closer you are. **Shoot a volatile while close**
  for an extra upward launch of up to **120 units/s**, fading with distance to zero
  at the blast's 11-unit radius; chained explosions give only the normal blast push.
  Bumping into a volatile detonates it and costs one HP: it gives a **40-unit/s upward
  launch** without the shield, or **80 units/s** with it, on top of any blast push.
  Volatiles appear above 140m.
- **Formations.** From roughly 300m, wings of **5–7 drones** fly in unison every
  200–300m. Above 500m some carry a volatile at their heart: one shot clears the
  wing.
  Feedback: kills, hull loss and blasts hold the frame for an instant; shockwave
  rings, floating labels (`x3`, `+HP`) and centred announcements (`HEAT SHIELD`,
  every 250m) mark events; the arena border flashes red on damage. A dim parallax
  starfield gives a sense of speed and smears into streaks on a fast climb; it is
  drawn in foreground glyphs only, so the terminal background and its transparency
  are untouched. A dashed gold line marks the **height to beat** (your record when
  the run began). `!! MELTDOWN RISING !!` blinks when the meltdown is within 12
  units. On death the wreckage keeps flying, then the results show height, kills,
  best streak and top speed, titled `N E W  R E C O R D` when you beat the line.

Wall bounces retain 78% of sideways speed, with a minimum 12-unit sideways
rebound so slow wall hits push you farther out. Upward velocity is multiplied by
0.75 on contact; falling speed is unaffected.
The arena is 100 world units wide, with a centered starting ledge and enemy
spawns spread across the full width.

- **Four rechargeable shells, up to six with kills.** Each enemy kill instantly
  adds three shells (including heat-shield kills), capped at six. Safe landings add two shells per touchdown,
  capped at four without removing existing bonus ammo; standing on a platform
  does not repeatedly grant ammo.
  Ammo pickups refill to four without removing bonus ammo. Passive reload restores one shell every
  0.75 seconds, only while below four. Firing cancels reload progress and starts
  the timer over. Chain kills to build an ammo reserve.
- **Enemies drop no items.** Every brute kill instantly restores **at least one hull
  point**. Scatter brute kills have a **10% chance to heal two HP** instead of one.
  Healing is capped at three HP and uses the killing pellet's original mode.
  Platform pickups remain available.
- **Cyan ledges** are one-way: fly through from below, land from above.
  Platform friction gradually slows horizontal motion instead of stopping it
  instantly. Airborne momentum is preserved when launching off a ledge.
- **Danger-colored ledges** have spikes. **Drones** drift; hunters pursue and shoot.
  Regular enemies and larger brutes use distinct colors that change with the theme.
  Enemy density increases gradually with altitude, with a slower ramp toward
  late-run pressure. Regular enemies keep their normal size at every altitude;
  difficulty ramps through spawn frequency instead of size.
  Regular enemies have 1 HP: a single pellet from any mode kills them.
  Rare 12-HP brutes only spawn above **1,000m**.
  Shotgun pellets have a small extra collision margin to make near-grazes fairer;
  enemy projectile hitboxes are unchanged.
- **Gold/yellow crosses** refill shells. **Neon pink/magenta crosses** repair hull.
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
  the streak resets to zero. Every kill refreshes the timer. A **BEST** value
  beneath the STREAK label shows your all-time longest streak, retained across
  deaths, retries, and launches. New records save immediately.
- **Unranked debug runs cannot set height or streak records.** Records earned
  earlier in the same run are withdrawn if debug tools are used. A fresh retry
  restores record eligibility; existing records from legitimate runs stay intact.
- The background uses your terminal's default background, including its
  transparency if configured. The game does not change terminal opacity.
- Your best height is saved under `$XDG_STATE_HOME/zapy/best-height`, or
  `$HOME/.local/state/zapy/best-height`; your starting theme is saved alongside it
  in `theme`, and your streak record in `best-streak`. If a Zapy save is absent, the corresponding legacy file under
  `$XDG_STATE_HOME/backblast/` (or `$HOME/.local/state/backblast/`) is read as a
  fallback. This compatibility path is the only retained old branding: existing
  data is preserved, and all new saves go under `zapy/`. Old points-based records
  are left untouched and are not mixed with height records. Quit whenever you like.

Optional **`--seed 42`** makes the first course repeatable to compete on.
Every reset—**R**, or **Enter after death**—generates a fresh random course,
even when launched with `--seed`. Omit the option for a random first course too.
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
world culling, fragmented mouse input, volatile chains, the death sequence,
and the decorative render layer. `nix build .` builds a package and runs the same
checks.

For a real pseudo-terminal integration check, run
**`python3 tests/terminal_smoke.py build/zapy`** inside **`nix develop`**.
It exercises mouse firing, pause/resume, resizing (including wide terminals),
retry, score persistence, clean quit, and SIGINT terminal restoration.
