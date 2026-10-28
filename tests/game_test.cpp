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
    bb::Game fresh(42);
    bool varied = false;
    for (int i = 0; i < 3; ++i)
    {
      auto previous = fresh.platforms;
      fresh.player.hp = 1;
      fresh.dead = true;
      fresh.reset();
      varied = varied || fresh.platforms.size() != previous.size() ||
               !near(fresh.platforms[1].x, previous[1].x) ||
               !near(fresh.platforms[1].y, previous[1].y) ||
               !near(fresh.platforms[1].width, previous[1].width);
      check(!fresh.dead && fresh.player.hp == 3 && fresh.player.shells == bb::Player::maxShells &&
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
      combat.generate(200);
      combat.platforms.clear();
      combat.enemies.clear();
      combat.pickups.clear();
      combat.player.pos = {50, 30};
      combat.shotMode = static_cast<bb::ShotMode>(mode);
      combat.shoot({50, 20});
      bb::Bullet pellet = combat.bullets.front();
      pellet.pos = {40, 20};
      pellet.vel = {};
      double hp = type == 2 ? bb::Enemy::bruteHp : 1;
      double damage = mode == 1 ? 5 : mode == 2 ? 0.85 : 1;
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
                "lethal mode-specific damage grants kill rewards exactly once");
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
    int pelletsPerKill = scatter ? 2 : 1;
    recoil.bullets.resize(static_cast<size_t>(2 * pelletsPerKill));
    for (int i = 0; i < 2; ++i)
    {
      bb::Vec pos{40.0 + i * 20, 20};
      recoil.enemies.push_back({pos, {}, 1, 1.25, 0, 100, 0});
      for (int j = 0; j < pelletsPerKill; ++j)
      {
        auto &pellet = recoil.bullets[static_cast<size_t>(i * pelletsPerKill + j)];
        pellet.pos = pos;
        pellet.vel = {};
      }
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
      check(modes.player.shells == 3 && near(modes.player.vel.x, mode == 1 ? -18.75 : mode == 2 ? -22.5
                                                                                                : -25),
            "modes use one ammo with 0.75x longshot and 0.9x scatter horizontal recoil");
      for (const auto &pellet : modes.bullets)
      {
        check(pellet.shotMode == modes.shotMode, "every fired pellet is tagged with the firing mode");
        check(near(pellet.hitRadius, mode == 1 ? 2.5 : 0.55), "only longshot gets a much larger hit radius");
        check(near(pellet.damage, mode == 1 ? 5 : mode == 2 ? 0.85 : 1),
              "each mode assigns its damage to every fired pellet");
        double range = pellet.vel.length() * pellet.life;
        check(mode == 1 ? range > 139 && range < 150 : mode == 2 ? range > 13 && range < 18
                                                                 : range > 32 && range < 43,
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
  for (int mode = 0; mode < 3; ++mode)
    for (bb::Vec aim : {bb::Vec{1, 0}, bb::Vec{0, -1}, bb::Vec{3, -4}, bb::Vec{-3, 4}})
      for (bb::Vec momentum : {bb::Vec{}, bb::Vec{7, -9}, bb::Vec{-50, 45}})
      {
        bb::Game recoil(42);
        recoil.shotMode = static_cast<bb::ShotMode>(mode);
        recoil.player.vel = momentum;
        bb::Vec unit = aim.unit();
        double horizontal = mode == 1 ? 18.75 : mode == 2 ? 22.5 : 25;
        double vertical = mode == 1 ? 27.5 : mode == 2 ? 22.5 : 25;
        bb::Vec expected = momentum + bb::Vec{-unit.x * horizontal, -unit.y * vertical};
        if (expected.length() > 57)
          expected = expected * (57 / expected.length());
        check(recoil.shoot(recoil.player.pos + aim), "axis and momentum recoil test fires");
        check(near(recoil.player.vel.x, expected.x) && near(recoil.player.vel.y, expected.y),
              "recoil scales each aim axis before adding momentum and applying the 57 speed cap");
        check(recoil.player.vel.length() <= 57 + 1e-7, "all firing modes retain the 57 speed cap");
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
