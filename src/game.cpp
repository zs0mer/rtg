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
    // Particle, ring and popup palette indices shared with the renderer.
    constexpr int colorPlatform = 1, colorDanger = 2, colorRepair = 3, colorFireYellow = 4, colorFireRed = 5,
                  colorAmmo = 6, colorWhite = 7;
    bool drifts(const Enemy &e) { return e.type == Enemy::drone || e.type == Enemy::volatileDrone; }
  }
  Game::Game(uint32_t seed) : rng(seed) { reset(seed); }
  double Game::random(double lo, double hi) { return std::uniform_real_distribution<double>(lo, hi)(rng); }
  void Game::reset(uint32_t seed)
  {
    rng.seed(seed);
    player = Player{};
    shotMode = flashMode = ShotMode::Normal;
    modeTarget = {random(modeTargetRadius + 3, width - modeTargetRadius - 3), random(750, 1000) + 3};
    modeTargetCooldown = 0;
    modeTargetContact = false;
    player.pos.x = width / 2;
    platforms = {{width / 2 - 15, 1, 30, false}};
    enemies.clear();
    bullets.clear();
    particles.clear();
    pickups.clear();
    rings.clear();
    popups.clear();
    banner.clear();
    camera = altitude = time = shake = flash = comboTime = scatterBoost = overspeedTime = heatShieldBelowTime = 0;
    hitStop = deathTime = hurtFlash = topSpeed = bannerTime = heatShieldBreak = heatShieldBreakHeat = 0;
    lava = -22;
    kills = combo = bonus = bestCombo = 0;
    generated = 1;
    // The first formation waits until the player has learned to climb.
    nextSwarm = random(300, 380);
    nextMilestone = 250;
    dead = false;
    debugUsed = false;
    deathReason.clear();
    generate(160);
  }
  void Game::burst(Vec pos, int count, int color, double speed, ShotMode mode)
  {
    for (int i = 0; i < count; ++i)
    {
      double a = random(0, 2 * std::numbers::pi), life = random(0.15, 0.65);
      particles.push_back({pos, Vec{std::cos(a), std::sin(a)} * random(speed * 0.2, speed), life, life, color, mode});
    }
  }
  // Rings, popups and banners never draw from the RNG, so effects cannot change a seeded run.
  void Game::ring(Vec pos, double radius, int color, double life)
  {
    if (rings.size() < 48)
      rings.push_back({pos, radius, life, life, color});
  }
  void Game::popup(Vec pos, std::string text, int color, double life)
  {
    if (popups.size() >= 16)
      popups.erase(popups.begin());
    popups.push_back({pos, std::move(text), life, life, color});
  }
  void Game::announce(std::string text, double seconds)
  {
    banner = std::move(text);
    bannerTime = seconds;
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
  bool Game::debugSwitchMode()
  {
    if (dead)
      return false;
    shotMode = static_cast<ShotMode>((static_cast<int>(shotMode) + 1) % 3);
    debugUsed = true;
    return true;
  }
  bool Game::shoot(Vec target)
  {
    if (dead || player.cooldown > 0 || player.shells <= 0)
      return false;
    Vec aim = (target - player.pos).unit();
    const bool boostedScatter = shotMode == ShotMode::Scatter && scatterBoost > 0;
    double baseRecoil = 25 - 10 * std::clamp((player.vel.length() - 25) / (softSpeedThreshold - 25), 0.0, 1.0);
    double recoil = baseRecoil * (shotMode == ShotMode::Scatter ? (boostedScatter ? 1.70 : 0.85) : 1.0);
    Vec impulse = aim * -recoil;
    if (shotMode == ShotMode::Longshot)
    {
      impulse.x *= 0.75;
      impulse.y *= 1.1;
    }
    player.vel += impulse;

    --player.shells;
    player.cooldown = 0.19;
    player.reload = 0;
    shake = 0.23;
    flash = 0.065;
    flashMode = shotMode;
    double angle = std::atan2(aim.y, aim.x);
    int count = shotMode == ShotMode::Longshot ? 2 : shotMode == ShotMode::Scatter ? 24
                                                                                   : 10;
    double spread = shotMode == ShotMode::Longshot ? 0.025 : shotMode == ShotMode::Scatter ? 0.55
                                                                                           : 0.19;
    double life = shotMode == ShotMode::Longshot ? 0.9 : shotMode == ShotMode::Scatter ? 0.14
                                                                                       : 0.30;
    // Boosted blasts already received their kill recoil; older blasts retain their unused bonus.
    auto killRecoil = shotMode == ShotMode::Scatter ? std::make_shared<KillRecoil>(KillRecoil{aim * (-baseRecoil * 0.85), boostedScatter}) : nullptr;
    for (int i = 0; i < count; ++i)
    {
      double a = angle + random(-spread, spread);
      double velocity = shotMode == ShotMode::Longshot ? random(155, 165) : random(95, 125);
      bullets.push_back({player.pos + aim * 1.1, Vec{std::cos(a), std::sin(a)} * velocity, life, false, killRecoil,
                         shotMode == ShotMode::Longshot ? 2.5 : 0.55,
                         shotMode == ShotMode::Longshot ? 5.0 : 1.0, shotMode});
    }
    burst(player.pos + aim * 1.5, 10, 0, 17, shotMode);
    return true;
  }
  void Game::activateModeTarget()
  {
    if (modeTargetCooldown > 0)
      return;
    int offset = std::uniform_int_distribution<int>(1, 2)(rng);
    shotMode = static_cast<ShotMode>((static_cast<int>(shotMode) + offset) % 3);

    modeTargetCooldown = 1.25;
    burst(modeTarget, 50, 1, 24);
    shake = 0.3;
    ring(modeTarget, modeTargetRadius + 5, colorWhite, 0.45);
    announce(shotModeName());
  }
  void Game::die(const char *reason)
  {
    dead = true;
    deathReason = reason;
    burst(player.pos, 40, colorDanger, 28);
    ring(player.pos, 12, colorDanger, 0.6);
    ring(player.pos, 6, colorWhite, 0.35);
    shake = 0.7;
    hitStop = std::max(hitStop, 0.16);
  }
  void Game::damage(Vec from, const char *reason, bool blockedByHeatShield, bool knockback)
  {
    if (dead || player.invulnerable > 0 || (blockedByHeatShield && heatShieldActive()))
      return;
    --player.hp;
    player.invulnerable = 1.25;
    shake = 0.4;
    hurtFlash = 0.4;
    hitStop = std::max(hitStop, 0.07);
    if (knockback)
      player.vel += (player.pos - from).unit() * 13;
    burst(player.pos, 22, 2, 20);
    if (player.hp <= 0)
      die(reason);
  }

  void Game::explode(Vec at, bool shotTriggered)
  {
    burst(at, 30, colorFireYellow, 30);
    burst(at, 16, colorFireRed, 20);
    ring(at, blastRadius, colorFireYellow, 0.38);
    ring(at, blastRadius * 0.6, colorFireRed, 0.28);
    shake = std::max(shake, 0.45);
    hitStop = std::max(hitStop, 0.04);
    for (auto &e : enemies)
    {
      if (e.hp <= 0 || (e.pos - at).length() > blastRadius + e.radius)
        continue;
      if (e.type == Enemy::volatileDrone)
      {
        // Neighbours cook off a beat later, so chains ripple outward instead of popping at once.
        if (e.fuse <= 0)
          e.fuse = 0.09;
      }
      else if (e.type == Enemy::brute)
      {
        e.hp -= blastBruteDamage;
        e.flash = 0.12;
        if (e.hp <= 0)
          kill(e, KillCause::Blast);
      }
      else
        kill(e, KillCause::Blast);
    }
    for (auto &b : bullets)
      if (b.hostile && b.life > 0 && (b.pos - at).length() < blastRadius)
        b.life = 0;
    // A blast by itself never costs hull; direct contact with a volatile does.
    Vec away = player.pos - at;
    double distance = away.length();
    if (distance < blastRadius + radius)
    {
      Vec direction = distance > 0.001 ? away * (1 / distance) : Vec{0, 1};
      direction = (direction + Vec{0, 0.5}).unit();
      double falloff = std::clamp(distance / blastRadius, 0.0, 1.0);
      player.vel += direction * (blastMaxImpulse + (blastMinImpulse - blastMaxImpulse) * falloff);
      if (shotTriggered)
        player.vel.y += shotVolatileLaunch * (1 - falloff);
    }
  }
  void Game::kill(Enemy &e, KillCause cause, ShotMode mode)
  {
    const bool brute = e.type == Enemy::brute;
    e.hp = std::min(e.hp, 0.0);
    player.shells = std::min(Player::maxBonusShells, player.shells + Player::killAmmo);
    player.reload = 0;
    if (brute)
    {
      double roll = random(0, 1);
      int heal = cause == KillCause::Pellet && mode == ShotMode::Scatter && roll < 0.10 ? 2 : 1;
      player.hp = std::min(3, player.hp + heal);
    }
    ++kills;
    ++combo;
    bestCombo = std::max(bestCombo, combo);
    comboTime = 2;
    bonus += 100 * std::min(combo, 8) * (brute ? 3 : 1);
    burst(e.pos, brute ? 40 : 24, cause == KillCause::Shield ? colorFireYellow : colorDanger, 22);
    shake = 0.18;
    // Ordinary kills hold for a single frame; chained kills must never feel like stutter.
    hitStop = std::max(hitStop, brute ? 0.05 : 0.012);
    ring(e.pos, e.radius + (brute ? 5.0 : 2.5), brute ? colorRepair : colorWhite, brute ? 0.4 : 0.22);
    if (brute)
      popup(e.pos + Vec{0, 3}, "+HP", colorRepair, 1.1);

    else if (combo >= 2)
      popup(e.pos + Vec{0, 2}, "x" + std::to_string(combo), colorAmmo, 0.7);

    if (e.type == Enemy::volatileDrone)
      explode(e.pos, cause == KillCause::Pellet);
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
        if (type == Enemy::drone && generated > 140 && random(0, 1) < 0.25)
          type = Enemy::volatileDrone;
        double r = type == 2 ? 2.3 : 1.25;
        enemies.push_back({{random(8, width - 8), generated + 4}, {}, type == 2 ? Enemy::bruteHp : 1.0, r, random(0, 6.28), random(1, 3), type});
      }
      if (random(0, 1) < 0.3)
      {
        pickups.push_back({{x + w / 2, generated + 2.5}, random(0, 1) < 0.23 ? Pickup::repair : Pickup::ammo});
      }
      if (generated >= nextSwarm)
      {
        // A formation shares one phase so it flies in unison: a ready-made streak.
        int count = std::uniform_int_distribution<int>(5, 7)(rng);
        double centre = random(22, width - 22), phase = random(0, 6.28);
        bool vee = random(0, 1) < 0.5;
        // Later formations may carry a volatile at their heart: one shot clears the lot.
        bool primed = generated > 500 && random(0, 1) < 0.4;
        for (int i = 0; i < count; ++i)
        {
          double offset = i - (count - 1) / 2.0;
          int type = primed && i == count / 2 ? Enemy::volatileDrone : Enemy::drone;
          enemies.push_back({{centre + offset * 4.2, generated + 5.5 + (vee ? std::abs(offset) * 1.6 : 0)}, {}, 1.0, 1.25, phase, random(1, 3), type});
        }
        nextSwarm = generated + random(200, 300);
      }
    }
  }
  void Game::animateEffects(double dt)
  {
    for (auto &p : particles)
    {
      p.pos += p.vel * dt;
      p.vel.y -= 15 * dt;
      p.life -= dt;
    }
    for (auto &r : rings)
      r.life -= dt;
    for (auto &p : popups)
    {
      p.pos.y += 5 * dt;
      p.life -= dt;
    }
    hurtFlash = std::max(0.0, hurtFlash - dt);
    heatShieldBreak = std::max(0.0, heatShieldBreak - dt);

    bannerTime = std::max(0.0, bannerTime - dt);
    removeIf(particles, [](const Particle &p)
             { return p.life <= 0; });
    removeIf(rings, [](const Ring &r)
             { return r.life <= 0; });
    removeIf(popups, [](const Popup &p)
             { return p.life <= 0; });
  }
  void Game::update(double dt)
  {
    dt = std::clamp(dt, 0.0, 1.0 / 30);
    if (dead)
    {
      // The run is frozen, but the wreckage keeps flying so death reads as an event, not a hang.
      deathTime += dt;
      shake = std::max(0.0, shake - dt);
      flash = std::max(0.0, flash - dt);
      animateEffects(dt);
      return;
    }
    time += dt;
    const double shieldHeat = heatShieldHeat();
    modeTargetCooldown = std::max(0.0, modeTargetCooldown - dt);
    scatterBoost = std::max(0.0, scatterBoost - dt);

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
    double speed = player.vel.length();
    if (speed > softSpeedThreshold)
    {
      double deceleration = (speed - softSpeedThreshold) / overspeedSeconds;
      player.vel = player.vel * ((speed - deceleration * dt) / speed);
    }
    player.pos += player.vel * dt;
    if (player.pos.x < radius)
    {
      player.pos.x = radius;
      player.vel.x = std::max(12.0, std::abs(player.vel.x) * 0.78);
      if (player.vel.y > 0)
        player.vel.y *= 0.75;
      burst(player.pos, 4, 1, 8);
    }
    if (player.pos.x > width - radius)
    {
      player.pos.x = width - radius;
      player.vel.x = -std::max(12.0, std::abs(player.vel.x) * 0.78);
      if (player.vel.y > 0)
        player.vel.y *= 0.75;
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
    double shieldSpeed = player.vel.length();
    topSpeed = std::max(topSpeed, shieldSpeed);
    const bool shieldWasActive = heatShieldActive();
    if (shieldSpeed > heatShieldSpeedThreshold ||
        (heatShieldActive() && shieldSpeed >= heatShieldSpeedThreshold))
    {
      heatShieldBelowTime = 0;
      overspeedTime = std::min(heatShieldChargeSeconds, overspeedTime + dt);
      if (heatShieldChargeSeconds - overspeedTime < 1e-9)
        overspeedTime = heatShieldChargeSeconds;
    }
    else if (overspeedTime >= heatShieldChargeSeconds)
    {
      heatShieldBelowTime = std::min(heatShieldGraceSeconds, heatShieldBelowTime + dt);
      if (heatShieldGraceSeconds - heatShieldBelowTime < 1e-9)
      {
        heatShieldBelowTime = heatShieldGraceSeconds;
        overspeedTime = 0;
      }
    }
    else
    {
      overspeedTime = 0;
      heatShieldBelowTime = 0;
    }
    if (!shieldWasActive && heatShieldActive())
    {
      announce("HEAT SHIELD");
      ring(player.pos, 6, colorFireYellow, 0.4);
    }
    altitude = std::max(altitude, player.pos.y - 3);
    while (altitude >= nextMilestone)
    {
      announce(std::to_string(static_cast<int>(nextMilestone)) + " m");
      nextMilestone += 250;
    }
    camera = std::max(camera, player.pos.y - 16);
    // Replace a circle only after it has left the playable view, even if skipped.
    while (modeTarget.y + modeTargetRadius < camera - 35)
    {
      modeTarget.x = random(modeTargetRadius + 3, width - modeTargetRadius - 3);
      modeTarget.y += random(1000, 1500);
      modeTargetCooldown = 0;
      modeTargetContact = false;
    }
    if (!modeTargetContact && segmentDistance(modeTarget, old, player.pos) <= modeTargetRadius + radius)
      activateModeTarget();
    modeTargetContact = (player.pos - modeTarget).length() <= modeTargetRadius + radius;
    if (time > 4)
      lava += (2.3 + std::min(6.0, altitude / 150)) * dt;
    lava = std::max(lava, camera - 25);
    if (player.pos.y < lava + radius)
      die("Caught by the rising meltdown");
    generate(camera + 150);
    for (auto &e : enemies)
    {
      if (e.hp <= 0)
        continue;
      if (e.pos.y > camera + 85 || e.pos.y < camera - 30)
        continue;
      e.phase += dt;
      e.flash = std::max(0.0, e.flash - dt);
      if (e.fuse > 0)
      {
        e.fuse -= dt;
        if (e.fuse <= 0)
        {
          kill(e, KillCause::Blast);
          continue;
        }
      }
      Vec delta = player.pos - e.pos;
      if (drifts(e))
        e.vel = {std::sin(e.phase * 1.8) * 6, std::cos(e.phase * 1.3) * 2};
      else
        e.vel = delta.unit() * (e.type == 2 ? 3.5 : 7.5);
      e.pos += e.vel * dt;
      e.pos.x = std::clamp(e.pos.x, e.radius, width - e.radius);
      if (e.type == 2 && heatShieldActive() && segmentDistance(e.pos, old, player.pos) < e.radius + radius)
      {
        double away = old.x - e.pos.x;
        if (std::abs(away) < 0.001)
          away = std::abs(player.vel.x) > 0.001 ? -player.vel.x : (e.pos.x <= width / 2 ? 1.0 : -1.0);
        double side = away > 0 ? 1.0 : -1.0;
        player.vel.x = side * (std::abs(player.vel.x) + heatShieldBruteImpulse);
        player.pos.x = e.pos.x + side * (e.radius + radius + 0.05);
        overspeedTime = heatShieldBelowTime = 0;
        // Keep the intended horizontal bounce and vertical momentum; damage supplies feedback
        // and invulnerability, but no second knockback impulse.
        damage(e.pos, "Crushed by a brute", false, false);
        burst(player.pos, 18, 4, 24);
        burst(player.pos, 12, 5, 18);
        shake = 0.4;
        continue;
      }
      bool shieldContact = e.type != Enemy::brute && heatShieldActive() &&
                           segmentDistance(e.pos, old, player.pos) < e.radius + heatShieldRadius;
      if (e.type == Enemy::volatileDrone &&
          (shieldContact || segmentDistance(e.pos, old, player.pos) < e.radius + radius))
      {
        kill(e, shieldContact ? KillCause::Shield : KillCause::Blast);
        // Only direct contact hurts: remote pellets and chain blasts stay safe.
        damage(e.pos, "Caught in a volatile explosion", false, false);
        player.vel.y = std::max(0.0, player.vel.y) +
                       (shieldContact ? shieldVolatileLaunch : volatileContactLaunch);
        continue;
      }
      if (shieldContact)
      {
        kill(e, KillCause::Shield);
        continue;
      }
      if (delta.length() < e.radius + radius)
        damage(e.pos, e.type == Enemy::brute ? "Flattened by a brute" : e.type == Enemy::hunter ? "Rammed by a hunter"
                                                                    : "Rammed by a drone",
               e.type != 2);
      e.cooldown -= dt;
      if (!drifts(e) && e.cooldown <= 0 && delta.length() < 48)
      {
        Bullet projectile{e.pos, delta.unit() * (13 + std::min(12, sector())), 4, true};
        projectile.bruteShot = e.type == 2;
        bullets.push_back(projectile);
        e.cooldown = e.type == 2 ? 0.9 : 1.7;
      }
    }
    for (auto &b : bullets)
    {
      // A blast earlier in this step may already have spent this shot.
      if (b.life <= 0)
        continue;
      Vec before = b.pos;
      b.pos += b.vel * dt;
      b.life -= dt;
      if (b.hostile)
      {
        double hitRadius = heatShieldActive() && !b.bruteShot ? heatShieldRadius : radius;
        if (segmentDistance(player.pos, before, b.pos) < hitRadius + 0.4)
        {
          damage(b.pos, b.bruteShot ? "Hit by a brute's plasma" : "Hit by a hunter's plasma", !b.bruteShot);
          b.life = 0;
        }
      }
      else
      {

        for (auto &e : enemies)
        {
          if (e.hp <= 0 || segmentDistance(e.pos, before, b.pos) > e.radius + b.hitRadius)
            continue;
          e.hp -= b.damage;
          e.vel += b.vel * 0.08;
          b.life = 0;
          burst(b.pos, 4, 0, 10, b.shotMode);
          if (e.hp <= 0)
          {
            if (b.shotMode == ShotMode::Scatter)
              scatterBoost = scatterBoostSeconds;
            if (b.killRecoil && !b.killRecoil->used)
            {
              b.killRecoil->used = true;
              player.vel += b.killRecoil->impulse;

              shake = 0.23;
            }
            kill(e, KillCause::Pellet, b.shotMode);
          }
          else
            e.flash = 0.1;
          break;
        }
      }
    }
    if (heatShieldActive() && dt > 0)
    {
      constexpr double emberInterval = 0.04;
      int ember = static_cast<int>(std::floor(time / emberInterval));
      if (ember > static_cast<int>(std::floor((time - dt) / emberInterval)))
      {
        double phase = ember * 2.399963;
        double life = 0.4 + 0.1 * std::sin(phase);
        double side = ember % 2 ? -1.0 : 1.0;
        Vec pos = player.pos + Vec{side * 1.4, 1.1};
        Vec velocity = player.vel * 0.8 + Vec{side * 2, -6};
        particles.push_back({pos, velocity, life, life, ember % 2 ? 4 : 5});
      }
    }
    // Expiry and brute contact both break the shield; death has its own explosion.
    if (shieldHeat > 0 && !heatShieldActive() && !dead)
    {
      heatShieldBreak = heatShieldBreakSeconds;
      heatShieldBreakHeat = shieldHeat;
    }
    animateEffects(dt);
    for (auto &p : pickups)
    {
      if ((player.pos - p.pos).length() < 2.4)
      {
        if (p.type == Pickup::repair)
        {
          player.hp = std::min(3, player.hp + 1);
          popup(p.pos + Vec{0, 2}, "+HP", colorRepair);
        }

        else
        {
          player.shells = std::max(player.shells, Player::maxShells);
          player.reload = 0;
          bonus += 25;
        }
        burst(p.pos, 12, p.type == Pickup::repair ? colorRepair : colorPlatform, 11);
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
  }
}
