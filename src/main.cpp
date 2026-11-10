#include "game.hpp"
#include "terminal.hpp"
#include <array>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>

namespace
{
  volatile std::sig_atomic_t running = 1;
  void stop(int) { running = 0; }
  using bb::Vec;
  struct RGB
  {
    int r, g, b;
    bool operator==(const RGB &) const = default;
  };
  constexpr RGB ink{-1, -1, -1}, gun{0, 0, 0}, modeIndicator{255, 255, 255};
    constexpr RGB fireYellow{255, 220, 35}, fireRed{255, 55, 20};
  constexpr double modeWarningRange = 120, warningTolerance = 1e-7;
  constexpr std::array<const char *, 6> titleArt{{
      R"(     / /  ______   ___    ____  __  __)",
      R"(    / /  /___  /  /   |  / __ \/ / / /)",
      R"(   / /___   / /  / /| | / /_/ / /_/ /)",
      R"(  /___  /  / /__/ ___ |/ ____/\__, /)",
      R"(     / /  /____/_/  |_/_/    /____/)",
      R"(    /_/)",
  }};
  struct Theme
  {
    const char *name;
    RGB platform, ammo, danger, player, repair, brute, enemy;
  };
  constexpr std::array<Theme, 10> themes{{{"reactor", {40, 255, 220}, {255, 215, 45}, {255, 65, 110}, {225, 255, 250}, {255, 30, 220}, {190, 85, 255}, {255, 65, 110}},
                                          {"synthwave", {0, 235, 255}, {255, 200, 50}, {255, 105, 65}, {255, 230, 255}, {255, 55, 240}, {75, 235, 255}, {255, 65, 180}},
                                          {"acid", {170, 255, 0}, {255, 230, 35}, {255, 55, 150}, {240, 255, 205}, {255, 30, 255}, {255, 140, 40}, {120, 85, 255}},
                                          {"glacier", {70, 180, 255}, {255, 215, 60}, {255, 85, 145}, {235, 250, 255}, {255, 60, 220}, {100, 150, 255}, {80, 225, 255}},
                                          {"inferno", {255, 120, 20}, {255, 225, 50}, {255, 45, 100}, {255, 245, 205}, {255, 40, 240}, {255, 235, 70}, {255, 130, 35}},
                                          {"sakura", {255, 105, 210}, {255, 210, 55}, {255, 90, 65}, {255, 235, 250}, {255, 25, 255}, {100, 180, 255}, {100, 245, 155}},
                                          {"ultraviolet", {170, 90, 255}, {255, 220, 40}, {255, 70, 130}, {245, 235, 255}, {255, 50, 215}, {255, 150, 55}, {75, 255, 210}},
                                          {"outrun", {255, 65, 170}, {255, 190, 35}, {255, 80, 65}, {255, 240, 250}, {255, 35, 245}, {65, 255, 210}, {85, 165, 255}},
                                          {"matrix", {30, 255, 100}, {255, 220, 45}, {255, 90, 110}, {220, 255, 230}, {255, 50, 235}, {255, 145, 50}, {165, 255, 45}},
                                          {"prism", {65, 210, 255}, {255, 230, 55}, {255, 65, 155}, {245, 250, 255}, {255, 45, 250}, {150, 110, 255}, {255, 145, 65}}}};
  // These channel bounds also keep gold ammo and neon-magenta health apart during fades.
  static_assert([]
                {
                  for (const auto &t : themes)
                    if (t.ammo.r != 255 || t.ammo.g < 190 || t.ammo.b > 60 ||
                        t.repair.r != 255 || t.repair.g > 60 || t.repair.b < 215)
                      return false;
                  return true;
                }(), "Every theme must keep ammo warm gold and health vivid magenta");
  RGB cyan, gold, red, white, muted, repair, brute, enemyColor, edge, shadow, lavaDark;
  RGB dim(RGB c, double factor) { return {int(c.r * factor), int(c.g * factor), int(c.b * factor)}; }
  // Palette indices used by the simulation for particles, rings and popups.
  RGB effectColor(int index)
  {
    switch (index)
    {
    case 1:
      return cyan;
    case 2:
      return red;
    case 4:
      return fireYellow;
    case 5:
      return fireRed;
    case 6:
      return gold;
    case 7:
      return white;
    default:
      return repair;
    }
  }
  constexpr uint32_t starHash(uint32_t x, uint32_t y)
  {
    uint32_t h = x * 374761393u + y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
  }
  RGB shotColor(bb::ShotMode mode)
  {
    switch (mode)
    {
    case bb::ShotMode::Normal:
      return {0, 240, 255};
    case bb::ShotMode::Longshot:
      return {45, 80, 255};
    default:
      return gold;
    }
  }
  struct ThemePhase
  {
    size_t current, next;
    double blend;
  };
  constexpr ThemePhase themePhase(size_t base, double altitude)
  {
    double height = std::max(0.0, altitude);
    size_t stage = static_cast<size_t>(height / 750);
    double progress = std::clamp((height - stage * 750.0 - 550) / 200, 0.0, 1.0);
    double smooth = progress * progress * (3 - 2 * progress);
    return {(base + stage) % themes.size(), (base + stage + 1) % themes.size(), smooth};
  }
  constexpr RGB mix(RGB a, RGB b, double t)
  {
    return {int(a.r + (b.r - a.r) * t), int(a.g + (b.g - a.g) * t), int(a.b + (b.b - a.b) * t)};
  }
  static_assert(themePhase(0, -1).blend == 0);
  static_assert(themePhase(0, 550).blend == 0);
  static_assert(themePhase(0, 650).blend == 0.5);
  static_assert(themePhase(0, 749).blend > 0.99);
  static_assert(themePhase(0, 750).current == 1 && themePhase(0, 750).blend == 0);
  static_assert(themePhase(0, 1300).blend == 0 && themePhase(0, 1400).blend == 0.5);
  static_assert(themePhase(0, 1500).current == 2 && themePhase(0, 1500).blend == 0);
  static_assert(themePhase(9, 750).current == 0 && themePhase(0, 7500).current == 0);
  static_assert(mix({0, 100, 200}, {100, 200, 0}, 0.5) == RGB{50, 150, 100});
  void applyTheme(size_t index, double altitude = 0)
  {
    auto phase = themePhase(index, altitude);
    const auto &a = themes[phase.current], &b = themes[phase.next];
    Theme t{a.name, mix(a.platform, b.platform, phase.blend), mix(a.ammo, b.ammo, phase.blend),
            mix(a.danger, b.danger, phase.blend), mix(a.player, b.player, phase.blend),
            mix(a.repair, b.repair, phase.blend), mix(a.brute, b.brute, phase.blend),
            mix(a.enemy, b.enemy, phase.blend)};
    cyan = t.platform;
    gold = t.ammo;
    red = t.danger;
    white = t.player;
    repair = t.repair;
    brute = t.brute;
    enemyColor = t.enemy;
    muted = dim(t.platform, 0.48);
    edge = dim(t.platform, 0.38);
    shadow = dim(t.platform, 0.24);
    lavaDark = dim(t.danger, 0.42);
  }
  struct Cell
  {
    RGB fg = white, bg = ink;
    std::string glyph = " ";
  };
  class Screen
  {
    int w, h, pixelH;
    std::vector<RGB> pixels;
    std::vector<Cell> cells;

