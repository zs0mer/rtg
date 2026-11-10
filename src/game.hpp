#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace bb
{
  struct Vec
  {
    double x = 0, y = 0;
    Vec operator+(Vec b) const { return {x + b.x, y + b.y}; }
    Vec operator-(Vec b) const { return {x - b.x, y - b.y}; }
    Vec operator*(double s) const { return {x * s, y * s}; }
    Vec &operator+=(Vec b)
    {
      x += b.x;
      y += b.y;
      return *this;
    }
    double length() const { return std::hypot(x, y); }
    Vec unit() const
    {
      double n = length();
      return n > 0.001 ? *this * (1 / n) : Vec{0, -1};
    }
  };
  struct Platform
  {
    double x, y, width;
    bool spikes = false;
  };
  struct Enemy
  {
    static constexpr double bruteHp = 12;
    Vec pos, vel;
    double hp, radius, phase, cooldown;
    int type;
  };
  enum class ShotMode
  {
    Normal,
    Longshot,
    Scatter
  };
  struct KillRecoil
  {
    Vec impulse;
    bool used = false;
  };
  struct Bullet
  {
    Vec pos, vel;
    double life;
    bool hostile;
    std::shared_ptr<KillRecoil> killRecoil = nullptr;
    double hitRadius = 0.55;
    double damage = 1;
    ShotMode shotMode = ShotMode::Normal;
    bool bruteShot = false;
  };
  struct Particle
  {
    Vec pos, vel;
    double life, total;
    int color;
    ShotMode shotMode = ShotMode::Normal;
  };
  struct Pickup
  {
    Vec pos;
    int type;
  };
  struct Player
  {
    static constexpr int maxShells = 4;
    static constexpr int maxBonusShells = 6;
    static constexpr int killAmmo = 3;
    static constexpr double reloadSeconds = 0.75;
    Vec pos{50, 3}, vel{};
    int hp = 3, shells = maxShells;
    double cooldown = 0, reload = 0, invulnerable = 0;
  };

  class Game
  {
  public:
    static constexpr double width = 100, gravity = 34, radius = 0.85;
    static constexpr double softSpeedThreshold = 57, overspeedSeconds = 1;
    static constexpr double heatShieldChargeSeconds = 3, heatShieldSpeedThreshold = 60;
    static constexpr double heatShieldGraceSeconds = 1, heatShieldRadius = 2.5;
        static constexpr double heatShieldBruteImpulse = 80;
    Vec modeTarget{}; // Displayed height is world Y minus 3; randomized on each spawn.
    static constexpr double modeTargetRadius = 6;
    double modeTargetCooldown = 0;
    Player player;
    ShotMode shotMode = ShotMode::Normal, flashMode = ShotMode::Normal;
    std::vector<Platform> platforms;
    std::vector<Enemy> enemies;
    std::vector<Bullet> bullets;
    std::vector<Particle> particles;
    std::vector<Pickup> pickups;
    double camera = 0, altitude = 0, lava = -22, time = 0, shake = 0, flash = 0;
    static constexpr double scatterBoostSeconds = 1;
    double comboTime = 0, scatterBoost = 0, overspeedTime = 0, heatShieldBelowTime = 0;
    int kills = 0, combo = 0, bonus = 0;
    bool dead = false;
    bool debugUsed = false;
    std::string deathReason;
    explicit Game(uint32_t seed = std::random_device{}());
    void reset(uint32_t seed = std::random_device{}());
    bool shoot(Vec target);
    bool debugSwitchMode();
    const char *shotModeName() const;
    void update(double dt);
    void generate(double top);
    bool heatShieldActive() const
    {
      return !dead && overspeedTime >= heatShieldChargeSeconds && heatShieldBelowTime < heatShieldGraceSeconds;
    }
    int score() const { return static_cast<int>(altitude); }
    int sector() const { return 1 + static_cast<int>(altitude / 90); }

  private:
    std::mt19937 rng;
    double generated = 0;
    bool modeTargetContact = false;
    double random(double lo, double hi);
    void activateModeTarget();
    void burst(Vec pos, int count, int color, double speed, ShotMode mode = ShotMode::Normal);
    void damage(Vec from, const char *reason, bool blockedByHeatShield = false);
  };
}
