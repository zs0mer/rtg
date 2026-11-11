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
    // Volatiles drift like drones, but detonate when destroyed.
    static constexpr int drone = 0, hunter = 1, brute = 2, volatileDrone = 3;
    Vec pos, vel;
    double hp, radius, phase, cooldown;
    int type;
    double flash = 0; // Seconds of hit flash remaining; cosmetic only.
    double fuse = 0;  // Armed volatiles detonate when this runs out.
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
    static constexpr int ammo = 0, repair = 1;
    Vec pos;
    int type;
  };
  // Expanding shockwave outline; cosmetic only.
  struct Ring
  {
    Vec pos;
    double radius, life, total;
    int color;
  };
  // Short floating label in world space; cosmetic only.
  struct Popup
  {
    Vec pos;
    std::string text;
    double life, total;
    int color;
  };
  enum class KillCause
  {
    Pellet,
    Shield,
    Blast
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
    static constexpr double heatShieldBreakSeconds = 0.4;

    static constexpr double blastRadius = 11, blastBruteDamage = 6;
    static constexpr double blastMinImpulse = 12, blastMaxImpulse = 38;
    static constexpr double shieldVolatileLaunch = 80, volatileContactLaunch = 40;
    static constexpr double shotVolatileLaunch = 120;

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
    std::vector<Ring> rings;
    std::vector<Popup> popups;
    double camera = 0, altitude = 0, lava = -22, time = 0, shake = 0, flash = 0;
    // hitStop is consumed by the frontend; the simulation itself never waits on it.
    double hitStop = 0, deathTime = 0, hurtFlash = 0, topSpeed = 0;
    double bannerTime = 0;
    // Cosmetic: seconds left of the shield's break-up, and how hot it was when it went.
    double heatShieldBreak = 0, heatShieldBreakHeat = 0;
    std::string banner;
    int bestCombo = 0;
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
    void popup(Vec pos, std::string text, int color, double life = 0.9);
    void ring(Vec pos, double radius, int color, double life = 0.3);
    void announce(std::string text, double seconds = 1.6);
    // Animation clock that keeps running through the death sequence.
    double clock() const { return time + deathTime; }
    bool heatShieldActive() const
    {
      return !dead && overspeedTime >= heatShieldChargeSeconds && heatShieldBelowTime < heatShieldGraceSeconds;
    }
    // 1 at full heat, cooling through the slowdown grace period; 0 without a shield.
    double heatShieldHeat() const
    {
      return heatShieldActive() ? 1 - 0.55 * heatShieldBelowTime / heatShieldGraceSeconds : 0;
    }
    int score() const { return static_cast<int>(altitude); }
    int sector() const { return 1 + static_cast<int>(altitude / 90); }

  private:
    std::mt19937 rng;
    double generated = 0, nextSwarm = 0, nextMilestone = 250;
    bool modeTargetContact = false;
    double random(double lo, double hi);
    void activateModeTarget();
    void burst(Vec pos, int count, int color, double speed, ShotMode mode = ShotMode::Normal);
    void damage(Vec from, const char *reason, bool blockedByHeatShield = false, bool knockback = true);
    void die(const char *reason);
    void kill(Enemy &e, KillCause cause, ShotMode mode = ShotMode::Normal);

    void explode(Vec at, bool shotTriggered);
    void animateEffects(double dt);
  };
}