  public:
    double bottom, scale;
    // Purely decorative layers (starfield, speed streaks, afterimages, record line). Off by
    // default so the gameplay layer can be rendered and verified on its own.
    bool decor = false;
    int bestHeight = 0;
    Screen(int columns, int rows, double camera) : w(columns), h(rows), pixelH(std::max(2, (rows - 10) * 2)),
                                                   pixels(static_cast<size_t>(std::max(1, columns - 4) * pixelH), ink), cells(static_cast<size_t>(columns * rows)),
                                                   bottom(camera - 8), scale(std::min((columns - 4) / bb::Game::width, pixelH / 52.0)) {}
    double inset() const { return ((w - 4) - bb::Game::width * scale) / 2; }
    Vec mouse(int x, int y) const { return {(x - 2 - inset()) / scale, bottom + (pixelH - (y - 3) * 2 - 1) / scale}; }
    void pixel(int x, int y, RGB color)
    {
      if (x >= 0 && x < w - 4 && y >= 0 && y < pixelH)
        pixels[static_cast<size_t>(y * (w - 4) + x)] = color;
    }
    std::pair<int, int> point(Vec p) const { return {static_cast<int>(std::lround(inset() + p.x * scale)), static_cast<int>(std::lround(pixelH - 1 - (p.y - bottom) * scale))}; }
    void dot(Vec p, double radius, RGB c)
    {
      auto [x, y] = point(p);
      int r = std::max(0, static_cast<int>(radius * scale));
      for (int yy = -r; yy <= r; ++yy)
        for (int xx = -r; xx <= r; ++xx)
          if (xx * xx + yy * yy <= r * r + 1)
            pixel(x + xx, y + yy, c);
    }
    void line(Vec a, Vec b, RGB c)
    {
      auto [x, y] = point(a);
      auto [xx, yy] = point(b);
      int steps = std::max(std::abs(xx - x), std::abs(yy - y));
      // Bound work for off-screen objects and tiny terminal sizes.
      steps = std::min(steps, 2000);
      for (int i = 0; i <= steps; ++i)
      {
        double t = steps ? double(i) / steps : 0;
        pixel(static_cast<int>(x + (xx - x) * t), static_cast<int>(y + (yy - y) * t), c);
      }
    }
    void circle(Vec center, double radius, RGB c)
    {
      int steps = std::clamp(static_cast<int>(radius * scale * 7), 12, 400);
      for (int i = 0; i < steps; ++i)
      {
        double a = i * 6.283185307179586 / steps;
        auto [x, y] = point(center + Vec{std::cos(a), std::sin(a)} * radius);
        pixel(x, y, c);
      }
    }
    void backdrop(const bb::Game &g)
    {
      // Only a fast climb drags the camera, so only then do the stars smear into streaks.
      const int smear = g.dead ? 0 : std::clamp(static_cast<int>((g.player.vel.y - 38) / 7), 0, 4);
      for (int layer = 0; layer < 2; ++layer)
      {
        const int scroll = static_cast<int>(std::floor(bottom * scale * (layer ? 0.55 : 0.25)));
        const uint32_t rarity = layer ? 330 : 150;
        const RGB color = dim(cyan, layer ? 0.34 : 0.2);
        for (int y = 0; y < pixelH; ++y)
          for (int x = 0; x < w - 4; ++x)
          {
            RGB &at = pixels[static_cast<size_t>(y * (w - 4) + x)];
            if (!(at == ink))
              continue;
            for (int k = 0; k <= smear; ++k)
              if (starHash(static_cast<uint32_t>(x + layer * 7919), static_cast<uint32_t>(y + k - scroll)) % rarity == 0)
              {
                at = dim(color, 1.0 - k / (smear + 1.5));
                break;
              }
          }
      }
      if (bestHeight > 0)
      {
        auto [x0, y] = point({0, bestHeight + 3.0});
        int x1 = point({bb::Game::width, 0}).first;
        for (int x = x0 + 1; x < x1; ++x)
          if (x % 4 < 2)
            pixel(x, y, dim(gold, 0.45));
      }
    }
    void text(int x, int y, const std::string &s, RGB fg = white, RGB bg = ink)
    {
      if (y < 0 || y >= h)
        return;
      for (char ch : s)
      {
        if (x >= 0 && x < w)
          cells[static_cast<size_t>(y * w + x)] = {fg, bg, std::string(1, ch)};
        ++x;
      }
    }
    void centered(int y, const std::string &s, RGB fg = white) { text((w - static_cast<int>(s.size())) / 2, y, s, fg); }
    void block(int x, int y, RGB color)
    {
      if (x >= 0 && x < w && y >= 0 && y < h)
        cells[static_cast<size_t>(y * w + x)] = {color, color, "▀"};
    }
    void bigNumber(int x, int y, int value, RGB color)
    {
      static constexpr std::array<std::array<const char *, 5>, 10> digits{{{{"###", "# #", "# #", "# #", "###"}},
                                                                           {{" # ", "## ", " # ", " # ", "###"}},
                                                                           {{"###", "  #", "###", "#  ", "###"}},
                                                                           {{"###", "  #", "###", "  #", "###"}},
                                                                           {{"# #", "# #", "###", "  #", "  #"}},
                                                                           {{"###", "#  ", "###", "  #", "###"}},
                                                                           {{"###", "#  ", "###", "# #", "###"}},
                                                                           {{"###", "  #", "  #", "  #", "  #"}},
                                                                           {{"###", "# #", "###", "# #", "###"}},
                                                                           {{"###", "# #", "###", "  #", "###"}}}};
      for (char digit : std::to_string(value))
      {
        for (int row = 0; row < 5; ++row)
          for (int col = 0; col < 3; ++col)
            if (digits[static_cast<size_t>(digit - '0')][static_cast<size_t>(row)][col] == '#')
              block(x + col, y + row, color);
        x += 4;
      }
    }
    void hud(const bb::Game &g, int best, int bestStreak = 0)
    {
      int y = h - 6;
      text(2, y, "HP", repair);
      for (int i = 0; i < 3; ++i)
      {
        RGB color = i < g.player.hp ? repair : muted;
        for (int row = 1; row <= 3; ++row)
          for (int col = 0; col < 5; ++col)
            if (row == 2 || col == 2)
              block(2 + i * 6 + col, y + row, color);
      }
      int right = w - 26;
      text(right, y, "AMMO", gold);
      text(24, y, "MODE", cyan);
      text(24, y + 1, g.shotModeName(), shotColor(g.shotMode));
      for (int i = 0; i < bb::Player::maxBonusShells; ++i)
        for (int row = 1; row <= 3; ++row)
          for (int col = 0; col < 3; ++col)
            block(right + i * 4 + col, y + row, i < g.player.shells ? gold : muted);
      int progress = g.player.shells >= bb::Player::maxShells ? 12 : std::clamp(static_cast<int>(g.player.reload / bb::Player::reloadSeconds * 12), 0, 12);
      text(right, y + 4, (g.player.shells >= bb::Player::maxShells ? "READY  [" : "RELOAD [") + std::string(static_cast<size_t>(progress), '=') + std::string(static_cast<size_t>(12 - progress), '.') + "]", gold);
      centered(0, "Zapy", cyan);
      if (g.debugUsed)
        text(2, 0, "UNRANKED", muted);
      text(2, 1, "BEST " + std::to_string(best) + " m", gold);
      centered(1, "HEIGHT " + std::to_string(g.score()) + " m", white);
      text(w - 24, 1, "SECTOR " + std::to_string(g.sector()) + " / KILLS " + std::to_string(g.kills), muted);
      text(2, 2, std::string(static_cast<size_t>(w - 4), '-'), muted);
      std::string streak = std::to_string(g.combo);
      std::string streakRecord = "BEST " + std::to_string(bestStreak);
      int streakWidth = std::max({9, static_cast<int>(streak.size()) * 4 - 1, static_cast<int>(streakRecord.size())});
      int streakX = w - streakWidth - 3;
      int streakY = 4;
      for (int row = 0; row < 9; ++row)
        text(streakX - 1, streakY + row, std::string(static_cast<size_t>(streakWidth + 2), ' '));
      text(streakX, streakY, "STREAK", cyan);
      text(streakX, streakY + 1, streakRecord, gold);
      bigNumber(streakX, streakY + 2, g.combo, g.combo > 0 ? gold : muted);
      int remaining = std::clamp(static_cast<int>(g.comboTime / 2.0 * streakWidth), 0, streakWidth);
      text(streakX, streakY + 8, std::string(static_cast<size_t>(remaining), '=') + std::string(static_cast<size_t>(streakWidth - remaining), '.'), cyan);

      double speed = g.player.vel.length();
      int speedValue = static_cast<int>(std::lround(speed));
      int speedWidth = std::max(11, static_cast<int>(std::to_string(speedValue).size()) * 4 - 1);
      bool overcap = speed > bb::Game::softSpeedThreshold;
      RGB speedColor = overcap ? fireRed : cyan;
      for (int row = 0; row < 9; ++row)
        text(2, 4 + row, std::string(static_cast<size_t>(speedWidth + 2), ' '));
      text(3, 4, "VELOCITY", cyan);
      text(3, 5, "SOFT " + std::to_string(static_cast<int>(bb::Game::softSpeedThreshold)), fireYellow);
      bigNumber(3, 6, speedValue, speed > 0 ? speedColor : muted);
      text(3, 11, overcap ? "OVERCAP" : "u/s", speedColor);
      int filled = std::clamp(static_cast<int>(speed / bb::Game::softSpeedThreshold * (speedWidth - 2)), 0, speedWidth - 2);
      text(3, 12, std::string(static_cast<size_t>(filled), '=') + std::string(static_cast<size_t>(speedWidth - 2 - filled), '.'), speedColor);
      text(3 + speedWidth - 2, 12, "|", fireYellow);
      text(3 + speedWidth - 1, 12, overcap ? "!" : ".", overcap ? fireRed : muted);
    }
    void warning(Vec pos, double radius, RGB color, int size = 1, double range = 30)
    {
      double top = bottom + (pixelH - 1) / scale;
      double gap = pos.y - radius - top;
      if (gap <= warningTolerance || gap > range + warningTolerance)
        return;
      auto [x, y] = point(pos);
      (void)y;
      for (int row = 0; row <= size; ++row)
        for (int offset = row - size; offset <= size - row; ++offset)
          pixel(x + offset, row, color);
    }
    void world(const bb::Game &g, Vec aim)
    {
      for (int y = 0; y < pixelH; ++y)
        for (int x = 0; x < w - 4; ++x)
        {
          double wy = bottom + (pixelH - y) / scale;
          RGB c = ink;
          if (wy < g.lava)
            c = (x + static_cast<int>(wy * 3 + g.time * 12)) % 7 < 3 ? red : lavaDark;
          pixel(x, y, c);
        }
      if (decor)
        backdrop(g);
      line({0, bottom}, {0, bottom + pixelH / scale}, edge);
      line({bb::Game::width, bottom}, {bb::Game::width, bottom + pixelH / scale}, edge);
      for (const auto &p : g.platforms)
      {
        if (p.y < bottom - 2 || p.y > bottom + pixelH / scale + 2)
          continue;
        line({p.x, p.y - 0.7}, {p.x + p.width, p.y - 0.7}, shadow);
        line({p.x, p.y}, {p.x + p.width, p.y}, p.spikes ? red : cyan);
        if (p.spikes)
          for (double x = p.x; x < p.x + p.width; x += 2)
            dot({x, p.y + 0.7}, 0, red);
      }
      if (g.modeTarget.y + 9 >= bottom && g.modeTarget.y - 9 <= bottom + pixelH / scale)
      {
        double pulse = 0.5 + 0.5 * std::sin(g.time * 5);
        RGB glow = g.modeTargetCooldown > 0 ? muted : cyan;
        RGB halo{int(glow.r * (0.18 + pulse * 0.12)), int(glow.g * (0.18 + pulse * 0.12)), int(glow.b * (0.18 + pulse * 0.12))};
        dot(g.modeTarget, bb::Game::modeTargetRadius + 2 + pulse, halo);
        dot(g.modeTarget, bb::Game::modeTargetRadius, glow);
        dot(g.modeTarget, bb::Game::modeTargetRadius - 1.5, shadow);
        dot(g.modeTarget, 2 + pulse * 0.5, g.modeTargetCooldown > 0 ? muted : white);
      }
      for (const auto &p : g.pickups)
      {
        RGB c = p.type == bb::Pickup::repair ? repair : gold;
        Vec pos = p.pos + Vec{0, std::sin(g.time * 5 + p.pos.x) * 0.35};

        line(pos - Vec{1, 0}, pos + Vec{1, 0}, c);
        line(pos - Vec{0, 1}, pos + Vec{0, 1}, c);
      }
      for (const auto &e : g.enemies)
      {
        RGB body = e.type == 2 ? brute : enemyColor;
        if (e.type == bb::Enemy::volatileDrone)
          body = e.fuse > 0 ? white : static_cast<int>(g.clock() * 6 + e.phase) % 2 ? fireYellow : fireRed;
        if (e.flash > 0)
          body = white;
        dot(e.pos, e.radius, body);
        dot(e.pos + Vec{-0.4, 0.3}, 0, e.type == bb::Enemy::volatileDrone || e.flash > 0 ? gun : white);
        if (e.type == 2)
          line(e.pos - Vec{1.5, 2.8}, e.pos + Vec{-1.5 + 3 * e.hp / bb::Enemy::bruteHp, -2.8}, brute);
      }
      for (const auto &b : g.bullets)
        line(b.pos - b.vel * 0.016, b.pos, b.hostile ? enemyColor : shotColor(b.shotMode));
      for (const auto &p : g.particles)
      {
        RGB c = p.color == 0 ? shotColor(p.shotMode) : effectColor(p.color);
        double fade = p.life / p.total;
        c = {int(c.r * fade), int(c.g * fade), int(c.b * fade)};
        dot(p.pos, 0, c);
      }
      for (const auto &r : g.rings)
      {
        // Ease out: the shockwave snaps open, then lingers as it fades.
        double age = 1 - r.life / r.total;
        circle(r.pos, r.radius * (1 - (1 - age) * (1 - age)), dim(effectColor(r.color), 1 - age * 0.75));
      }
      Vec dir = (aim - g.player.pos).unit();
      RGB energy = shotColor(g.shotMode);
      if (!g.dead)
        for (int i = 3; i <= 10; i += 2)
          dot(g.player.pos + dir * double(i), 0, dim(energy, 0.38));

      if (!g.dead && (g.player.invulnerable <= 0 || static_cast<int>(g.time * 18) % 2 == 0))
      {
        dot(g.player.pos, 1.1, white);
        line(g.player.pos, g.player.pos + dir * 2.6, gun);
        auto [tipX, tipY] = point(g.player.pos + dir * 2.6);
        pixel(tipX, tipY, energy);
        if (g.flash > 0)
          dot(g.player.pos + dir * 3.1, 0, shotColor(g.flashMode));
        dot(g.player.pos + Vec{-0.25, 0.3}, 0, cyan);
      }
      if (g.heatShieldActive())
      {
        auto [bodyX, bodyY] = point(g.player.pos);
        int radius = static_cast<int>(1.1 * scale) + 1;
        int phase = static_cast<int>(g.time * 10);
        int top = bodyY - radius;
        int cycle = radius * 2 + 2;
        for (int x = -radius; x <= radius; ++x)
          pixel(bodyX + x, top, (x + phase) % 2 ? fireRed : fireYellow);
        // One-pixel-wide streams spill from the top and drip down opposite sides.
        for (int side : {-1, 1})
        {
          int drop = 1 + (phase + (side > 0 ? cycle / 2 : 0)) % cycle;
          pixel(bodyX + side * radius, top + drop - 1, fireRed);
          pixel(bodyX + side * radius, top + drop, fireYellow);
        }
      }
      line(aim - Vec{1.2, 0}, aim + Vec{1.2, 0}, energy);
      line(aim - Vec{0, 1.2}, aim + Vec{0, 1.2}, energy);
      // Use the same mapping for nearby, fully off-screen targets and enemies.
      double top = bottom + (pixelH - 1) / scale;
      double modeGap = g.modeTarget.y - bb::Game::modeTargetRadius - top;
      int modeSize = modeGap > modeWarningRange * 0.75 + warningTolerance ? 0 : modeGap > modeWarningRange * 0.5 + warningTolerance ? 1 : modeGap > modeWarningRange * 0.25 + warningTolerance ? 2 : 3;
      warning(g.modeTarget, bb::Game::modeTargetRadius, g.modeTargetCooldown > 0 ? dim(modeIndicator, 0.48) : modeIndicator, modeSize, modeWarningRange);
      for (const auto &e : g.enemies)
        if (e.hp > 0)
          warning(e.pos, e.radius, e.type == 2 ? brute : enemyColor);
      for (int y = 0; y < pixelH / 2; ++y)
        for (int x = 0; x < w - 4; ++x)
        {
          auto at = static_cast<size_t>((y + 3) * w + x + 2);
          RGB top = pixels[static_cast<size_t>(y * 2 * (w - 4) + x)];
          RGB bottomPixel = pixels[static_cast<size_t>((y * 2 + 1) * (w - 4) + x)];
          if (top == ink)
            cells[at] = {bottomPixel == ink ? white : bottomPixel, ink, bottomPixel == ink ? " " : "▄"};
          else
            cells[at] = {top, bottomPixel, "▀"};
        }
      // The frame flinches when hull is lost.
      RGB border = g.hurtFlash > 0 ? red : muted;
      for (int y = 3; y < h - 7; ++y)
      {
        text(1, y, "|", border);
        text(w - 2, y, "|", border);
      }
    }
    // Text drawn over the playfield: floating labels, announcements and the meltdown alarm.
    void overlay(const bb::Game &g)
    {
      for (const auto &p : g.popups)
      {
        auto [px, py] = point(p.pos);
        int row = py / 2 + 3, col = px + 2 - static_cast<int>(p.text.size()) / 2;
        if (py < 0 || row < 3 || row >= h - 7)
          continue;
        col = std::clamp(col, 2, std::max(2, w - 2 - static_cast<int>(p.text.size())));
        text(col, row, p.text, dim(effectColor(p.color), 0.45 + 0.55 * p.life / p.total));
      }
      if (g.bannerTime > 0 && !g.banner.empty())
        centered(5, "-=  " + g.banner + "  =-", g.bannerTime > 0.3 || static_cast<int>(g.clock() * 20) % 2 ? white : muted);
      if (!g.dead && g.time > 4 && g.player.pos.y - g.lava < 12 && static_cast<int>(g.time * 6) % 2 == 0)
        centered(h - 8, "!! MELTDOWN RISING !!", red);
    }
    void panel(int y, const std::vector<std::string> &lines)
    {
      int box = 58, x = (w - box) / 2;
      for (int row = 0; row < static_cast<int>(lines.size()) + 2; ++row)
        text(x, y + row, std::string(box, ' '), white, ink);
      for (size_t i = 0; i < lines.size(); ++i)
        centered(y + 1 + static_cast<int>(i), lines[i], i == 0 ? cyan : white);
    }
    void intro()
    {
      std::vector<std::string> lines(titleArt.size());
      lines.insert(lines.end(), {"Zapy // HIGH VOLTAGE. NO BRAKES.", "",
                                 "Aim DOWN. Hold LEFT CLICK. Fly UP.",
                                 "Side shots bounce you off the walls.",
                                 "Kills +3 ammo. Landings +2. Brutes always heal.", "",
                                 "CLICK TO DROP IN  /  ENTER TO START"});
      int y = std::max(3, (h - static_cast<int>(lines.size()) - 2) / 2);
      panel(y, lines);
      size_t titleWidth = 0;
      for (const auto *line : titleArt)
        titleWidth = std::max(titleWidth, std::string(line).size());
      int x = (w - static_cast<int>(titleWidth)) / 2;
      for (size_t i = 0; i < titleArt.size(); ++i)
        text(x, y + 1 + static_cast<int>(i), titleArt[i], i % 2 == 0 ? shotColor(bb::ShotMode::Normal) : modeIndicator);
    }
    std::string frame() const
    {
      std::string out = "\033[H\033[1m";
      out.reserve(cells.size() * 10);
      RGB fg{-2, -2, -2}, bg = fg;
      for (int y = 0; y < h; ++y)
      {
        for (int x = 0; x < w; ++x)
        {
          const auto &c = cells[static_cast<size_t>(y * w + x)];
          if (!(fg == c.fg))
          {
            fg = c.fg;
            out += "\033[38;2;" + std::to_string(fg.r) + ";" + std::to_string(fg.g) + ";" + std::to_string(fg.b) + "m";
          }
          if (!(bg == c.bg))
          {
            bg = c.bg;
            out += bg == ink ? "\033[49m" : "\033[48;2;" + std::to_string(bg.r) + ";" + std::to_string(bg.g) + ";" + std::to_string(bg.b) + "m";
          }
          out += c.glyph;
        }
        if (y < h - 1)
          out += "\r\n";
      }
      return out + "\033[0m";
    }
  };
  std::filesystem::path statePath(const char *app)
  {
    if (const char *x = std::getenv("XDG_STATE_HOME"); x && *x)
      return std::filesystem::path(x) / app;
    if (const char *h = std::getenv("HOME"); h && *h)
      return std::filesystem::path(h) / ".local/state" / app;
    return {};
  }
  std::filesystem::path scorePath()
  {
    auto state = statePath("zapy");
    return state.empty() ? std::filesystem::path{} : state / "best-height";
  }
  std::ifstream loadState(const char *name)
  {
    auto state = statePath("zapy");
    if (state.empty())
      return {};
    std::ifstream file(state / name);
    // Keep existing height records and styles available after the rename.
    if (!file.is_open())
      file.open(statePath("backblast") / name);
    return file;
  }
  int loadBest(const char *name = "best-height")
  {
    int n = 0;
    auto f = loadState(name);
    f >> n;
    return std::max(0, n);
  }
  std::filesystem::path themePath()
  {
    auto score = scorePath();
    return score.empty() ? std::filesystem::path{} : score.parent_path() / "theme";
  }
  size_t themeIndex(const std::string &name)
  {
    for (size_t i = 0; i < themes.size(); ++i)
      if (name == themes[i].name)
        return i;
    throw std::runtime_error("Unknown theme: " + name + " (use --list-themes)");
  }
  size_t loadTheme()
  {
    std::string name;
    auto f = loadState("theme");
    f >> name;
    try
    {
      return themeIndex(name);
    }
    catch (const std::exception &)
    {
      return 0;
    }
  }
  void saveTheme(size_t index)
  {
    auto path = themePath();
    if (path.empty())
      return;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (!ec)
    {
      std::ofstream f(path);
      f << themes[index].name << '\n';
    }
  }
  void saveBest(int n, const char *name = "best-height")
  {
    auto p = scorePath();
    if (p.empty())
      return;
    p = p.parent_path() / name;
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (ec)
      return;
    auto temp = p;
    temp += ".tmp";
    {
      std::ofstream f(temp);
      if (!(f << n << '\n'))
        return;
    }
    std::filesystem::rename(temp, p, ec);
  }
  class RunRecords
  {
  public:
    int best = loadBest(), bestStreak = loadBest("best-streak");
    void beginRun()
    {
      runBest = best;
      runBestStreak = bestStreak;
      disqualified = false;
    }
    void update(const bb::Game &game)
    {
      if (game.debugUsed)
      {
        if (!disqualified)
        {
          best = runBest;
          // Withdraw streak records already saved earlier in this now-unranked run.
          if (bestStreak > runBestStreak)
            saveBest(runBestStreak, "best-streak");
          bestStreak = runBestStreak;
          disqualified = true;
        }
        return;
      }
      if (disqualified)
        return;
      best = std::max(best, game.score());
      if (game.combo > bestStreak)
      {
        bestStreak = game.combo;
        saveBest(bestStreak, "best-streak");
      }
      if (game.dead)
        finish();
    }
    void finish()
    {
      if (best > savedBest)
      {
        saveBest(best);
        savedBest = best;
      }
    }

