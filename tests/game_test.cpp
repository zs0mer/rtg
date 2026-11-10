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
  void isolateCombat(bb::Game &g)
  {
    // Generate beyond the test camera before clearing so updates cannot add encounters.
    g.generate(200);
    g.platforms.clear();
    g.enemies.clear();
    g.pickups.clear();
    g.bullets.clear();
    g.particles.clear();
    g.modeTarget = {50, 1000};
    g.player.pos = {50, 30};
    g.player.vel = {};
  }
  void checkSameNextShot(bb::Game a, bb::Game b)
  {
    for (bb::Game *world : {&a, &b})
    {
      world->bullets.clear();
      world->player.shells = bb::Player::maxShells;
      world->player.cooldown = 0;
      check(world->shoot({50, 20}), "seeded RNG probe fires");
    }
    check(a.bullets.size() == b.bullets.size(), "seeded RNG probes have matching pellet counts");
    for (size_t i = 0; i < a.bullets.size(); ++i)
      check(near(a.bullets[i].vel.x, b.bullets[i].vel.x) &&
                near(a.bullets[i].vel.y, b.bullets[i].vel.y),
            "equivalent hits consume the same RNG draws before the next shot");
  }
  void checkDebugStateUnchanged(const bb::Game &g, const bb::Game &before)
  {
    check(near(g.player.pos.x, before.player.pos.x) && near(g.player.pos.y, before.player.pos.y) &&
              near(g.player.vel.x, before.player.vel.x) && near(g.player.vel.y, before.player.vel.y) &&
              g.player.hp == before.player.hp && g.player.shells == before.player.shells &&
              near(g.player.cooldown, before.player.cooldown) && near(g.player.reload, before.player.reload) &&
              near(g.player.invulnerable, before.player.invulnerable),
          "debug switch preserves player position, momentum, health, ammo, cooldown and reload");
    check(near(g.flash, before.flash) && g.flashMode == before.flashMode && near(g.shake, before.shake) &&
              near(g.modeTarget.x, before.modeTarget.x) && near(g.modeTarget.y, before.modeTarget.y) &&
              near(g.modeTargetCooldown, before.modeTargetCooldown),
          "debug switch preserves flash, flash mode, shake, circle position and cooldown");
    check(near(g.camera, before.camera) && near(g.altitude, before.altitude) && near(g.lava, before.lava) &&
              near(g.time, before.time) && near(g.comboTime, before.comboTime) &&
              near(g.scatterBoost, before.scatterBoost) && near(g.overspeedTime, before.overspeedTime) &&
                            near(g.heatShieldBelowTime, before.heatShieldBelowTime) &&
              g.kills == before.kills &&
              g.combo == before.combo && g.bonus == before.bonus && g.dead == before.dead &&
              g.deathReason == before.deathReason,
          "debug switch preserves world physics, scoring and death state");
    check(g.bullets.size() == before.bullets.size(), "debug switch preserves existing bullet count");
    for (size_t i = 0; i < g.bullets.size(); ++i)
    {
      const auto &a = g.bullets[i], &b = before.bullets[i];
      check(near(a.pos.x, b.pos.x) && near(a.pos.y, b.pos.y) && near(a.vel.x, b.vel.x) &&
                near(a.vel.y, b.vel.y) && near(a.life, b.life) && a.hostile == b.hostile &&
                a.killRecoil == b.killRecoil && near(a.hitRadius, b.hitRadius) &&
                near(a.damage, b.damage) && a.shotMode == b.shotMode,
            "debug switch preserves each fired payload, including its shared kill recoil");
    }
    check(g.particles.size() == before.particles.size(), "debug switch neither creates nor removes particles");
    for (size_t i = 0; i < g.particles.size(); ++i)
    {
      const auto &a = g.particles[i], &b = before.particles[i];
      check(near(a.pos.x, b.pos.x) && near(a.pos.y, b.pos.y) && near(a.vel.x, b.vel.x) &&
                near(a.vel.y, b.vel.y) && near(a.life, b.life) && near(a.total, b.total) &&
                a.color == b.color && a.shotMode == b.shotMode,
            "debug switch preserves existing particles and their firing mode");
    }
    check(g.platforms.size() == before.platforms.size() && g.enemies.size() == before.enemies.size() &&
              g.pickups.size() == before.pickups.size(),
          "debug switch preserves world object counts");
    for (size_t i = 0; i < g.platforms.size(); ++i)
    {
      const auto &a = g.platforms[i], &b = before.platforms[i];
      check(near(a.x, b.x) && near(a.y, b.y) && near(a.width, b.width) && a.spikes == b.spikes,
            "debug switch preserves platforms");
    }
    for (size_t i = 0; i < g.enemies.size(); ++i)
    {
      const auto &a = g.enemies[i], &b = before.enemies[i];
      check(near(a.pos.x, b.pos.x) && near(a.pos.y, b.pos.y) && near(a.vel.x, b.vel.x) &&
                near(a.vel.y, b.vel.y) && near(a.hp, b.hp) && near(a.radius, b.radius) &&
                near(a.phase, b.phase) && near(a.cooldown, b.cooldown) && a.type == b.type,
            "debug switch preserves enemies");
    }
    for (size_t i = 0; i < g.pickups.size(); ++i)
      check(near(g.pickups[i].pos.x, before.pickups[i].pos.x) &&
                near(g.pickups[i].pos.y, before.pickups[i].pos.y) && g.pickups[i].type == before.pickups[i].type,
            "debug switch preserves pickups");
  }
  void isolateTarget(bb::Game &g)
  {
    g.generate(1450);
    g.platforms.clear();
    g.enemies.clear();
    g.pickups.clear();
    g.modeTarget = {50, 100};
    g.player.pos = g.modeTarget + bb::Vec{20, 0};
    g.player.vel = {};
  }
}
int main()
{
  bb::Game g(42);
  {
    bb::Game debug(42);
    check(!debug.debugUsed, "new runs have not used debug mode");
    for (int cycle = 0; cycle < 2; ++cycle)
      for (bb::ShotMode next : {bb::ShotMode::Longshot, bb::ShotMode::Scatter, bb::ShotMode::Normal})
      {
        check(debug.debugSwitchMode() && debug.shotMode == next,
              "alive debug switch deterministically cycles Normal, Longshot, Scatter, Normal");
        check(debug.debugUsed, "debug flag stays latched even after cycling back to Normal");
      }
    debug.reset(42);
    check(!debug.debugUsed && debug.shotMode == bb::ShotMode::Normal,
          "seeded retry clears debug use and restores Normal");
  }
  for (int mode = 0; mode < 3; ++mode)
    for (bool dead : {false, true})
      for (bool used : {false, true})
      {
        bb::Game debug(42);
        debug.shotMode = static_cast<bb::ShotMode>(mode);
        check(debug.shoot(debug.player.pos + bb::Vec{3, -4}), "debug preservation test fires a payload");
        debug.bullets.push_back({{20, 30}, {4, -5}, 2, true});
        debug.player.hp = 2;
        debug.player.vel = {7, -9};
        debug.player.reload = 0.43;
        debug.player.invulnerable = 0.8;
        debug.modeTargetCooldown = 0.7;
        debug.camera = 12;
        debug.altitude = 28;
        debug.lava = -10;
        debug.time = 1.5;
        debug.comboTime = 0.6;
        debug.scatterBoost = 1.4;
        debug.kills = 2;
        debug.combo = 2;
        debug.bonus = 300;
        debug.dead = dead;
        debug.debugUsed = used;
        debug.deathReason = dead ? "test death" : "";
        const bb::Game before = debug;
        const auto recoil = debug.bullets.front().killRecoil;
        const bb::Vec recoilImpulse = recoil ? recoil->impulse : bb::Vec{};
        check(debug.debugSwitchMode() == !dead, "debug switch succeeds only while alive");
        check(debug.shotMode == (dead ? before.shotMode : static_cast<bb::ShotMode>((mode + 1) % 3)) &&
                  debug.debugUsed == (used || !dead),
              "dead debug switch preserves mode and flag, alive switch latches the flag");
        checkDebugStateUnchanged(debug, before);
        if (recoil)
          check(near(recoil->impulse.x, recoilImpulse.x) && near(recoil->impulse.y, recoilImpulse.y) && !recoil->used,
                "debug switch does not alter or consume stored scatter kill recoil");
        if (!dead)
        {
          check(!debug.shoot(debug.player.pos + bb::Vec{0, -10}), "debug switch does not bypass firing cooldown");
          debug.player.cooldown = 0;
          debug.player.shells = 0;
          check(debug.debugSwitchMode() && !debug.shoot(debug.player.pos + bb::Vec{0, -10}),
                "empty guns can debug switch without receiving ammo");
          check(near(debug.player.reload, before.player.reload), "empty debug switch preserves reload progress");
        }
      }
  for (unsigned seed = 0; seed < 8; ++seed)
  {
    bb::Game debug(seed), untouched(seed);
    for (int i = 0; i < 6; ++i)
      check(debug.debugSwitchMode(), "RNG probe cycles debug mode back to Normal");
    checkSameNextShot(debug, untouched);
    debug.generate(2000);
    untouched.generate(2000);
    checkDebugStateUnchanged(debug, untouched);
  }
  {
    bb::Game debug(42), untouched(42);
    for (bb::Game *world : {&debug, &untouched})
    {
      isolateTarget(*world);
      world->player.pos = world->modeTarget;
      world->update(0);
    }
    const bb::ShotMode original = debug.shotMode;
    for (int i = 0; i < 3; ++i)
      check(debug.debugSwitchMode(), "debug switch can cycle while inside the circle");
    debug.modeTargetCooldown = untouched.modeTargetCooldown = 0;
    debug.update(0);
    untouched.update(0);
    check(debug.shotMode == original, "debug switching preserves the circle's contact entry latch");
    checkDebugStateUnchanged(debug, untouched);
    for (bb::Game *world : {&debug, &untouched})
    {
      world->player.pos.x += 20;
      world->update(0);
      world->player.pos = world->modeTarget;
      world->update(0);
    }
    check(debug.shotMode == untouched.shotMode && debug.debugUsed && !untouched.debugUsed,
          "debug cycling does not change the next random circle roll or clear its disqualification");
    checkDebugStateUnchanged(debug, untouched);
  }
  {
    bb::Game fresh(42);
    bool varied = false;
    for (int i = 0; i < 3; ++i)
    {
      auto previous = fresh.platforms;
      check(fresh.debugSwitchMode(), "alive run can use debug mode before random retry");
      fresh.player.hp = 1;
      fresh.dead = true;
      fresh.reset();
      varied = varied || fresh.platforms.size() != previous.size() ||
               !near(fresh.platforms[1].x, previous[1].x) ||
               !near(fresh.platforms[1].y, previous[1].y) ||
               !near(fresh.platforms[1].width, previous[1].width);
      check(!fresh.dead && !fresh.debugUsed && fresh.player.hp == 3 && fresh.player.shells == bb::Player::maxShells &&
                fresh.shotMode == bb::ShotMode::Normal && near(fresh.time, 0) && near(fresh.altitude, 0),
            "random reset starts a fresh playable run");
    }
    check(varied, "resets without an explicit seed generate new random courses");
    fresh.reset(42);
    check(fresh.platforms.size() == g.platforms.size() && near(fresh.platforms[1].x, g.platforms[1].x),
          "explicit seeds still reproduce courses for testing and first-run challenges");
  }
  {
    bb::Game recurring(42), matching(42);
    double previousGap = 0;
    bool varied = false, variedX = false;
    check(near(recurring.modeTarget.x, matching.modeTarget.x), "seeded first circle X is reproducible");
    for (int i = 0; i < 12; ++i)
    {
      bb::Vec previous = recurring.modeTarget;
      for (bb::Game *world : {&recurring, &matching})
      {
        world->platforms.clear();
        world->enemies.clear();
        world->pickups.clear();
        world->player.pos = world->modeTarget + bb::Vec{20, 0};
        world->player.vel = {};
        world->update(1.0 / 120);
        check(near(world->modeTarget.x, previous.x) && near(world->modeTarget.y, previous.y),
              "circle stays in place while nearby");
        world->player.pos.y = previous.y + 70;
        world->player.vel = {};
        world->modeTargetCooldown = 1;
        world->update(1.0 / 120);
      }
      double gap = recurring.modeTarget.y - previous.y;
      check(gap >= 1000 && gap <= 1500, "successive circles are spaced 1000-1500 meters apart even when skipped");
      check(near(recurring.modeTarget.x, matching.modeTarget.x) && near(recurring.modeTarget.y, matching.modeTarget.y),
            "seeded circle positions are reproducible");
      check(recurring.modeTarget.x >= 9 && recurring.modeTarget.x <= bb::Game::width - 9,
            "successive circles keep their halo inside the arena");
      check(near(recurring.modeTargetCooldown, 0), "new circle starts ready for contact");
      varied = varied || (i > 0 && !near(gap, previousGap));
      variedX = variedX || !near(recurring.modeTarget.x, previous.x);
      previousGap = gap;
    }
    check(varied, "circle intervals vary rather than using a fixed spacing");
    check(variedX, "successive circles vary their horizontal position");
    recurring.reset(42);
    check(near(recurring.modeTarget.x, g.modeTarget.x) && near(recurring.modeTarget.y, g.modeTarget.y),
          "retry restores the seeded first circle position");
  }
  bool variedFirstHeight = false, variedFirstX = false;
  for (unsigned seed = 0; seed < 60; ++seed)
  {
    bb::Game first(seed);
    double height = first.modeTarget.y - 3;
    check(height >= 750 && height <= 1000, "first circle spawns between displayed heights 750 and 1000m");
    check(first.modeTarget.x >= 9 && first.modeTarget.x <= bb::Game::width - 9,
          "first circle keeps its halo inside the arena");
    variedFirstHeight = variedFirstHeight || !near(first.modeTarget.y, g.modeTarget.y);
    variedFirstX = variedFirstX || !near(first.modeTarget.x, g.modeTarget.x);
    bb::Vec initial = first.modeTarget;
    first.reset(seed);
    check(near(first.modeTarget.x, initial.x) && near(first.modeTarget.y, initial.y),
          "reset reproduces each seed's initial circle position");
  }
  check(variedFirstHeight, "first circle height varies between seeds");
  check(variedFirstX, "first circle X varies between seeds");
  for (int mode = 0; mode < 3; ++mode)
    for (bool hostile : {false, true})
    {
      bb::Game target(42);
      isolateTarget(target);
      target.shotMode = static_cast<bb::ShotMode>(mode);
      // Both a direct hit and a fast crossing must leave the circle untouched.
      target.bullets = {{target.modeTarget, {}, 0.5, hostile},
                        {target.modeTarget - bb::Vec{15, 0}, {3600, 0}, 0.5, hostile}};
      target.update(1.0 / 120);
      check(static_cast<int>(target.shotMode) == mode && near(target.modeTargetCooldown, 0),
            "friendly and hostile shots do not activate the circle in any mode");
      check(target.bullets.size() == 2 && target.particles.empty(),
            "shots pass through the circle without being consumed or causing effects");
      target.bullets = {{target.modeTarget - bb::Vec{15, 0}, {3600, 0}, 0.5, false}};
      target.enemies = {{target.modeTarget + bb::Vec{10, 0}, {}, 1, 1.25, 0, 100, 0}};
      target.update(1.0 / 120);
      check(target.kills == 1 && static_cast<int>(target.shotMode) == mode,
            "pellets can hit enemies through the circle without switching modes");
    }
  for (int mode = 0; mode < 3; ++mode)
  {
    int outcomes[3]{};
    for (unsigned seed = 0; seed < 60; ++seed)
    {
      bb::Game contact(seed);
      isolateTarget(contact);
      contact.shotMode = static_cast<bb::ShotMode>(mode);
      contact.player.pos = contact.modeTarget;
      contact.player.shells = 2;
      contact.player.cooldown = 0.15;
      contact.update(1.0 / 120);
      int next = static_cast<int>(contact.shotMode);
      check(!contact.debugUsed, "ordinary circle contact does not disqualify the run");
      check(next != mode && near(contact.modeTargetCooldown, 1.25),
            "player contact selects an alternate mode and starts the cooldown");
      check(contact.player.shells == 2 && near(contact.player.cooldown, 0.15 - 1.0 / 120),
            "contact does not refill ammo or bypass firing cooldown");
      ++outcomes[next];
    }
    check(outcomes[(mode + 1) % 3] > 10 && outcomes[(mode + 2) % 3] > 10,
          "contact randomly offers both alternate modes");
  }
  for (double offset : {0.0, bb::Game::modeTargetRadius + bb::Game::radius - 0.001,
                        bb::Game::modeTargetRadius + bb::Game::radius + 0.001})
  {
    bb::Game falling(42);
    isolateTarget(falling);
    falling.player.pos = falling.modeTarget + bb::Vec{offset, 15};
    falling.player.vel = {0, -900};
    falling.update(1.0 / 30);
    check(falling.player.pos.y < falling.modeTarget.y - bb::Game::modeTargetRadius - bb::Game::radius,
          "falling sweep ends below the circle with both endpoints outside");
    check((falling.shotMode != bb::ShotMode::Normal) == (offset < bb::Game::modeTargetRadius + bb::Game::radius),
          "swept falling contact catches center and boundary grazes but not near misses");
  }
  {
    bb::Game tangent(42);
    isolateTarget(tangent);
    tangent.player.pos = tangent.modeTarget + bb::Vec{-15, bb::Game::modeTargetRadius + bb::Game::radius};
    tangent.player.vel = {900, bb::Game::gravity / 30};
    tangent.update(1.0 / 30);
    check(tangent.shotMode != bb::ShotMode::Normal, "a swept tangent at the combined radii counts as contact");
  }
  {
    bb::Game contact(42);
    isolateTarget(contact);
    contact.player.pos = contact.modeTarget;
    contact.update(1.0 / 120);
    bb::ShotMode first = contact.shotMode;
    contact.player.pos = contact.modeTarget;
    contact.player.vel = {};
    contact.platforms = {{40, contact.modeTarget.y - bb::Game::radius, 20, false}};
    contact.bullets = {{contact.modeTarget, {}, 0.5, false}};
    contact.update(1.0 / 120);
    check(contact.shotMode == first && contact.bullets.size() == 1,
          "shooting while inside does not switch modes or consume pellets");
    contact.player.pos.x += 20;
    contact.update(0);
    contact.player.pos = contact.modeTarget;
    contact.update(0);
    check(contact.shotMode == first, "reentering during cooldown cannot switch modes");
    advance(contact, 1.4);
    check(near(contact.modeTargetCooldown, 0) && contact.shotMode == first,
          "staying inside never automatically rerolls, including entry during cooldown");
    contact.player.pos.x += 20;
    contact.update(0);
    contact.player.pos = contact.modeTarget;
    contact.update(0);
    check(contact.shotMode != first && near(contact.modeTargetCooldown, 1.25),
          "leaving and reentering allows a fresh contact roll after cooldown");
    bb::ShotMode second = contact.shotMode;
    advance(contact, 1.4);
    contact.bullets = {{contact.modeTarget, {}, 0.5, false}};
    contact.update(0);
    check(contact.shotMode == second && contact.bullets.size() == 1,
          "shooting cannot reroll after cooldown while the player remains inside");
    contact.reset(42);
    check(contact.shotMode == bb::ShotMode::Normal && near(contact.modeTargetCooldown, 0),
          "retry resets both gun mode and circle recharge");
    isolateTarget(contact);
    contact.player.pos = contact.modeTarget;
    contact.update(0);
    check(contact.shotMode != bb::ShotMode::Normal, "reset clears the contact entry latch");
    contact.player.pos = contact.modeTarget + bb::Vec{0, 70};
    contact.update(0);
    check(near(contact.modeTargetCooldown, 0), "replacement clears cooldown after player contact");
    contact.player.pos = contact.modeTarget;
    contact.player.vel = {};
    second = contact.shotMode;
    contact.update(0);
    check(contact.shotMode != second, "a replacement circle has a fresh contact entry latch");
  }
  {
    bb::Game simultaneous(42), contactOnly(42);
    for (bb::Game *world : {&simultaneous, &contactOnly})
    {
      isolateTarget(*world);
      world->player.pos = world->modeTarget;
    }
    simultaneous.bullets = {{simultaneous.modeTarget, {}, 0.5, false},
                            {simultaneous.modeTarget, {}, 0.5, false}};
    simultaneous.update(0);
    contactOnly.update(0);
    check(simultaneous.bullets.size() == 2 && simultaneous.shotMode == contactOnly.shotMode &&
              near(simultaneous.modeTargetCooldown, 1.25) && simultaneous.particles.size() == 50,
          "only player contact triggers the roll and burst when pellets also overlap");
    simultaneous.player.pos.x += 20;
    contactOnly.player.pos.x += 20;
    advance(simultaneous, 1.4);
    advance(contactOnly, 1.4);
    simultaneous.player.pos = simultaneous.modeTarget;
    contactOnly.player.pos = contactOnly.modeTarget;
    simultaneous.update(0);
    contactOnly.update(0);
    check(simultaneous.shotMode == contactOnly.shotMode, "shots do not consume an extra random mode roll");
  }
  for (double offset : {3.5, 4.0})
  {
    bb::Game hitbox(42);
    hitbox.platforms.clear();
    hitbox.enemies.clear();
    hitbox.pickups.clear();
    hitbox.player.pos = {50, 30};
    hitbox.shotMode = bb::ShotMode::Longshot;
    hitbox.shoot({50, 20});
    hitbox.bullets.resize(1);
    hitbox.bullets.front().pos = {40 + offset, 20};
    hitbox.bullets.front().vel = {};
    hitbox.enemies.push_back({{40, 20}, {}, 12, 1.25, 0, 100, 0});
    hitbox.shotMode = bb::ShotMode::Normal;
    hitbox.update(1.0 / 120);
    check(near(hitbox.enemies.front().hp, offset < 3.75 ? 7 : 12),
          "longshot's wide hitbox persists after switching modes and still has a finite boundary");
  }
  for (int mode = 0; mode < 3; ++mode)
    for (int type : {0, 2})
    {
      bb::Game combat(42);
      isolateCombat(combat);
      combat.shotMode = static_cast<bb::ShotMode>(mode);
      combat.shoot({50, 20});
      bb::Bullet pellet = combat.bullets.front();
      pellet.pos = {40, 20};
      pellet.vel = {};
      double hp = type == 2 ? bb::Enemy::bruteHp : 1;
      double damage = mode == 1 ? 5 : 1;
      int hits = static_cast<int>(std::ceil(hp / damage));
      combat.enemies = {{pellet.pos, {}, hp, type == 2 ? 2.3 : 1.25, 0, 100, type}};
      combat.player.shells = 0;
      combat.player.hp = 2;
      bb::Vec initialVelocity = combat.player.vel;
      combat.shotMode = static_cast<bb::ShotMode>((mode + 1) % 3);
      for (int hit = 1; hit <= hits; ++hit)
      {
        combat.bullets = {pellet};
        combat.particles.clear();
        combat.update(0);
        int impacts = 0;
        for (const auto &particle : combat.particles)
          if (particle.color == 0)
          {
            ++impacts;
            check(particle.shotMode == static_cast<bb::ShotMode>(mode),
                  "impact particles retain the pellet's mode after switching, including lethal hits");
          }
        check(impacts == 4, "each hitting pellet creates four shot-colored impact particles");
        check(combat.bullets.empty(), "a hitting pellet is consumed regardless of damage");
        if (hit < hits)
        {
          check(combat.enemies.size() == 1 && near(combat.enemies.front().hp, hp - hit * damage),
                "pellets retain their damage after a mode switch, including fractional health");
          check(combat.kills == 0 && combat.player.shells == 0 && combat.player.hp == 2 &&
                    near(combat.player.vel.y, initialVelocity.y),
                "nonlethal pellets grant no kill rewards or scatter recoil bonus");
        }
        else
        {
          check(combat.enemies.empty() && combat.kills == 1 && combat.player.shells == 3 &&
                    combat.player.hp == (type == 2 ? 3 : 2),
                "lethal mode-specific damage grants kill rewards exactly once with guaranteed brute healing");
          check(near(combat.player.vel.y, initialVelocity.y + (mode == 2 ? 21.25 : 0)),
                "scatter recoil bonus applies on the lethal pellet, not earlier hits");
        }
      }
    }
  for (bool scatter : {false, true})
  {
    bb::Game recoil(42);
    recoil.platforms.clear();
    recoil.enemies.clear();
    recoil.pickups.clear();
    recoil.player.pos = {50, 30};
    recoil.shotMode = scatter ? bb::ShotMode::Scatter : bb::ShotMode::Normal;
    recoil.shoot({50, 20});
    // Two separate kills from the same blast must grant only one recoil bonus.
    recoil.bullets.resize(2);
    for (int i = 0; i < 2; ++i)
    {
      bb::Vec pos{40.0 + i * 20, 20};
      recoil.enemies.push_back({pos, {}, 1, 1.25, 0, 100, 0});
      auto &pellet = recoil.bullets[static_cast<size_t>(i)];
      pellet.pos = pos;
      pellet.vel = {};
    }
    recoil.shotMode = bb::ShotMode::Longshot;
    recoil.update(1.0 / 120);
    check(recoil.kills == 2, "recoil test blast kills two enemies");
    check(near(recoil.player.vel.y, (scatter ? 21.25 : 25) - bb::Game::gravity / 120 + (scatter ? 21.25 : 0)),
          "scatter kill brings total recoil to 1.70x once per blast even after mode switching");
  }
  {
    bb::Game boost(42);
    isolateCombat(boost);
    check(boost.scatterBoost == 0 && near(bb::Game::scatterBoostSeconds, 1),
          "scatter boost starts inactive and lasts one second");
    boost.shotMode = bb::ShotMode::Scatter;
    check(boost.shoot(boost.player.pos + bb::Vec{3, -4}), "unboosted scatter blast fires");
    bb::Bullet first = boost.bullets.front(), second = boost.bullets[1], third = boost.bullets[2];
    check(first.killRecoil == second.killRecoil && !first.killRecoil->used,
          "unboosted pellets share one unused kill recoil");
    first.pos = {40, 20};
    first.vel = {};
    boost.bullets = {first};
    boost.enemies = {{first.pos, {}, 2, 1.25, 0, 100, 0}};
    boost.shotMode = bb::ShotMode::Normal;
    boost.update(0);
    check(boost.scatterBoost == 0 && !first.killRecoil->used &&
              near(boost.player.vel.x, -12.75) && near(boost.player.vel.y, 17),
          "nonlethal Scatter impact neither activates boost nor consumes recoil");
    boost.bullets = {first};
    boost.update(0);
    check(near(boost.scatterBoost, 1) && first.killRecoil->used &&
              near(boost.player.vel.x, -25.5) && near(boost.player.vel.y, 34),
          "Scatter kill activates boost and adds original diagonal impulse after mode switching");
    boost.player.vel = {};
    boost.update(1.0 / 30);
    check(near(boost.scatterBoost, 1 - 1.0 / 30), "boost counts down while another mode is selected");
    second.pos = {60, 20};
    second.vel = {};
    boost.bullets = {second};
    boost.enemies = {{second.pos, {}, 1, 1.25, 0, 100, 0}};
    bb::Vec before = boost.player.vel;
    boost.update(0);
    check(near(boost.scatterBoost, 1) && near(boost.player.vel.x, before.x) &&
              near(boost.player.vel.y, before.y) && boost.kills == 2,
          "second kill from the same blast refreshes to one second without stacking recoil or duration");
    boost.scatterBoost = 0.001;
    third.pos = {40, 20};
    third.vel = {};
    boost.bullets = {third};
    boost.enemies = {{third.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.update(0.002);
    check(near(boost.scatterBoost, 1) && boost.kills == 3,
          "used Scatter blast refreshes boost even when the timer expires before its next kill");
    boost.shotMode = bb::ShotMode::Scatter;
    boost.player.cooldown = 0;
    boost.player.vel = {};
    check(boost.shoot(boost.player.pos + bb::Vec{0, -10}) && near(boost.player.vel.y, 42.5),
          "returning to Scatter after kill activation boosts the next shot upfront");
  }
  for (bool expireBeforeHit : {false, true})
  {
    bb::Game boost(42);
    isolateCombat(boost);
    boost.shotMode = bb::ShotMode::Scatter;
    boost.scatterBoost = 1;
    boost.player.reload = 0.4;
    check(boost.shoot({50, 20}), "boosted Scatter shot fires");
    check(near(boost.player.vel.y, 42.5) && boost.player.shells == 3 &&
              near(boost.player.cooldown, 0.19) && boost.player.reload == 0 && near(boost.scatterBoost, 1),
          "boosted shot gets 1.70x upfront with unchanged ammo, cooldown and reload; firing does not refresh");
    for (const auto &pellet : boost.bullets)
      check(pellet.killRecoil == boost.bullets.front().killRecoil && pellet.killRecoil->used &&
                pellet.shotMode == bb::ShotMode::Scatter && near(pellet.damage, 1) &&
                near(pellet.hitRadius, 0.55) && near(pellet.life, 0.14),
            "boost eligibility is shared at firing without changing Scatter payloads");
    bb::Bullet pellet = boost.bullets.front();
    pellet.pos = {40, 20};
    pellet.vel = {};
    boost.bullets = {pellet};
    boost.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.shotMode = bb::ShotMode::Longshot;
    if (expireBeforeHit)
      boost.scatterBoost = 0;
    boost.update(0);
    check(near(boost.player.vel.y, 42.5) && near(boost.scatterBoost, 1) && boost.kills == 1,
          "boosted blast adds no extra kill recoil even after expiry or mode switch, but refreshes boost");
    boost.bullets = {pellet};
    boost.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.scatterBoost = 0.5;
    boost.update(0);
    check(near(boost.player.vel.y, 42.5) && near(boost.scatterBoost, 1) && boost.kills == 2,
          "every kill from an upfront-boosted blast refreshes without extra recoil");
  }
  {
    bb::Game boost(42);
    isolateCombat(boost);
    boost.shotMode = bb::ShotMode::Scatter;
    boost.shoot({50, 20});
    bb::Bullet older = boost.bullets.front();
    older.pos = {40, 20};
    older.vel = {};
    boost.bullets.clear();
    boost.scatterBoost = 1;
    boost.player.cooldown = 0;
    boost.player.vel = {};
    boost.shoot({50, 20});
    bb::Bullet newer = boost.bullets.front();
    newer.pos = {60, 20};
    newer.vel = {};
    boost.bullets = {newer};
    boost.enemies = {{newer.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.update(0);
    boost.player.vel = {};
    boost.bullets = {older};
    boost.enemies = {{older.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.update(0);
    check(near(boost.player.vel.y, 21.25) && older.killRecoil->used && near(boost.scatterBoost, 1),
          "unboosted blast retains its once-only bonus when a newer blast has refreshed the boost before impact");
    boost.bullets = {older};
    boost.enemies = {{older.pos, {}, 1, 1.25, 0, 100, 0}};
    boost.update(0);
    check(near(boost.player.vel.y, 21.25), "older unboosted blast still receives its kill bonus only once");
  }
  {
    bb::Game timer(42);
    isolateCombat(timer);
    timer.scatterBoost = 1;
    for (int i = 0; i < 11; ++i)
    {
      timer.player.pos = {50, 30};
      timer.player.vel = {};
      timer.update(1);
    }
    check(near(timer.scatterBoost, 1 - 11.0 / 30), "boost countdown uses the existing clamped simulation timestep");
    timer.scatterBoost = 0.02;
    timer.update(-1);
    check(near(timer.scatterBoost, 0.02), "negative timestep cannot extend boost");
    timer.update(0.019);
    check(near(timer.scatterBoost, 0.001), "boost remains active just before expiry");
    timer.shotMode = bb::ShotMode::Scatter;
    timer.player.vel = {};
    check(timer.shoot({50, 20}) && near(timer.player.vel.y, 42.5), "shot just before expiry gets boosted recoil");
    timer.bullets.clear();
    timer.scatterBoost = 0.001;
    timer.update(0.001);
    check(timer.scatterBoost == 0, "boost expires exactly at its remaining duration");
    timer.player.vel = {};
    timer.player.cooldown = 0;
    check(timer.shoot(timer.player.pos + bb::Vec{0, -10}) && near(timer.player.vel.y, 21.25),
          "shot at expiry returns to base Scatter recoil");
    timer.bullets.clear();
    timer.update(1.0 / 30);
    check(timer.scatterBoost == 0, "expired boost stays clamped at zero");
    timer.scatterBoost = 1.7;
    timer.dead = true;
    timer.update(1);
    check(near(timer.scatterBoost, 1.7), "dead updates freeze boost");
    check(!timer.shoot({50, 20}) && near(timer.scatterBoost, 1.7), "dead firing cannot consume or refresh boost");
    timer.reset(42);
    check(timer.scatterBoost == 0 && !timer.dead, "reset clears active boost");
  }
  for (bb::ShotMode mode : {bb::ShotMode::Normal, bb::ShotMode::Longshot})
  {
    bb::Game other(42);
    isolateCombat(other);
    other.shotMode = mode;
    other.scatterBoost = 1;
    other.shoot({50, 20});
    bb::Bullet pellet = other.bullets.front();
    pellet.pos = {40, 20};
    pellet.vel = {};
    other.bullets = {pellet};
    other.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
    other.shotMode = bb::ShotMode::Scatter;
    other.update(0);
    check(near(other.scatterBoost, 1) && near(other.player.vel.y, mode == bb::ShotMode::Longshot ? 27.5 : 25),
          "Normal and Longshot kills retain original recoil and do not refresh boost even in current Scatter mode");
    other.scatterBoost = 0;
    other.bullets = {pellet};
    other.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
    other.update(0);
    check(other.scatterBoost == 0, "non-Scatter kills cannot activate boost");
  }
  {
    bb::Game modes(42);
    for (int mode = 0; mode < 3; ++mode)
    {
      modes.player = bb::Player{};
      modes.bullets.clear();
      modes.particles.clear();
      modes.shotMode = static_cast<bb::ShotMode>(mode);
      check(modes.shoot(modes.player.pos + bb::Vec{10, 0}), "each mode can shoot");
      check(modes.flash > 0 && modes.flashMode == modes.shotMode, "successful shots remember their flash mode");
      check(modes.particles.size() == 10, "each shot emits ten muzzle particles");
      for (const auto &particle : modes.particles)
        check(particle.color == 0 && particle.shotMode == modes.shotMode,
              "muzzle particles are shot-colored and tagged with the firing mode");
      check(modes.bullets.size() == static_cast<size_t>(mode == 1 ? 2 : mode == 2 ? 24
                                                                                  : 10),
            "shot pattern has the correct pellet count");
      check(modes.player.shells == 3 && near(modes.player.vel.x, mode == 1 ? -18.75 : mode == 2 ? -21.25
                                                                                                : -25),
            "modes use one ammo with 0.75x longshot and 0.85x scatter horizontal recoil");
      for (const auto &pellet : modes.bullets)
      {
        check(pellet.shotMode == modes.shotMode, "every fired pellet is tagged with the firing mode");
        check(near(pellet.hitRadius, mode == 1 ? 2.5 : 0.55), "only longshot gets a much larger hit radius");
        check(near(pellet.damage, mode == 1 ? 5 : 1),
              "each mode assigns its damage to every fired pellet");
        check(near(pellet.life, mode == 1 ? 0.9 : mode == 2 ? 0.14 : 0.30),
              "normal pellets last 0.30 seconds while longshot and scatter lifetimes are unchanged");
        double speed = pellet.vel.length();
        check(mode == 1 ? speed >= 155 && speed <= 165 : speed >= 95 && speed <= 125,
              "shorter normal range does not change any mode's pellet speed");
        double range = speed * pellet.life;
        check(mode == 1 ? range > 139 && range < 150 : mode == 2 ? range > 13 && range < 18
                                                                 : range >= 28.5 && range <= 37.5,
              "each firing pattern has its intended range");
        if (mode == 1)
          check(std::abs(std::atan2(pellet.vel.y, pellet.vel.x)) <= 0.025, "longshot has a tight spread");
      }
      modes.shotMode = static_cast<bb::ShotMode>((mode + 1) % 3);
      check(modes.flashMode == static_cast<bb::ShotMode>(mode), "switching modes preserves the last flash mode");
      check(!modes.shoot(modes.player.pos + bb::Vec{10, 0}), "mode switching cannot bypass cooldown");
      check(modes.flashMode == static_cast<bb::ShotMode>(mode), "cooldown-rejected shots preserve the last flash mode");
      modes.player.cooldown = 0;
      modes.player.shells = 0;
      check(!modes.shoot(modes.player.pos + bb::Vec{10, 0}), "empty switched gun cannot shoot");
      check(modes.flashMode == static_cast<bb::ShotMode>(mode), "empty-gun shots preserve the last flash mode");
      for (const auto &pellet : modes.bullets)
        check(pellet.shotMode == static_cast<bb::ShotMode>(mode), "switching modes preserves existing pellet tags");
      for (const auto &particle : modes.particles)
        check(particle.shotMode == static_cast<bb::ShotMode>(mode), "switching modes preserves existing muzzle particle tags");
    }
    modes.shotMode = bb::ShotMode::Scatter;
    modes.reset(42);
    check(modes.shotMode == bb::ShotMode::Normal, "retry resets shot mode");
    check(modes.flashMode == bb::ShotMode::Normal && near(modes.flash, 0), "retry clears the flash and resets its remembered mode");
    bb::Bullet legacyBullet{{}, {}, 1, false};
    bb::Particle legacyParticle{{}, {}, 1, 1, 0};
    check(legacyBullet.shotMode == bb::ShotMode::Normal && legacyParticle.shotMode == bb::ShotMode::Normal,
          "omitted aggregate mode tags default to normal for bullets and particles");
  }
  {
    bool widerLeft = false, widerRight = false;
    for (unsigned seed = 0; seed < 64; ++seed)
    {
      bb::Game spread(seed);
      spread.shotMode = bb::ShotMode::Scatter;
      spread.shoot(spread.player.pos + bb::Vec{10, 0});
      for (const auto &pellet : spread.bullets)
      {
        double angle = std::atan2(pellet.vel.y, pellet.vel.x);
        check(std::abs(angle) <= 0.55, "scatter stays inside its wider symmetric spread");
        widerLeft = widerLeft || angle < -0.48;
        widerRight = widerRight || angle > 0.48;
      }
    }
    check(widerLeft && widerRight, "scatter now reaches beyond its old spread on both sides");
  }
  for (int mode = 0; mode < 3; ++mode)
    for (bool boosted : {false, true})
    for (bb::Vec aim : {bb::Vec{1, 0}, bb::Vec{0, -1}, bb::Vec{3, -4}, bb::Vec{-3, 4}})
      for (bb::Vec momentum : {bb::Vec{}, bb::Vec{7, -9}, bb::Vec{-50, 45}})
      {
        bb::Game recoil(42);
        recoil.shotMode = static_cast<bb::ShotMode>(mode);
        recoil.scatterBoost = boosted ? bb::Game::scatterBoostSeconds : 0;
        recoil.player.vel = momentum;
        bb::Vec unit = aim.unit();
        double baseRecoil = 25 - 10 * std::clamp((momentum.length() - 25) / 32, 0.0, 1.0);
        double horizontal = baseRecoil * (mode == 1 ? 0.75 : mode == 2 ? (boosted ? 1.70 : 0.85) : 1);
        double vertical = baseRecoil * (mode == 1 ? 1.1 : mode == 2 ? (boosted ? 1.70 : 0.85) : 1);
        bb::Vec expected = momentum + bb::Vec{-unit.x * horizontal, -unit.y * vertical};

        check(recoil.shoot(recoil.player.pos + aim), "axis and momentum recoil test fires");
        check(near(recoil.player.vel.x, expected.x) && near(recoil.player.vel.y, expected.y),
              "all firing modes add full axis-scaled recoil without a hard speed cap");
      }
  for (auto [speed, impulse] : {std::pair{0.0, 25.0}, {24.0, 25.0}, {25.0, 25.0},
                               {33.0, 22.5}, {41.0, 20.0}, {49.0, 17.5}, {57.0, 15.0}, {100.0, 15.0}})
    for (bb::Vec direction : {bb::Vec{1, 0}, bb::Vec{0, -1}, bb::Vec{0.6, 0.8}})
      for (bb::Vec aim : {bb::Vec{1, 0}, bb::Vec{0, -1}, bb::Vec{-0.6, -0.8}})
        for (bb::ShotMode mode : {bb::ShotMode::Normal, bb::ShotMode::Longshot, bb::ShotMode::Scatter})
          for (bool boosted : {false, true})
          {
            bb::Game taper(42);
            isolateCombat(taper);
            taper.shotMode = mode;
            taper.scatterBoost = boosted ? 1 : 0;
            bb::Vec before = direction * speed;
            taper.player.vel = before;
            check(taper.shoot(taper.player.pos + aim), "all-mode recoil taper test fires");
            double horizontal = impulse * (mode == bb::ShotMode::Longshot ? 0.75 :
                                           mode == bb::ShotMode::Scatter ? (boosted ? 1.70 : 0.85) : 1);
            double vertical = impulse * (mode == bb::ShotMode::Longshot ? 1.1 :
                                         mode == bb::ShotMode::Scatter ? (boosted ? 1.70 : 0.85) : 1);
            bb::Vec expected = before - bb::Vec{aim.x * horizontal, aim.y * vertical};
            check(near(taper.player.vel.x, expected.x) && near(taper.player.vel.y, expected.y),
                  "all guns taper base recoil from 25 to 15 before applying mode and boost multipliers");
            if (mode == bb::ShotMode::Scatter)
            {
              auto shared = taper.bullets.front().killRecoil;
              check(near(shared->impulse.x, -aim.x * impulse * 0.85) &&
                        near(shared->impulse.y, -aim.y * impulse * 0.85) && shared->used == boosted,
                    "Scatter kill bonus stores the original tapered recoil and respects upfront boosts");
            }
          }
  for (double dt : {0.0, 1.0 / 120, 1.0 / 30})
    for (double speed : {0.0, 50.0, 57.0, 57.001, 80.0, 150.0})
      for (bb::Vec direction : {bb::Vec{1, 0}, bb::Vec{-1, 0}, bb::Vec{0, 1}, bb::Vec{0, -1}, bb::Vec{0.6, 0.8}})
      {
        bb::Game drag(42);
        isolateCombat(drag);
        bb::Vec expected = direction * speed;
        drag.player.vel = {expected.x * std::exp(0.26 * dt), expected.y + bb::Game::gravity * dt};
        drag.update(dt);
        double slowed = speed > 57 ? 57 + (speed - 57) * (1 - dt) : speed;
        expected = direction * slowed;
        check(near(drag.player.vel.x, expected.x) && near(drag.player.vel.y, expected.y),
              "overspeed drag decays only excess speed in every direction and preserves direction");
        if (speed > 57)
          check(drag.player.vel.length() > 57 && (dt == 0 || drag.player.vel.length() < speed),
                "overspeed slows gradually without clipping, and zero timestep preserves momentum");
      }
  for (int steps : {150, 600})
    for (bool extraRecoil : {false, true})
    {
      bb::Game decay(42);
      isolateCombat(decay);
      double dt = 5.0 / steps;
      decay.player.vel = {0, 100};
      for (int i = 0; i < steps; ++i)
      {
        if (extraRecoil && i == steps / 2)
        {
          decay.player.cooldown = 0;
          decay.shoot(decay.player.pos + bb::Vec{0, -10});
        }
        double before = decay.player.vel.y;
        // Cancel gravity and keep the player away from world interactions to isolate decay.
        decay.player.pos = {50, 30};
        decay.player.vel.y += bb::Game::gravity * dt;
        decay.bullets.clear();
        decay.update(dt);
        double expected = 57 + 43 * std::pow(1 - dt, i + 1);
        if (extraRecoil && i >= steps / 2)
          expected += 15 * std::pow(1 - dt, i + 1 - steps / 2);
        check(near(decay.player.vel.y, expected),
              "excess velocity decays proportionally at different timesteps, including additional recoil");
        check(near((before - decay.player.vel.y) / dt, (before - 57)),
              "deceleration is recalculated from current excess speed every update");
      }
      check(decay.player.vel.length() > 57,
            "proportional braking approaches 57 without imposing a fixed deadline");
      decay.player.vel = {0, 80};
      decay.update(0);
      check(near(decay.player.vel.y, 80), "zero timestep preserves excess velocity");
      decay.dead = true;
      decay.update(dt);
      check(near(decay.player.vel.y, 80), "overspeed braking freezes when dead");
      decay.dead = false;
      decay.player.vel = {};
      decay.update(0);
      check(decay.player.vel.length() == 0, "no overspeed braking below threshold");
      decay.player.vel = {0, 80};
      decay.update(0);
      decay.reset(42);
      check(decay.player.vel.length() == 0, "retry clears excess velocity");
    }
  {
    bb::Game shield(42);
    isolateCombat(shield);
    constexpr double dt = 1.0 / 120;
    auto charge = [&]
    {
      shield.player.pos = {50, 30};
      shield.player.vel = {0, 65};
      shield.update(dt);
    };
    for (int i = 0; i < 359; ++i)
      charge();
    check(!shield.heatShieldActive(), "heat shield needs three continuous seconds above 60");
    charge();
    check(shield.heatShieldActive() && near(shield.overspeedTime, 3),
          "heat shield activates at exactly three seconds");
    shield.player.vel = {60, 0};
    shield.update(0);
    check(shield.heatShieldActive() && shield.heatShieldBelowTime == 0,
          "reaching the speed threshold does not instantly clear an active shield");
    auto slowStep = [&]
    {
      shield.player.pos = {50, 30};
      shield.player.vel = {};
      shield.update(dt);
    };
    for (int i = 0; i < 119; ++i)
      slowStep();
    check(shield.heatShieldActive() && near(shield.heatShieldBelowTime, 119.0 / 120),
          "shield persists through slowdown until just before one second");
    shield.player.vel = {60, 0};
    shield.update(0);
    check(shield.heatShieldActive() && shield.heatShieldBelowTime == 0,
          "hitting exactly 60 cancels expiration without needing to recharge");
    charge();
    check(shield.heatShieldActive() && shield.heatShieldBelowTime == 0,
          "returning above 60 also cancels expiration without needing to recharge");
    for (int i = 0; i < 120; ++i)
      slowStep();
    check(!shield.heatShieldActive() && shield.overspeedTime == 0 && near(shield.heatShieldBelowTime, 1),
          "one continuous second below threshold expires shield and resets charge");
    shield.player.vel = {60, 0};
    shield.update(0);
    check(shield.overspeedTime == 0 && !shield.heatShieldActive(),
          "exactly 60 maintains an active shield but cannot charge an inactive one");
    shield.player.vel = {0, 59};
    shield.update(dt);
    check(shield.overspeedTime == 0 && !shield.heatShieldActive(),
          "speed above the 57 braking threshold but below 60 cannot charge heat shield");
    for (int i = 0; i < 180; ++i)
      charge();
    shield.player.vel = {};
    shield.update(0);
    for (int i = 0; i < 180; ++i)
      charge();
    check(!shield.heatShieldActive() && near(shield.overspeedTime, 1.5),
          "separate overspeed intervals do not accumulate shield charge");
    double before = shield.overspeedTime;
    shield.update(-1);
    check(near(shield.overspeedTime, before), "nonpositive timestep cannot charge heat shield");
    shield.dead = true;
    shield.update(dt);
    check(!shield.heatShieldActive() && near(shield.overspeedTime, before),
          "death hides heat shield and freezes charge");
    shield.reset(42);
    check(shield.overspeedTime == 0 && shield.heatShieldBelowTime == 0 && !shield.heatShieldActive(),
              "retry clears heat shield and both timers");
  }
  {
    bb::Game fire(42);
    isolateCombat(fire);
    fire.overspeedTime = 3;
    fire.player.vel = {0, 100};
    fire.time = 0.039;
    constexpr double dt = 1.0 / 120;
    fire.update(dt);
    check(fire.particles.size() == 1 && fire.particles.front().color == 4 &&
              (fire.particles.front().pos - fire.player.pos).length() < 3,
          "active shield emits a yellow ember close to the player body");
    bb::Particle ember = fire.particles.front();
    fire.update(dt);
    check(fire.particles.size() == 1 && near(fire.particles.front().pos.y, ember.pos.y + ember.vel.y * dt) &&
              near(fire.particles.front().vel.y, ember.vel.y - 15 * dt) &&
              near(fire.particles.front().life, ember.life - dt),
          "embers move independently, drip under gravity, and fade over time");
    fire.time = 0.079;
    fire.update(dt);
    check(fire.particles.size() == 2 && fire.particles.back().color == 5,
          "shield alternates yellow and red embers");
    fire.player.vel = {};
    fire.time = 0.119;
    fire.update(dt);
    check(fire.heatShieldActive() && fire.particles.size() == 3,
          "shield continues emitting during slowdown grace period");
    fire.time = 0.159;
    fire.heatShieldBelowTime = 0.999;
    fire.update(dt);
    check(!fire.heatShieldActive() && fire.particles.size() == 3,
          "expired shield stops emitting while existing embers linger");
    advance(fire, 0.7);
    check(fire.particles.empty(), "shield embers expire instead of accumulating");
    fire.reset(42);
    check(fire.particles.empty(), "retry clears shield embers");
  }
  {
    bb::Game grace(42);
    isolateCombat(grace);
    grace.overspeedTime = 3;
    grace.heatShieldBelowTime = 0.5;
    grace.player.vel = {};
    grace.enemies = {{grace.player.pos, {}, 1, 1.25, 0, 100, 0}};
    grace.update(0);
    check(grace.heatShieldActive() && grace.enemies.empty() && grace.player.hp == 3 &&
              grace.kills == 1 && grace.player.shells == 6 && near(grace.heatShieldBelowTime, 0.5),
          "slowdown grace period retains shield kills, ammo rewards and contact immunity");
    bb::Bullet bullet{grace.player.pos, {}, 1, true};
    grace.bullets = {bullet};
    grace.update(0);
    check(grace.player.hp == 3 && grace.bullets.empty(), "grace shield still blocks regular bullets");
    bullet.bruteShot = true;
    grace.bullets = {bullet};
    grace.update(0);
    check(grace.player.hp == 2, "brute bullets remain dangerous during slowdown grace period");
  }
  for (bb::Vec normal : {bb::Vec{1, 0}, bb::Vec{-1, 0}, bb::Vec{0, 1}, bb::Vec{0, -1}, bb::Vec{0.6, 0.8}})
    for (double outwardSpeed : {-100.0, 0.0, 30.0})
      for (double invulnerable : {0.0, 0.7})
      {
        bb::Game bounce(42);
        isolateCombat(bounce);
        bb::Vec tangent{-normal.y, normal.x};
        bounce.player.pos = bb::Vec{50, 30} + normal * 2;
        bounce.player.vel = normal * outwardSpeed + tangent * 25;
        bb::Vec incoming = bounce.player.vel;
        bounce.player.invulnerable = invulnerable;
        bounce.overspeedTime = bb::Game::heatShieldChargeSeconds;
        bounce.heatShieldBelowTime = 0.5;
        bounce.enemies = {{{50, 30}, {}, bb::Enemy::bruteHp, 2.3, 0, 100, 2}};
        bounce.update(0);
        double side = std::abs(normal.x) > 0.001 ? (normal.x > 0 ? 1 : -1) : (incoming.x > 0 ? -1 : 1);
        bb::Vec expected{side * (std::abs(incoming.x) + 80), incoming.y};
        check(near(bounce.player.vel.x, expected.x) && near(bounce.player.vel.y, expected.y),
              "shield brute bounce adds an outward horizontal kick without changing vertical momentum");
        check(near(bounce.player.pos.y, 30 + normal.y * 2),
              "separating from brute does not teleport the player vertically");
        check(!bounce.heatShieldActive() && bounce.overspeedTime == 0 && bounce.heatShieldBelowTime == 0 &&
                  bounce.player.hp == 3 && near(bounce.player.invulnerable, invulnerable) &&
                  bounce.enemies.size() == 1 && near(bounce.enemies.front().hp, bb::Enemy::bruteHp) &&
                  bounce.kills == 0 && bounce.player.shells == 4 && bounce.combo == 0,
              "brute survives the shield sacrifice; bounce deals no hull damage or kill rewards even while invulnerable");
        check((bounce.player.pos - bounce.enemies.front().pos).length() > 2.3 + bb::Game::radius,
              "bounce separates player from brute to avoid repeated overlap");
        bb::Vec after = bounce.player.vel;
        bounce.update(0);
        check(near(bounce.player.vel.x, after.x) && near(bounce.player.vel.y, after.y) && bounce.player.hp == 3,
              "shield cannot bounce or damage player again immediately after being consumed");
      }
  {
    bb::Game sweepBounce(42);
    isolateCombat(sweepBounce);
    sweepBounce.player.vel = {0, 300};
    sweepBounce.overspeedTime = bb::Game::heatShieldChargeSeconds;
    sweepBounce.enemies = {{{50, 35}, {}, bb::Enemy::bruteHp, 2.3, 0, 100, 2}};
    sweepBounce.update(1.0 / 30);
    double verticalBeforeBounce = 300 - bb::Game::gravity / 30;
    verticalBeforeBounce -= (verticalBeforeBounce - bb::Game::softSpeedThreshold) / 30;
    check(near(sweepBounce.player.vel.y, verticalBeforeBounce) && near(sweepBounce.player.vel.x, 80) &&
              !sweepBounce.heatShieldActive() && sweepBounce.overspeedTime == 0 &&
              sweepBounce.player.pos.y > sweepBounce.enemies.front().pos.y && sweepBounce.player.hp == 3,
          "swept brute crossing launches sideways while preserving rising velocity after normal gravity and drag");
    bb::Game stationary(42);
    isolateCombat(stationary);
    stationary.overspeedTime = bb::Game::heatShieldChargeSeconds;
    stationary.heatShieldBelowTime = 0.5;
    stationary.enemies = {{stationary.player.pos, {}, bb::Enemy::bruteHp, 2.3, 0, 100, 2}};
    stationary.update(0);
    check(near(stationary.player.vel.x, 80) && near(stationary.player.vel.y, 0) &&
              !stationary.heatShieldActive() && stationary.player.hp == 3,
          "coincident stationary contact has a finite horizontal bounce fallback");
    stationary.enemies.clear();
    bb::Bullet bruteBullet{stationary.player.pos, {}, 1, true};
    bruteBullet.bruteShot = true;
    stationary.bullets = {bruteBullet};
    stationary.update(0);
    check(stationary.player.hp == 2, "shield bounce grants no blanket immunity to brute bullets");
  }
  for (bool charged : {false, true})
    for (int type : {0, 1, 2})
    {
      bb::Game contact(42);
      isolateCombat(contact);
      contact.player.vel = {0, 100};
      contact.overspeedTime = charged ? 3 : 0;
      contact.enemies = {{contact.player.pos, {}, 12, 2.3, 0, 100, type}};
      contact.update(0);
      bool blocked = charged && type != 2;
      check(contact.player.hp == (charged ? 3 : 2),
            "active shield protects brute contact by breaking; uncharged players take contact damage");
      if (blocked)
        check(contact.player.invulnerable == 0 && near(contact.player.vel.y, 100) &&
                  contact.enemies.empty() && contact.kills == 1 && contact.player.shells == 6,
              "shield contact destroys regular enemies and adds three ammo without damage or knockback");
    }
  for (bool charged : {false, true})
    for (int type : {0, 1, 2})
      for (double offset : {2.2, 3.74, 3.76})
      {
        bb::Game reach(42);
        isolateCombat(reach);
        reach.overspeedTime = charged ? 3 : 0;
        reach.player.vel = {0, 100};
        reach.enemies = {{reach.player.pos + bb::Vec{offset, 0}, {}, 12, 1.25, 0, 100, type}};
        reach.update(0);
        bool burned = charged && type != 2 && offset < 3.75;
        check(reach.enemies.empty() == burned && reach.kills == (burned ? 1 : 0) &&
                  reach.player.shells == (burned ? 6 : 4) && reach.player.hp == 3,
              "larger shield radius burns regular enemies beyond body contact but preserves distant enemies and brutes");
      }
  for (bool brute : {false, true})
    for (double offset : {1.3, 2.89, 2.91})
    {
      bb::Game intercept(42);
      isolateCombat(intercept);
      intercept.overspeedTime = 3;
      intercept.player.vel = {0, 100};
      bb::Bullet bullet{intercept.player.pos + bb::Vec{offset, 0}, {}, 1, true};
      bullet.bruteShot = brute;
      intercept.bullets = {bullet};
      intercept.update(0);
      check(intercept.bullets.empty() == (!brute && offset < 2.9) && intercept.player.hp == 3,
            "larger shield radius intercepts regular bullets without enlarging brute bullet damage hitbox");
    }
  for (int ammo : {0, 5, 6})
    for (bb::ShotMode mode : {bb::ShotMode::Normal, bb::ShotMode::Scatter})
    {
      bb::Game burn(42);
      isolateCombat(burn);
      burn.overspeedTime = 3;
      burn.shotMode = mode;
      burn.player.vel = {0, 110};
      burn.player.shells = ammo;
      burn.player.reload = 0.3;
      burn.enemies = {{burn.player.pos, {}, 12, 1.25, 0, 0, 1}};
      burn.bullets = {{burn.player.pos, {}, 1, false}};
      burn.update(0);
      check(burn.enemies.empty() && burn.kills == 1 && burn.combo == 1 && burn.bonus == 100 &&
                burn.player.shells == std::min(6, ammo + 3) && burn.player.reload == 0 &&
                burn.bullets.size() == 1 && burn.player.hp == 3 && burn.scatterBoost == 0,
            "shield kill grants exactly three ammo capped at six, normal streak credit, no Scatter boost or double bullet reward");
      burn.update(0);
      check(burn.kills == 1 && burn.player.shells == std::min(6, ammo + 3),
            "destroyed enemy cannot grant another shield reward on later frames");
    }
  {
    bb::Game sweep(42);
    isolateCombat(sweep);
    sweep.overspeedTime = 3;
    sweep.player.vel = {0, 120};
    sweep.enemies = {{{50, 31.9}, {}, 12, 0.1, 0, 100, 0}};
    sweep.update(1.0 / 30);
    check(sweep.enemies.empty() && sweep.kills == 1 && sweep.player.shells == 6,
          "fast heat shield contact destroys enemies crossed between simulation endpoints");
  }
  for (bool charged : {false, true})
    for (int type : {1, 2})
    {
      bb::Game plasma(42);
      isolateCombat(plasma);
      plasma.player.vel = {0, 100};
      plasma.overspeedTime = charged ? 3 : 0;
      plasma.enemies = {{{40, 30}, {}, 12, 2.3, 0, 0, type}};
      plasma.update(0);
      check(plasma.bullets.size() == 1 && plasma.bullets.front().bruteShot == (type == 2),
            "hostile projectiles remember whether their shooter was a brute");
      bb::Bullet projectile = plasma.bullets.front();
      plasma.enemies.clear();
      projectile.pos = plasma.player.pos;
      projectile.vel = {};
      plasma.bullets = {projectile};
      plasma.update(0);
      check(plasma.player.hp == (charged && type != 2 ? 3 : 2) && plasma.bullets.empty(),
            "heat shield blocks regular bullets but not brute bullets even after shooter removal");
    }
  {
    bb::Game spikes(42);
    isolateCombat(spikes);
    spikes.overspeedTime = 3;
    spikes.player.pos = {50, 10 + bb::Game::radius};
    spikes.player.vel = {100, -1};
    spikes.platforms = {{40, 10, 20, true}};
    spikes.update(0);
    check(spikes.player.hp == 2, "heat shield does not protect against spikes");
    bb::Game lava(42);
    isolateCombat(lava);
    lava.overspeedTime = 3;
    lava.player.vel = {0, 100};
    lava.lava = 31;
    lava.update(0);
    check(lava.dead, "heat shield does not protect against meltdown");
  }
  {
    bb::Game kill(42);
    isolateCombat(kill);
    kill.shotMode = bb::ShotMode::Scatter;
    kill.player.vel = {0, 40};
    kill.shoot(kill.player.pos + bb::Vec{0, -10});
    check(near(kill.player.vel.y, 57.265625), "tapered Scatter firing can exceed the old cap");
    bb::Bullet pellet = kill.bullets.front();
    pellet.pos = {40, 20};
    pellet.vel = {};
    kill.bullets = {pellet};
    kill.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
    kill.update(0);
    check(near(kill.player.vel.y, 74.53125), "Scatter kill applies its original tapered impulse above the soft threshold");
    kill.bullets.clear();
    kill.update(1.0 / 120);
    check(kill.player.vel.y > 57 && kill.player.vel.y < 74.53125,
          "Scatter kill overspeed decelerates on subsequent simulation steps");
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
    for (double dt : {0.0, 1.0 / 120, 1.0 / 30})
      for (double speed : {0.0, 1e-10, 4.0, 12.0, 12.0 / 0.78 - 1e-4,
                           12.0 / 0.78, 12.0 / 0.78 + 1e-4, 30.0, 57.0})
        for (double vertical : {-20.0, 0.0, bb::Game::gravity * dt - 1e-4,
                                bb::Game::gravity * dt, bb::Game::gravity * dt + 1e-4, 20.0})
        {
          bb::Game wall(42);
          isolateCombat(wall);
          double inward = right ? -1.0 : 1.0;
          double boundary = right ? bb::Game::width - bb::Game::radius : bb::Game::radius;
          // Start just outside so even a zero-speed contact resolves in this update.
          wall.player.pos = {boundary - inward * 0.01, 30};
          // Account for drag so the cases straddle the exact post-drag floor threshold.
          wall.player.vel = {-inward * speed * std::exp(0.26 * dt), vertical};
          wall.update(dt);
          double afterGravity = vertical - bb::Game::gravity * dt;
          double combinedSpeed = std::hypot(speed, afterGravity);
          double dragScale = combinedSpeed > 57 ? (57 + (combinedSpeed - 57) * (1 - dt)) / combinedSpeed : 1;
          double expectedX = inward * std::max(12.0, speed * dragScale * 0.78);
          double expectedY = afterGravity * dragScale;
          if (expectedY > 0)
            expectedY *= 0.75;
          check(near(wall.player.pos.x, boundary), "both wall contacts clamp exactly to the arena boundary");
          check(near(wall.player.vel.x, expectedX),
                "both walls rebound at at least 12 or exactly 78 percent of post-drag speed");
          check(near(wall.player.vel.y, expectedY),
                "both walls lose 25 percent only when vertical speed remains positive after gravity");
          check(wall.particles.size() == 4, "each wall contact emits exactly one bounce burst");
          wall.update(1.0 / 120);
          check(near(wall.player.vel.x, expectedX * std::exp(-0.26 / 120)) &&
                    near(wall.player.vel.y, expectedY - bb::Game::gravity / 120),
                "moving away from either wall applies drag and gravity without another rebound");
          check(inward * (wall.player.pos.x - boundary) > 0 && wall.particles.size() == 4,
                "a rebound moves into the arena without repeating wall effects");
        }
  for (bool right : {false, true})
  {
    bb::Game wall(42);
    isolateCombat(wall);
    double inward = right ? -1.0 : 1.0;
    double boundary = right ? bb::Game::width - bb::Game::radius : bb::Game::radius;
    wall.player.pos = {boundary, 30};
    wall.player.vel = {inward * 2, 20};
    wall.update(1.0 / 120);
    check(near(wall.player.vel.x, inward * 2 * std::exp(-0.26 / 120)) &&
              near(wall.player.vel.y, 20 - bb::Game::gravity / 120) && wall.particles.empty(),
          "slow inward movement from either boundary is neither boosted nor vertically damped");
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
  for (int shells : {0, 1, 3, 4, 5, 6})
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
  {
    constexpr unsigned seedCount = 512;
    int heals[2] = {};
    for (unsigned seed = 0; seed < seedCount; ++seed)
      for (int hp : {1, 2, 3})
        for (int type : {0, 1, 2})
        {
          bb::Game reward(seed);
          isolateCombat(reward);
          reward.player.shells = 0;
          reward.player.reload = 0.4;
          reward.player.hp = hp;
          reward.enemies = {{{40, 20}, {}, 1, type == 2 ? 2.3 : 1.25, 0, 100, type}};
          reward.bullets = {{{40, 20}, {}, 0.3, false}};
          bb::Game surplus = reward;
          surplus.bullets.resize(8, surplus.bullets.front());
          reward.update(0);
          check(reward.enemies.empty() && reward.kills == 1 && reward.player.shells == 3 &&
                    near(reward.player.reload, 0),
                "all enemy types restore exactly three rounds and reset reload once per kill");
          check(reward.combo == 1 && near(reward.comboTime, 2) && reward.bonus == (type == 2 ? 300 : 100),
                "guaranteed healing preserves exact kill counts, combo rewards, and type-specific points");
          check(reward.player.hp == (type == 2 ? std::min(3, hp + 1) : hp),
                "Normal brute kills always heal exactly one HP, capped at three; ordinary kills never heal");
          check(reward.pickups.empty(), "no enemy type drops items");
          if (type != 2)
            continue;
          if (hp < 3 && reward.player.hp == hp + 1)
            ++heals[hp - 1];
          if (hp == 1)
          {
            bb::Game capped = surplus;
            capped.player.hp = 3;
            capped.bullets.resize(1);
            capped.update(0);
            check(capped.player.hp == 3, "guaranteed brute healing never exceeds three HP");
            checkSameNextShot(reward, capped);
          }

          bb::Game replay(seed);
          isolateCombat(replay);
          replay.player.shells = 0;
          replay.player.reload = 0.4;
          replay.player.hp = hp;
          replay.enemies = {{{40, 20}, {}, 1, 2.3, 0, 100, 2}};
          replay.bullets = {{{40, 20}, {}, 0.3, false}};
          replay.update(0);
          check(replay.player.hp == reward.player.hp, "explicit seeds reproduce guaranteed brute healing");
          checkSameNextShot(reward, replay);

          surplus.update(0);
          check(surplus.player.hp == reward.player.hp && surplus.kills == 1 && surplus.player.shells == 3 &&
                    surplus.combo == 1 && surplus.bonus == 300 && near(surplus.comboTime, 2) &&
                    surplus.enemies.empty() && surplus.pickups.empty(),
                "surplus pellets on a dead brute cannot reroll healing or repeat any kill reward");
          check(surplus.particles.size() == reward.particles.size() && reward.particles.size() == 44,
                "surplus pellets do not produce extra impacts on an already killed brute");
          checkSameNextShot(reward, surplus);
          surplus.update(0);
          check(surplus.player.hp == reward.player.hp && surplus.kills == 1 && surplus.player.shells == 3 &&
                    surplus.combo == 1 && surplus.bonus == 300,
                "lingering pellets on later updates cannot repeat healing or kill rewards");
        }
    for (int healed : heals)
      check(healed == seedCount,
            "at both one and two HP, every seeded Normal brute kill heals one HP");
  }
  {
    constexpr unsigned seedCount = 1024;
    int outcomes[3][3] = {};
    for (unsigned seed = 0; seed < seedCount; ++seed)
      for (int mode = 0; mode < 3; ++mode)
      {
        bb::Game reward(seed);
        isolateCombat(reward);
        reward.player.hp = 1;
        reward.shotMode = static_cast<bb::ShotMode>((mode + 1) % 3);
        reward.enemies = {{{40, 20}, {}, 1, 2.3, 0, 100, 2}};
        bb::Bullet pellet{{40, 20}, {}, 0.3, false};
        pellet.shotMode = static_cast<bb::ShotMode>(mode);
        reward.bullets = {pellet};
        bb::Game replay(seed);
        isolateCombat(replay);
        replay.player.hp = 1;
        replay.shotMode = reward.shotMode;
        replay.enemies = reward.enemies;
        replay.bullets = {pellet};
        bb::Game surplus = reward, capped = reward, wounded = reward, normal = reward;
        surplus.bullets.assign(8, pellet);
        capped.player.hp = 3;
        wounded.player.hp = 2;
        normal.bullets.front().shotMode = bb::ShotMode::Normal;
        reward.update(0);
        replay.update(0);
        surplus.update(0);
        capped.update(0);
        wounded.update(0);
        normal.update(0);
        const int healed = reward.player.hp - 1;
        check(healed >= 1 && healed <= (mode == 2 ? 2 : 1),
              "every brute kill heals one HP, with only the killing Scatter pellet eligible for two");
        ++outcomes[mode][healed];
        check(replay.player.hp == reward.player.hp && surplus.player.hp == reward.player.hp &&
                  surplus.kills == 1 && capped.player.hp == 3 && wounded.player.hp == std::min(3, 2 + healed),
              "seeded heal outcomes reproduce, ignore surplus pellets and obey the three-HP cap");
        check(near(reward.scatterBoost, mode == 2 ? 1 : 0),
              "brute boost activation follows the killing pellet's original mode after switching");
        check(normal.player.hp == 2 && (mode == 2 || reward.player.hp == normal.player.hp),
              "Normal and Longshot always heal one HP regardless of the current mode or Scatter bonus roll");
        for (bb::Game *world : {&reward, &replay, &surplus, &capped, &wounded, &normal})
          world->shotMode = bb::ShotMode::Normal;
        checkSameNextShot(reward, replay);
        checkSameNextShot(reward, surplus);
        checkSameNextShot(reward, capped);
        checkSameNextShot(reward, wounded);
        checkSameNextShot(reward, normal);
        surplus.update(0);
        check(surplus.player.hp == reward.player.hp && surplus.kills == 1,
              "surplus Scatter pellets cannot reroll healing on a later update");

        bb::Game ordinary(seed);
        isolateCombat(ordinary);
        ordinary.player.hp = 1;
        ordinary.shotMode = static_cast<bb::ShotMode>((mode + 1) % 3);
        ordinary.bullets = {pellet};
        ordinary.enemies = {{pellet.pos, {}, 1, 1.25, 0, 100, 0}};
        bb::Game hunter = ordinary;
        hunter.enemies.front().type = 1;
        ordinary.update(0);
        hunter.update(0);
        check(ordinary.player.hp == 1 && hunter.player.hp == 1,
              "ordinary and hunter kills never heal in any original shot mode");
        checkSameNextShot(ordinary, hunter);
      }
    check(outcomes[2][1] > seedCount * 0.86 && outcomes[2][1] < seedCount * 0.94,
          "Scatter brute kills heal exactly one HP about 90 percent of the time");
    check(outcomes[2][2] > seedCount * 0.06 && outcomes[2][2] < seedCount * 0.14,
          "Scatter brute kills heal two HP about 10 percent of the time");
    check(outcomes[2][0] == 0, "Scatter brute kills never skip healing");
    for (int mode : {0, 1})
      check(outcomes[mode][1] == seedCount && outcomes[mode][0] == 0 && outcomes[mode][2] == 0,
            "Normal and Longshot brute kills always heal exactly one HP and never double");
  }
  for (unsigned seed = 0; seed < 32; ++seed)
    for (int mode = 0; mode < 3; ++mode)
    for (int hp : {1, 2})
      for (int type : {0, 1, 2})
      {
        bb::Game wounded(seed);
        isolateCombat(wounded);
        wounded.player.hp = hp;
        wounded.player.shells = 0;
        wounded.player.reload = 0.4;
        wounded.enemies = {{{40, 20}, {}, 4, 2.3, 0, 100, type}};
        bb::Bullet pellet{{40, 20}, {}, 0.3, false};
        pellet.shotMode = static_cast<bb::ShotMode>(mode);
        wounded.shotMode = static_cast<bb::ShotMode>((mode + 1) % 3);
        wounded.bullets.assign(3, pellet);
        bb::Game ordinary = wounded;
        ordinary.enemies.front().type = 0;
        wounded.update(0);
        ordinary.update(0);
        check(wounded.player.hp == hp && wounded.player.shells == 0 && near(wounded.player.reload, 0.4) &&
                  wounded.kills == 0 && wounded.combo == 0 && wounded.bonus == 0 && wounded.scatterBoost == 0 &&
                  wounded.enemies.size() == 1 && near(wounded.enemies.front().hp, 1),
              "multiple nonlethal pellets grant no healing, ammo, reload reset, or kill rewards for any type");
        checkSameNextShot(wounded, ordinary);
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
    check(g.player.shells == shells, "landing cannot add ammo at or above four or remove kill bonuses");
    g.pickups = {{g.player.pos, 0}};
    g.update(1.0 / 120);
    check(g.player.shells == shells && g.bonus == 25, "ammo pickups preserve bonus ammo");
  }
  for (int shells : {2, 3})
  {
    g.reset(42);
    g.enemies.clear();
    g.pickups.clear();
    g.player.shells = shells;
    g.player.pos = {40, 1 + bb::Game::radius + 0.01};
    g.player.vel = {0, -2};
    g.platforms = {{10, 1, 60, false}};
    g.update(1.0 / 120);
    check(g.player.shells == 4, "landing restores up to two shells without exceeding four");
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
  check(std::any_of(a.platforms.begin(), a.platforms.end(), [](const auto &p)
                    { return p.y >= 2000; }),
        "world extends indefinitely");
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
        check(near(e.hp, 1), "all regular enemy types have one HP at every height");
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
