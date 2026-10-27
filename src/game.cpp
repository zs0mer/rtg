#include "game.hpp"
#include <numbers>

namespace bb
{
  namespace
  {
    template <class T, class F>
    void removeIf(std::vector<T> &v, F f) { std::erase_if(v, f); }
    double segmentDistance(Vec p, Vec a, Vec b)
    {
      Vec d = b - a;
      double n = d.x * d.x + d.y * d.y;
      double t = n > 0 ? std::clamp(((p.x - a.x) * d.x + (p.y - a.y) * d.y) / n, 0.0, 1.0) : 0;
      return (p - (a + d * t)).length();
    }
  }
  Game::Game(uint32_t seed) : rng(seed) { reset(seed); }
  double Game::random(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); }
  void Game::reset(uint32_t seed)
  {
    rng.seed(seed);
    player = Player{};
    shotMode = ShotMode::Normal;
    modeTarget = {width / 2, random(750, 1000) + 3};
    modeTargetCooldown = 0;
    player.pos.x = width / 2;
    platforms = {{width / 2 - 15, 1, 30, false}};
    enemies.clear();
    bullets.clear();
    particles.clear();
    pickups.clear();
    camera = altitude = time = shake = flash = comboTime = 0;
    lava = -22;
    kills = combo = bonus = 0;
    generated = 1;
    dead = false;
    deathReason.clear();
    generate(160);
  }
  void Game::burst(Vec pos, int count, int color, double speed)
  {
    for (int i = 0; i < count; ++i)
    {
      double a = random(0, 2 * std::numbers::pi), life = random(0.15, 0.65);
      particles.push_back({pos, Vec{std::cos(a), std::sin(a)} * random(speed * 0.2, speed), life, life, color});
    }
  }
  const char *Game::shotModeName() const
  {
    switch (shotMode)
    {
    case ShotMode::Longshot:
      return "LONGSHOT";
    case ShotMode::Scatter:
      return "SCATTER";
    default:
      return "NORMAL";
    }
  }
  bool Game::shoot(Vec target)
  {
    if (dead || player.cooldown > 0 || player.shells <= 0)
      return false;
    Vec aim = (target - player.pos).unit();
    double recoil = 25 * (shotMode == ShotMode::Longshot ? 1.1 : shotMode == ShotMode::Scatter ? 0.9
                                                                                               : 1.0);
    player.vel += aim * -recoil;
    // Preserve momentum, but keep repeated blasts controllable.
    double speed = player.vel.length();
    if (speed > 57)
      player.vel = player.vel * (57 / speed);
    --player.shells;
    player.cooldown = 0.19;
    player.reload = 0;
    shake = 0.23;
    flash = 0.065;
    double angle = std::atan2(aim.y, aim.x);
    int count = shotMode == ShotMode::Longshot ? 2 : shotMode == ShotMode::Scatter ? 24
                                                                                   : 10;
    double spread = shotMode == ShotMode::Longshot ? 0.025 : shotMode == ShotMode::Scatter ? 0.48
                                                                                           : 0.19;
    double life = shotMode == ShotMode::Longshot ? 0.9 : shotMode == ShotMode::Scatter ? 0.14
                                                                                       : 0.34;
    auto killRecoil = shotMode == ShotMode::Scatter ? std::make_shared<KillRecoil>(KillRecoil{aim * (-25 * 0.85), false}) : nullptr;
    for (int i = 0; i < count; ++i)
    {
      double a = angle + random(-spread, spread);
      double velocity = shotMode == ShotMode::Longshot ? random(155, 165) : random(95, 125);
      bullets.push_back({player.pos + aim * 1.1, Vec{std::cos(a), std::sin(a)} * velocity, life, false, killRecoil,
                         shotMode == ShotMode::Longshot ? 2.5 : 0.55});
    }
    burst(player.pos + aim * 1.5, 10, 0, 17);
    return true;
  }
  void Game::damage(Vec from, const char *reason)
  {
    if (dead || player.invulnerable > 0)
      return;
    --player.hp;
    player.invulnerable = 1.25;
    shake = 0.4;
    player.vel += (player.pos - from).unit() * 13;
    burst(player.pos, 22, 2, 20);
    if (player.hp <= 0)
    {
      dead = true;
      deathReason = reason;
    }
  }
  void Game::generate(double top)
  {
    while (generated < top)
    {
      generated += random(7, 11);
      int difficulty = 1 + static_cast<int>(generated / 90);
      double w = random(10, 19), x = random(3, width - w - 3);
      bool spikes = difficulty >= 3 && random(0, 1) < 0.18;
      platforms.push_back({x, generated, w, spikes});
      // Every dangerous ledge has a safe alternative.
      if (spikes)
        platforms.push_back({x < width / 2 ? width - 23 : 7, generated - 2, 15, false});
      // Smooth, slower density ramp: approach peak pressure over roughly 1,400m.
      double spawnChance = std::min(0.78, 0.30 + generated / 3000.0);
      if (generated > 20 && random(0, 1) < spawnChance)
      {
        int type = generated > 1003 && random(0, 1) < 0.16 ? 2 : (difficulty >= 2 && random(0, 1) < 0.45 ? 1 : 0);
        double r = type == 2 ? 2.3 : 1.25;
        enemies.push_back({{random(8, width - 8), generated + 4}, {}, type == 2 ? Enemy::bruteHp : 1.0, r, random(0, 6.28), random(1, 3), type});
      }
      if (random(0, 1) < 0.3)
        pickups.push_back({{x + w / 2, generated + 2.5}, random(0, 1) < 0.23 ? 1 : 0});
    }
  }
  void Game::update(double dt)
  {
    if (dead)
      return;
    dt = std::clamp(dt, 0.0, 1.0 / 30);
    time += dt;
    modeTargetCooldown = std::max(0.0, modeTargetCooldown - dt);
    shake = std::max(0.0, shake - dt);
    flash = std::max(0.0, flash - dt);
    player.cooldown = std::max(0.0, player.cooldown - dt);
    player.invulnerable = std::max(0.0, player.invulnerable - dt);
    comboTime = std::max(0.0, comboTime - dt);
    if (comboTime == 0)
      combo = 0;
    if (player.shells < Player::maxShells)
    {
      player.reload += dt;
      if (player.reload >= Player::reloadSeconds)
      {
        ++player.shells;
        player.reload = 0;
      }
    }
    Vec old = player.pos;
    player.vel.y -= gravity * dt;
    player.vel.x *= std::exp(-0.26 * dt);
    player.pos += player.vel * dt;
    if (player.pos.x < radius)
    {
      player.pos.x = radius;
      player.vel.x = std::abs(player.vel.x) * 0.78;
      if (player.vel.y > 0)
        player.vel.y *= 0.85;
      burst(player.pos, 4, 1, 8);
    }
    if (player.pos.x > width - radius)
    {
      player.pos.x = width - radius;
      player.vel.x = -std::abs(player.vel.x) * 0.78;
      if (player.vel.y > 0)
        player.vel.y *= 0.85;
      burst(player.pos, 4, 1, 8);
    }
    for (const auto &p : platforms)
    {
      if (player.vel.y <= 0 && old.y - radius >= p.y && player.pos.y - radius <= p.y)
      {
        double fraction = (old.y - radius - p.y) / std::max(0.0001, old.y - player.pos.y);
        double crossX = old.x + (player.pos.x - old.x) * fraction;
        if (crossX + radius < p.x || crossX - radius > p.x + p.width)
          continue;
        player.pos.y = p.y + radius;
        player.vel.y = 0;
        // Ground friction bleeds off sideways momentum over time, not on impact.
        player.vel.x *= std::exp(-7.0 * dt);
        if (p.spikes)
        {
          damage({player.pos.x, p.y}, "Shredded by a spike ledge");
          player.vel.y = 16;
        }
        else
        {
          if (old.y - player.pos.y > 0.12)
            burst(player.pos - Vec{0, radius}, 6, 1, 7);
          // Reward a touchdown, not every simulation tick spent on the ledge.
          if (old.y - radius > p.y + 1e-7)
          {
            player.shells = std::max(player.shells, std::min(Player::maxShells, player.shells + 2));
            player.reload = 0;
          }
        }
        break;
      }
    }
    altitude = std::max(altitude, player.pos.y - 3);
    camera = std::max(camera, player.pos.y - 16);
    // Replace a circle only after it has left the playable view, even if skipped.
    while (modeTarget.y + modeTargetRadius < camera - 35)
    {
      modeTarget.y += random(1000, 1500);
      modeTargetCooldown = 0;
    }
    if (time > 4)
      lava += (2.3 + std::min(6.0, altitude / 150)) * dt;
    lava = std::max(lava, camera - 25);
    if (player.pos.y < lava + radius)
    {
      dead = true;
      deathReason = "Caught by the rising meltdown";
      burst(player.pos, 40, 2, 28);
    }
    generate(camera + 150);
    for (auto &e : enemies)
    {
      if (e.pos.y > camera + 85 || e.pos.y < camera - 30)
        continue;
      e.phase += dt;
      Vec delta = player.pos - e.pos;
      if (e.type == 0)
        e.vel = {std::sin(e.phase * 1.8) * 6, std::cos(e.phase * 1.3) * 2};
      else
        e.vel = delta.unit() * (e.type == 2 ? 3.5 : 7.5);
      e.pos += e.vel * dt;
      e.pos.x = std::clamp(e.pos.x, e.radius, width - e.radius);
      if (delta.length() < e.radius + radius)
        damage(e.pos, "Rammed by a hunter");
      e.cooldown -= dt;
      if (e.type != 0 && e.cooldown <= 0 && delta.length() < 48)
      {
        bullets.push_back({e.pos, delta.unit() * (13 + std::min(12, sector())), 4, true});
        e.cooldown = e.type == 2 ? 0.9 : 1.7;
      }
    }
    for (auto &b : bullets)
    {
      Vec before = b.pos;
      b.pos += b.vel * dt;
      b.life -= dt;
      if (b.hostile)
      {
        if (segmentDistance(player.pos, before, b.pos) < radius + 0.4)
        {
          damage(b.pos, "Hit by a hunter's plasma");
          b.life = 0;
        }
      }
      else
      {
        if (segmentDistance(modeTarget, before, b.pos) <= modeTargetRadius + b.hitRadius)
        {
          b.life = 0;
          if (modeTargetCooldown == 0)
          {
            int offset = std::uniform_int_distribution<int>(1, 2)(rng);
            shotMode = static_cast<ShotMode>((static_cast<int>(shotMode) + offset) % 3);
            // Longer than any pellet lifetime: one blast can never reroll twice.
            modeTargetCooldown = 1.25;
            burst(modeTarget, 50, 1, 24);
            shake = 0.3;
          }
          continue;
        }
        for (auto &e : enemies)
        {
          if (e.hp <= 0 || segmentDistance(e.pos, before, b.pos) > e.radius + b.hitRadius)
            continue;
          e.hp -= 1;
          e.vel += b.vel * 0.08;
          b.life = 0;
          burst(b.pos, 4, 0, 10);
          if (e.hp <= 0)
          {
            if (b.killRecoil && !b.killRecoil->used)
            {
              b.killRecoil->used = true;
              player.vel += b.killRecoil->impulse;
              double speed = player.vel.length();
              if (speed > 57)
                player.vel = player.vel * (57 / speed);
              shake = 0.23;
            }
            player.shells = std::min(Player::maxBonusShells, player.shells + 3);
            player.reload = 0;
            if (e.type == 2)
              player.hp = std::min(3, player.hp + 1);
            ++kills;
            ++combo;
            comboTime = 2;
            bonus += 100 * std::min(combo, 8) * (e.type == 2 ? 3 : 1);
            burst(e.pos, e.type == 2 ? 40 : 24, 2, 22);
            shake = 0.18;
          }
          break;
        }
      }
    }
    for (auto &p : particles)
    {
      p.pos += p.vel * dt;
      p.vel.y -= 15 * dt;
      p.life -= dt;
    }
    for (auto &p : pickups)
    {
      if ((player.pos - p.pos).length() < 2.4)
      {
        if (p.type == 1)
          player.hp = std::min(3, player.hp + 1);
        else
        {
          player.shells = std::max(player.shells, Player::maxShells);
          player.reload = 0;
          bonus += 25;
        }
        burst(p.pos, 12, p.type == 1 ? 3 : 1, 11);
        p.pos.y = -1e6;
      }
    }
    removeIf(bullets, [&](const Bullet &b)
             { return b.life <= 0 || b.pos.y < camera - 35 || b.pos.x < 0 || b.pos.x > width; });
    removeIf(enemies, [&](const Enemy &e)
             { return e.hp <= 0 || e.pos.y < camera - 35; });
    removeIf(platforms, [&](const Platform &p)
             { return p.y < camera - 35; });
    removeIf(pickups, [&](const Pickup &p)
             { return p.pos.y < camera - 35; });
    removeIf(particles, [](const Particle &p)
             { return p.life <= 0; });
  }
}