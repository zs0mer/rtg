#include "game.hpp"
#include "terminal.hpp"
#include <cstdlib>
#include <iostream>

namespace
{
  int checks = 0;
  void check(bool condition, const char *message)
  {
    ++checks;
    if (!condition)
    {
      std::cerr << "FAIL: " << message << '\n';
      std::exit(1);
    }
  }
  bool near(double a, double b) { return std::abs(a - b) < 1e-7; }
  void advance(bb::Game &g, double seconds)
  {
    for (int i = 0; i < static_cast<int>(seconds * 120); ++i)
      g.update(1.0 / 120);
  }
}
int main()
{
  bb::Game g(42);
  for (double offset : {3.5, 4.0})
  {
    bb::Game hitbox(42);
    hitbox.platforms.clear(); hitbox.enemies.clear(); hitbox.pickups.clear();
    hitbox.player.pos = {50, 30};
    hitbox.shotMode = bb::ShotMode::Longshot;
    hitbox.shoot({50, 20});
    hitbox.bullets.resize(1);
    hitbox.bullets.front().pos = {40 + offset, 20};
    hitbox.bullets.front().vel = {};
    hitbox.enemies.push_back({{40, 20}, {}, 3, 1.25, 0, 100, 0});
    hitbox.shotMode = bb::ShotMode::Normal;
    hitbox.update(1.0 / 120);
    check(near(hitbox.enemies.front().hp, offset < 3.75 ? 2 : 3),
          "longshot's wide hitbox persists after switching modes and still has a finite boundary");
  }
  for (bool scatter : {false, true})
  {
    bb::Game recoil(42);
    recoil.platforms.clear(); recoil.enemies.clear(); recoil.pickups.clear();
    recoil.player.pos = {50, 30};
    recoil.shotMode = scatter ? bb::ShotMode::Scatter : bb::ShotMode::Normal;
    recoil.shoot({50, 20});
    // Two separate kills from the same blast must grant only one recoil bonus.
    recoil.bullets.resize(2);
    for (int i = 0; i < 2; ++i)
    {
      bb::Vec pos{40.0 + i * 20, 20};
      recoil.enemies.push_back({pos, {}, 1, 1.25, 0, 100, 0});
      recoil.bullets[static_cast<size_t>(i)].pos = pos;
      recoil.bullets[static_cast<size_t>(i)].vel = {};
    }
    recoil.shotMode = bb::ShotMode::Longshot;
    recoil.update(1.0 / 120);
    check(recoil.kills == 2, "recoil test blast kills two enemies");
        check(near(recoil.player.vel.y, (scatter ? 22.5 : 25) - bb::Game::gravity / 120 + (scatter ? 21.25 : 0)),
          "scatter kill brings total recoil to 1.75x once per blast even after mode switching");
  }
  {
    bb::Game modes(42);
    for (int mode = 0; mode < 3; ++mode)
    {
      modes.player = bb::Player{};
      modes.bullets.clear();
      check(static_cast<int>(modes.shotMode) == mode, "shot mode cycles in expected order");
      check(modes.shoot(modes.player.pos + bb::Vec{10, 0}), "each mode can shoot");
      check(modes.bullets.size() == static_cast<size_t>(mode == 1 ? 2 : mode == 2 ? 24 : 10),
            "shot pattern has the correct pellet count");
      check(modes.player.shells == 3 && near(modes.player.vel.x, mode == 1 ? -27.5 : mode == 2 ? -22.5 : -25),
        "modes use one ammo with 1.1x longshot and 0.9x scatter firing recoil");
      for (const auto &pellet : modes.bullets)
      {
        check(near(pellet.hitRadius, mode == 1 ? 2.5 : 0.55), "only longshot gets a much larger hit radius");
        double range = pellet.vel.length() * pellet.life;
        check(mode == 1 ? range > 139 && range < 150 : mode == 2 ? range > 13 && range < 18 : range > 32 && range < 43,
              "each firing pattern has its intended range");
        if (mode == 1)
          check(std::abs(std::atan2(pellet.vel.y, pellet.vel.x)) <= 0.025, "longshot has a tight spread");
      }
      modes.cycleShotMode();
      check(!modes.shoot(modes.player.pos + bb::Vec{10, 0}), "mode switching cannot bypass cooldown");
    }
    check(modes.shotMode == bb::ShotMode::Normal, "shot modes wrap back to normal");
    modes.cycleShotMode(); modes.reset(42);
    check(modes.shotMode == bb::ShotMode::Normal, "retry resets shot mode");
  }
  check(g.platforms.size() > 10, "endless world generated at startup");
  check(near(bb::Game::width, 100) && near(g.player.pos.x, bb::Game::width / 2),
        "wider arena starts the player at its center");
  check(g.player.shells == 4 && g.player.hp == 3, "fresh player has maximum four shells");
  check(g.shoot(g.player.pos + bb::Vec{0, -10}), "first blast fires");
  check(near(g.player.vel.y, 25), "downward aim produces upward recoil");
  check(g.player.shells == 3 && g.bullets.size() == 10, "blast consumes one shell and emits pellets");
  check(!g.shoot(g.player.pos + bb::Vec{0, -10}), "fire cooldown enforced");
  advance(g, 0.3);
  check(g.player.pos.y > 6, "shot lifts player above starting ledge");
  check(g.altitude > 0 && g.score() > 0, "height contributes score");
  g.altitude = 42.9;
  g.bonus = 9999;
  check(g.score() == 42, "score is maximum height in whole meters, ignoring combat bonuses");
  g.reset(42);
  g.shoot(g.player.pos + bb::Vec{10, 0});
  check(near(g.player.vel.x, -25), "rightward aim recoils left");
  g.reset(42);
  g.shoot(g.player.pos);
  check(std::isfinite(g.player.vel.y), "zero-length aim remains finite");
  g.reset(42);
  g.player.shells = 0;
  check(!g.shoot({40, 0}), "empty gun does not fire");
  g.platforms.clear();
  g.player.pos = {40, 35};
  advance(g, 0.44);
  check(g.player.shells == 0, "passive refill is slower than the old 0.43 seconds");
  advance(g, 0.26);
  check(g.player.shells == 0, "passive refill waits the full 0.75 seconds");
  advance(g, 0.07);
  check(g.player.shells == 1, "airborne shell regeneration prevents softlock");
  check(g.time < 0.8, "passive refill completes after 0.75 seconds");
  g.reset(42);
  g.platforms.clear();
  g.enemies.clear();
  g.pickups.clear();
  g.player.pos = {40, 35};
  g.player.shells = 3;
  advance(g, 0.5);
  double reloadBeforeShot = g.player.reload;
  check(g.shoot(g.player.pos + bb::Vec{0, -10}), "can fire during passive reload");
  check(reloadBeforeShot > 0 && near(g.player.reload, 0), "firing cancels accumulated reload progress");
  advance(g, 0.32);
  check(g.player.shells == 2, "reload does not complete on its old schedule after firing");
  advance(g, 0.45);
  check(g.player.shells == 3, "reload completes 0.75 seconds after the shot");
  g.player.shells = 3;
  g.player.invulnerable = 100;
  g.player.pos = {40, 60};
  g.player.vel = {0, 34};
  advance(g, 2.1);
  check(g.player.shells == 4, "passive refilling never exceeds four shells");
  g.reset(42);
  g.player.pos = {40, 3};
  g.player.vel = {12, -8};
  g.player.shells = 0;
  advance(g, 0.15);
  check(near(g.player.pos.y, 1 + bb::Game::radius), "fall lands on one-way platform");
  check(g.player.shells == 2, "landing restores two shells");
  check(g.player.vel.x > 0 && g.player.vel.x < 12, "landing slows horizontal velocity without instantly stopping");
  double landingSpeed = g.player.vel.x;
  advance(g, 0.2);
  check(g.player.shells == 2, "standing on a platform does not repeat the landing reward");
  check(g.player.vel.x > 0 && g.player.vel.x < landingSpeed * 0.4, "platform friction continues slowing the player over time");
  for (double speed : {-12.0, 12.0})
  {
    g.reset(42);
    g.platforms = {{10, 1, 60, false}};
    g.enemies.clear();
    g.pickups.clear();
    g.player.pos = {40, 1 + bb::Game::radius};
    g.player.vel = {speed, 0};
    g.update(1.0 / 120);
    check(g.player.vel.x * speed > 0 && std::abs(g.player.vel.x) > std::abs(speed) * 0.9,
          "first grounded step preserves momentum in either direction");
    double groundedSpeed = g.player.vel.x;
    g.shoot(g.player.pos + bb::Vec{0, -10});
    g.update(1.0 / 120);
    check(near(g.player.vel.x, groundedSpeed * std::exp(-0.26 / 120)),
          "launching off a platform applies only airborne drag");
  }
  g.reset(42);
  g.platforms = {{25, 5, 30, false}};
  g.player.pos = {40, 2};
  g.player.vel = {0, 30};
  advance(g, 0.2);
  check(g.player.pos.y > 6, "player passes upward through ledges");
  g.reset(42);
  g.player.pos = {1, 10};
  g.player.vel = {-30, 0};
  g.update(1.0 / 120);
  check(g.player.vel.x > 0 && g.player.pos.x >= bb::Game::radius, "walls reflect horizontal momentum");
  for (bool right : {false, true})
    for (double vertical : {-20.0, 20.0})
    {
      g.reset(42);
      g.platforms.clear();
      g.enemies.clear();
      g.pickups.clear();
      g.player.pos = {right ? bb::Game::width - 1 : 1, 30};
      g.player.vel = {right ? 30.0 : -30.0, vertical};
      g.update(1.0 / 120);
      double afterGravity = vertical - bb::Game::gravity / 120;
      check(near(g.player.vel.y, afterGravity > 0 ? afterGravity * 0.85 : afterGravity),
            "both walls reduce rising momentum by fifteen percent without changing falling speed");
      check(right ? g.player.vel.x < 0 : g.player.vel.x > 0, "vertical momentum loss preserves sideways bounce");
    }
  g.reset(42);
  g.platforms.clear();
  g.enemies.clear();
  g.player.pos = {40, 30};
  g.enemies.push_back({{40, 20}, {}, 3, 1.25, 0, 100, 0});
  g.player.shells = 1;
  g.shoot({40, 20});
  advance(g, 0.16);
  check(g.kills == 1 && g.bonus >= 100, "pellets kill enemies and award points");
  check(g.player.shells == 3, "enemy kill instantly restores exactly three shells at range");
  check(g.combo == 1 && g.comboTime > 1.8 && g.comboTime <= 2, "enemy destruction starts the streak and its two-second timer");
  g.comboTime = 0.5;
  g.enemies.clear();
  g.bullets.clear();
  g.enemies.push_back({{40, 20}, {}, 1, 1.25, 0, 100, 0});
  g.bullets.push_back({{40, 20}, {}, 0.3, false});
  g.update(1.0 / 120);
  check(g.combo == 2 && near(g.comboTime, 2), "consecutive destruction increments streak and refreshes timer");
  for (double offset : {1.72, 1.90})
  {
    g.reset(42);
    g.platforms.clear();
    g.enemies.clear();
    g.pickups.clear();
    g.player.pos = {40, 30};
    g.enemies.push_back({{40, 20}, {}, 3, 1.25, 0, 100, 0});
    g.bullets.push_back({{40 + offset, 20}, {}, 0.3, false});
    g.update(1.0 / 120);
    check(near(g.enemies.front().hp, offset < 1.8 ? 2 : 3),
          "slightly wider pellets hit nearby grazes but not distant misses");
  }
  for (int shells : {1, 3, 4, 5, 6})
  {
    g.reset(42);
    g.platforms.clear();
    g.enemies.clear();
    g.pickups.clear();
    g.player.pos = {40, 30};
    g.player.shells = shells;
    g.enemies.push_back({{40, 20}, {}, 1, 1.25, 0, 100, 0});
    g.bullets.push_back({{40, 20}, {}, 0.3, false});
    g.update(1.0 / 120);
    check(g.kills == 1 && g.player.shells == std::min(6, shells + 3), "kill reward adds three beyond recharge limit and caps at six");
    check(g.pickups.empty(), "enemy kills drop no pickups");
  }
  for (int type : {0, 1, 2})
    for (int hp : {1, 2, 3})
    {
      g.reset(42);
      g.platforms.clear();
      g.enemies.clear();
      g.pickups.clear();
      g.player.pos = {40, 30};
      g.player.shells = 0;
      g.player.hp = hp;
      g.enemies.push_back({{40, 20}, {}, 1, 2.3, 0, 100, type});
      g.bullets.push_back({{40, 20}, {}, 0.3, false});
      g.update(1.0 / 120);
      check(g.kills == 1 && g.player.shells == 3, "all enemy types restore three rounds instantly");
      check(g.player.hp == (type == 2 ? std::min(3, hp + 1) : hp), "only brute kills heal one HP capped at three");
      check(g.pickups.empty(), "no enemy type drops items");
    }
  g.reset(42);
  g.player.pos = {40, 30};
  g.platforms.clear();
  g.enemies.clear();
  g.enemies.push_back({g.player.pos, {}, 100, 2, 0, 100, 0});
  g.update(1.0 / 120);
  check(g.player.hp == 2, "enemy contact damages player");
  g.update(1.0 / 120);
  check(g.player.hp == 2, "invulnerability prevents repeated instant damage");
  g.reset(42);
  g.player.pos = {40, 30};
  g.platforms.clear();
  g.enemies.clear();
  g.enemies.push_back({{40, 45}, {}, 8, 2.3, 0, 0, 2});
  g.update(1.0 / 120);
  check(!g.bullets.empty() && g.bullets.front().hostile, "brutes fire hostile plasma");
  check(g.enemies.front().pos.y < 45, "brutes pursue the player");
  g.combo = 4;
  g.comboTime = 0.01;
  advance(g, 0.03);
  check(g.combo == 0 && g.comboTime == 0, "expired combo resets exactly after clamping");
  g.reset(42);
  g.pickups = {{g.player.pos, 1}};
  g.player.hp = 2;
  g.update(1.0 / 120);
  check(g.player.hp == 3, "repair pickup restores hull");
  g.reset(42);
  g.pickups = {{g.player.pos, 0}};
  g.player.shells = 1;
  g.update(1.0 / 120);
  check(g.player.shells == 4 && g.bonus == 25, "shell pickup reloads and adds points");
  for (int shells : {4, 5, 6})
  {
    g.reset(42);
    g.platforms.clear();
    g.enemies.clear();
    g.pickups.clear();
    g.player.pos = {40, 60};
    g.player.vel = {0, 34};
    g.player.shells = shells;
    advance(g, 1.3);
    check(g.player.shells == shells, "passive recharge neither increases nor removes ammo at or above four");
    g.camera = 0;
    g.lava = -22;
    g.player.pos = {40, 1 + bb::Game::radius + 0.01};
    g.player.vel = {0, -2};
    g.platforms = {{10, 1, 60, false}};
    g.update(1.0 / 120);
    check(g.player.shells == std::min(6, shells + 2), "landing adds two shells and caps bonus ammo at six");
    g.pickups = {{g.player.pos, 0}};
    g.update(1.0 / 120);
    check(g.player.shells == std::min(6, shells + 2) && g.bonus == 25, "ammo pickups preserve bonus ammo");
  }
  g.reset(42);
  g.player.pos.y = -24;
  g.update(1.0 / 120);
  check(g.dead && !g.deathReason.empty(), "lava ends the run");
  auto pos = g.player.pos;
  g.update(1.0 / 120);
  check(near(pos.y, g.player.pos.y), "dead simulation freezes");
  bb::Game a(123), b(123);
  check(a.platforms.size() == b.platforms.size() && near(a.platforms[7].x, b.platforms[7].x), "seeded generation is reproducible");
  a.generate(2000);
  check(a.platforms.back().y >= 2000, "world extends indefinitely");
  check(std::all_of(a.enemies.begin(), a.enemies.end(), [](auto e)
                    { return e.type != 2 || near(e.hp, 12); }),
        "generated brutes have increased twelve HP");
  check(std::any_of(a.enemies.begin(), a.enemies.end(), [](auto e)
                    { return e.type == 2; }),
        "later sectors generate larger enemies");
  int earlyEnemies = 0, lateEnemies = 0, lateBrutes = 0;
  for (unsigned seed = 0; seed < 100; ++seed)
  {
    bb::Game world(seed);
    world.generate(2000);
    for (const auto &e : world.enemies)
    {
      check(e.type != 2 || e.pos.y - 3 > 1000, "brutes only spawn above 1000 meters");
      if (e.type != 2)
      {
        check(near(e.radius, 1.25), "regular enemies keep their normal size at every height");
        check(near(e.hp, 1), "all regular enemy types die from one pellet at every height");
      }
      if (e.pos.y >= 30 && e.pos.y < 270)
        ++earlyEnemies;
      if (e.pos.y >= 1500 && e.pos.y < 1740)
      {
        ++lateEnemies;
        if (e.type == 2)
          ++lateBrutes;
      }
    }
  }
  check(earlyEnemies > 650 && earlyEnemies < 1150, "early enemy density starts gently");
  check(lateEnemies > earlyEnemies * 1.6, "enemy density progressively increases with altitude");
  check(lateBrutes > lateEnemies * 0.10 && lateBrutes < lateEnemies * 0.21,
        "brutes remain a rarer minority of later enemies");
  a.player.pos.y = 1000;
  a.update(1.0 / 120);
  check(a.platforms.front().y >= a.camera - 35, "old platforms are culled");
  bb::InputParser parser;
  check(parser.feed("\033[<0;12").empty(), "fragmented mouse sequence buffered");
  auto events = parser.feed(";9M");
  check(events.size() == 1 && events[0].down && events[0].x == 11 && events[0].y == 8, "SGR left click decoded");
  events = parser.feed("\033[<32;15;10M\033[<0;15;10m");
  check(events.size() == 2 && events[0].down && events[1].release, "drag and release decoded");
  events = parser.feed("\033[<35;9;7M\033[<64;9;7M");
  check(events.size() == 2 && !events[0].down && !events[1].down, "motion and wheel do not fire");
  events = parser.feed("\033[Apq");
  check(events.size() == 2 && events[0].key == 'p' && events[1].key == 'q', "unmapped arrow keys ignored");
  std::cout << "PASS: " << checks << " physics, generation, combat, and input checks\n";
}