  private:
    int savedBest = best, runBest = best, runBestStreak = bestStreak;
    bool disqualified = false;
  };
}

int main(int argc, char **argv)
{
  try
  {
    uint32_t seed = std::random_device{}();
    size_t theme = loadTheme();
    for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i];
      if (a == "--help" || a == "-h")
      {
        for (const auto *line : titleArt)
          std::cout << line << '\n';
        std::cout << "\nZapy - endless recoil-only terminal platformer\n\n"
                  << "Aim with mouse. Hold left click to fire. Aim DOWN to fly UP.\n"
                  << "P: pause   R: restart   Q / Ctrl-C: quit   Enter: start\n"
                  << "Touch glowing circles to change gun mode: first at 750-1000m, then every 1000-1500m.\n"
                  << "Reload 1 shell per 0.75s up to 4; kills add 3 up to 6.\n"
                  << "Safe landings add 2 ammo up to 4, preserving existing bonus ammo.\n"
                  << "Firing resets reload progress. Platforms gradually slow sideways motion.\n"
                  << "Brute kills always heal 1 HP; Scatter has a 10% chance to heal 2 HP.\n"
                  << "Flashing volatiles explode: the blast kills neighbours and throws you clear.\n"
                  << "Themes advance every 750m, fading over the preceding 200m.\n"
                  << "T / ]: next starting style   [: previous style (saved automatically)\n"
                  << "Every retry generates a new course; --seed sets only the first course.\n"
                  << "Usage: zapy [--seed NUMBER] [--theme NAME] [--list-themes]\n"
                  << "Requires a mouse-capable UTF-8 terminal, at least 72 x 24.\n";
        return 0;
      }
      if (a == "--list-themes")
      {
        for (const auto &t : themes)
          std::cout << t.name << '\n';
        return 0;
      }
      if (a == "--theme" && i + 1 < argc)
        theme = themeIndex(argv[++i]);
      else if (a == "--seed" && i + 1 < argc)
      {
        std::string value = argv[++i];
        size_t used = 0;
        auto number = std::stoul(value, &used);
        if (used != value.size() || number > UINT32_MAX)
          throw std::runtime_error("Seed must be a 32-bit unsigned number");
        seed = static_cast<uint32_t>(number);
      }
      else
        throw std::runtime_error("Unknown argument: " + a + " (use --help)");
    }
    applyTheme(theme);
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    std::signal(SIGHUP, stop);
    bb::Terminal terminal;
    bb::Game game(seed);
    RunRecords records;
    bool started = false, paused = false, held = false;
    // The record to beat is fixed when a run begins, so the line in the world stays put.
    int recordToBeat = records.best;
    int mx = terminal.columns / 2, my = terminal.rows - 4;
    using Clock = std::chrono::steady_clock;
    auto last = Clock::now();
    double accumulator = 0;
    constexpr double step = 1.0 / 120;
    while (running)
    {
      auto frameStart = Clock::now();
      double elapsed = std::chrono::duration<double>(frameStart - last).count();
      last = frameStart;
      terminal.resize();
      bool small = terminal.columns < 72 || terminal.rows < 24;
      Screen mapping(std::max(72, terminal.columns), std::max(24, terminal.rows), game.camera);
      for (const auto &e : terminal.input())
      {
        if (e.kind == bb::Input::Mouse)
        {
          mx = e.x;
          my = e.y;
          if (e.release)
            held = false;
          if (e.down && !small && !paused)
          {
            started = true;
            held = true;
          }
        }
        else
        {
          int key = e.key;
          if (key == 'q' || key == 'Q' || key == 3)
            running = 0;
          if (key == 't' || key == 'T' || key == ']' || key == '[')
          {
            theme = (theme + (key == '[' ? themes.size() - 1 : 1)) % themes.size();
            applyTheme(theme);
            saveTheme(theme);
          }
          if (key == 'x' || key == 'X')
          {
            game.debugSwitchMode();
            records.update(game);
          }
          if (key == 'p' || key == 'P')
          {
            paused = !paused;
            held = false;
          }
          if (key == 'r' || key == 'R' || ((key == '\r' || key == '\n') && game.dead))
          {
            records.finish();
            game.reset();
            records.beginRun();
            recordToBeat = records.best;
            started = true;
            paused = false;
            held = false;
            accumulator = 0;
          }
          if (key == '\r' || key == '\n')
            started = true;
        }
      }
      if (started && !paused && !small && !game.dead)
      {
        if (game.hitStop > 0)
        {
          // Freeze-frame on impact. Capped so chained hits can never stall the run.
          game.hitStop = std::min(game.hitStop, 0.16) - elapsed;
          accumulator = 0;
        }
        else
        {
          accumulator += std::min(elapsed, 0.1);
          while (accumulator >= step && !game.dead && game.hitStop <= 0)
          {
            if (held)
              game.shoot(mapping.mouse(mx, my));
            game.update(step);
            accumulator -= step;
          }
        }
      }
      else
      {
        accumulator = 0;
        if (small || game.dead)
          held = false;
        // Keep the wreckage flying behind the results panel.
        if (game.dead && !paused && !small)
          game.update(std::min(elapsed, 1.0 / 30));
      }
      records.update(game);
      applyTheme(theme, game.altitude);
      Screen screen(std::max(6, terminal.columns), std::max(8, terminal.rows), game.camera);
      if (small)
      {
        screen.text(0, 1, "Zapy", cyan);
        screen.text(0, 3, "Resize terminal to at least 72 x 24.");
        screen.text(0, 5, "Game frozen. Q to quit.", gold);
      }
      else
      {
        Vec aim = mapping.mouse(mx, my);
        // Small, short-lived camera kick, never applied to physics or mouse aiming.
        if (game.shake > 0)
          screen.bottom += std::sin(game.clock() * 130) * game.shake * 0.8;
        screen.decor = true;
        screen.bestHeight = recordToBeat;
        screen.world(game, aim);
        screen.hud(game, records.best, records.bestStreak);
        if (started)
          screen.overlay(game);
        auto phase = themePhase(theme, game.altitude);
        std::string style = themes[phase.current].name;
        if (phase.blend > 0)
          style += ">" + std::string(themes[phase.next].name);
        screen.text(2, terminal.rows - 1, "LMB fire  P pause  R retry  Q quit  T STYLE " + style, muted);
        if (!started)
          screen.intro();
        else if (game.dead)
        {
          // Let the explosion land before the results cover it.
          if (game.deathTime > 0.45)
            screen.panel(terminal.rows / 2 - 6, {!game.debugUsed && game.score() > recordToBeat ? "N E W  R E C O R D" : "R U N  O V E R", game.deathReason, "",
                                                 "HEIGHT " + std::to_string(game.score()) + " METERS",
                                                 std::to_string(game.kills) + " HUNTERS DOWN  /  BEST " + std::to_string(records.best) + " m",
                                                 "STREAK " + std::to_string(game.bestCombo) + " / TOP SPEED " + std::to_string(static_cast<int>(game.topSpeed)),
                                                 "",
                                                 "R / ENTER: instant retry    Q: leave"});
        }
        else if (paused)
          screen.panel(terminal.rows / 2 - 2, {"P A U S E D", "P: resume   R: retry   Q: quit"});
        else if (game.time < 5)
          screen.centered(terminal.rows - 7, "AIM BELOW YOURSELF + HOLD LEFT CLICK TO CLIMB", gold);
      }
      terminal.write(screen.frame());
      std::this_thread::sleep_until(frameStart + std::chrono::microseconds(16667));
    }
    records.finish();
  }
  catch (const std::exception &e)
  {
    std::cerr << "Zapy: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
