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
  constexpr RGB ink{-1, -1, -1}, cyan{79, 244, 221}, gold{255, 206, 98}, red{255, 78, 111}, white{231, 242, 255}, muted{92, 115, 147};
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
      static constexpr std::array<std::array<const char *, 5>, 10> digits{{
          {{"###", "# #", "# #", "# #", "###"}},
          {{" # ", "## ", " # ", " # ", "###"}},
          {{"###", "  #", "###", "#  ", "###"}},
          {{"###", "  #", "###", "  #", "###"}},
          {{"# #", "# #", "###", "  #", "  #"}},
          {{"###", "#  ", "###", "  #", "###"}},
          {{"###", "#  ", "###", "# #", "###"}},
          {{"###", "  #", "  #", "  #", "  #"}},
          {{"###", "# #", "###", "# #", "###"}},
          {{"###", "# #", "###", "  #", "###"}}
      }};
      for (char digit : std::to_string(value))
      {
        for (int row = 0; row < 5; ++row)
          for (int col = 0; col < 3; ++col)
            if (digits[static_cast<size_t>(digit - '0')][static_cast<size_t>(row)][col] == '#')
              block(x + col, y + row, color);
        x += 4;
      }
    }
    void hud(const bb::Game &g, int best)
    {
      int y = h - 6;
      text(2, y, "HP", red);
      for (int i = 0; i < 3; ++i)
      {
        RGB color = i < g.player.hp ? red : muted;
        for (int row = 1; row <= 3; ++row)
          for (int col = 0; col < 5; ++col)
            if (row == 2 || col == 2)
              block(2 + i * 6 + col, y + row, color);
      }
      int right = w - 26;
      text(right, y, "AMMO", gold);
      for (int i = 0; i < bb::Player::maxBonusShells; ++i)
        for (int row = 1; row <= 3; ++row)
          for (int col = 0; col < 3; ++col)
            block(right + i * 4 + col, y + row, i < g.player.shells ? gold : muted);
      int progress = g.player.shells >= bb::Player::maxShells ? 12 :
          std::clamp(static_cast<int>(g.player.reload / bb::Player::reloadSeconds * 12), 0, 12);
      text(right, y + 4, (g.player.shells >= bb::Player::maxShells ? "READY  [" : "RELOAD [") +
          std::string(static_cast<size_t>(progress), '=') + std::string(static_cast<size_t>(12 - progress), '.') + "]", gold);
      centered(0, "BACKBLAST", cyan);
      text(2, 1, "BEST " + std::to_string(best) + " m", gold);
      centered(1, "HEIGHT " + std::to_string(g.score()) + " m", white);
      text(w - 24, 1, "SECTOR " + std::to_string(g.sector()) + " / KILLS " + std::to_string(g.kills), muted);
      text(2, 2, std::string(static_cast<size_t>(w - 4), '-'), muted);
      std::string streak = std::to_string(g.combo);
      int streakWidth = std::max(9, static_cast<int>(streak.size()) * 4 - 1);
      int streakX = w - streakWidth - 3;
      int streakY = 4;
      for (int row = 0; row < 9; ++row)
        text(streakX - 1, streakY + row, std::string(static_cast<size_t>(streakWidth + 2), ' '));
      text(streakX, streakY, "STREAK", cyan);
      bigNumber(streakX, streakY + 2, g.combo, g.combo > 0 ? gold : muted);
      int remaining = std::clamp(static_cast<int>(g.comboTime / 2.0 * streakWidth), 0, streakWidth);
      text(streakX, streakY + 8, std::string(static_cast<size_t>(remaining), '=') +
          std::string(static_cast<size_t>(streakWidth - remaining), '.'), cyan);
    }
    void world(const bb::Game &g, Vec aim)
    {
      for (int y = 0; y < pixelH; ++y)
        for (int x = 0; x < w - 4; ++x)
        {
          double wy = bottom + (pixelH - y) / scale;
          RGB c = ink;
          if (wy < g.lava)
            c = (x + static_cast<int>(wy * 3 + g.time * 12)) % 7 < 3 ? red : RGB{119, 31, 66};
          pixel(x, y, c);
        }
      line({0, bottom}, {0, bottom + pixelH / scale}, {42, 80, 91});
      line({bb::Game::width, bottom}, {bb::Game::width, bottom + pixelH / scale}, {42, 80, 91});
      for (const auto &p : g.platforms)
      {
        if (p.y < bottom - 2 || p.y > bottom + pixelH / scale + 2)
          continue;
        line({p.x, p.y - 0.7}, {p.x + p.width, p.y - 0.7}, {35, 65, 82});
        line({p.x, p.y}, {p.x + p.width, p.y}, p.spikes ? red : cyan);
        if (p.spikes)
          for (double x = p.x; x < p.x + p.width; x += 2)
            dot({x, p.y + 0.7}, 0, red);
      }
      for (const auto &p : g.pickups)
      {
        RGB c = p.type ? RGB{111, 255, 148} : gold;
        Vec pos = p.pos + Vec{0, std::sin(g.time * 5 + p.pos.x) * 0.35};
        line(pos - Vec{1, 0}, pos + Vec{1, 0}, c);
        line(pos - Vec{0, 1}, pos + Vec{0, 1}, c);
      }
      for (const auto &e : g.enemies)
      {
        dot(e.pos, e.radius, e.type == 2 ? RGB{163, 83, 255} : red);
        dot(e.pos + Vec{-0.4, 0.3}, 0, white);
        if (e.type == 2)
          line(e.pos - Vec{1.5, 2.8}, e.pos + Vec{-1.5 + 3 * e.hp / bb::Enemy::bruteHp, -2.8}, red);
      }
      for (const auto &b : g.bullets)
        line(b.pos - b.vel * 0.016, b.pos, b.hostile ? red : gold);
      for (const auto &p : g.particles)
      {
        RGB c = p.color == 0 ? gold : p.color == 1 ? cyan
                                  : p.color == 2   ? red
                                                   : RGB{111, 255, 148};
        double fade = p.life / p.total;
        c = {int(c.r * fade), int(c.g * fade), int(c.b * fade)};
        dot(p.pos, 0, c);
      }
      Vec dir = (aim - g.player.pos).unit();
      for (int i = 3; i <= 10; i += 2)
        dot(g.player.pos + dir * double(i), 0, {42, 80, 91});
      if (g.player.invulnerable <= 0 || static_cast<int>(g.time * 18) % 2 == 0)
      {
        dot(g.player.pos, 1.1, white);
        line(g.player.pos, g.player.pos + dir * 2.6, g.flash > 0 ? gold : cyan);
        dot(g.player.pos + Vec{-0.25, 0.3}, 0, cyan);
      }
      line(aim - Vec{1.2, 0}, aim + Vec{1.2, 0}, gold);
      line(aim - Vec{0, 1.2}, aim + Vec{0, 1.2}, gold);
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
      for (int y = 3; y < h - 7; ++y)
      {
        text(1, y, "|", muted);
        text(w - 2, y, "|", muted);
      }
    }
    void panel(int y, const std::vector<std::string> &lines)
    {
      int box = 58, x = (w - box) / 2;
      for (int row = 0; row < static_cast<int>(lines.size()) + 2; ++row)
        text(x, y + row, std::string(box, ' '), white, ink);
      for (size_t i = 0; i < lines.size(); ++i)
        centered(y + 1 + static_cast<int>(i), lines[i], i == 0 ? cyan : white);
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
  std::filesystem::path scorePath()
  {
    if (const char *x = std::getenv("XDG_STATE_HOME"); x && *x)
      return std::filesystem::path(x) / "backblast" / "best-height";
    if (const char *h = std::getenv("HOME"); h && *h)
      return std::filesystem::path(h) / ".local/state/backblast/best-height";
    return {};
  }
  int loadBest()
  {
    int n = 0;
    std::ifstream f(scorePath());
    f >> n;
    return std::max(0, n);
  }
  void saveBest(int n)
  {
    auto p = scorePath();
    if (p.empty())
      return;
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
}

int main(int argc, char **argv)
{
  try
  {
    uint32_t seed = std::random_device{}();
    for (int i = 1; i < argc; ++i)
    {
      std::string a = argv[i];
      if (a == "--help" || a == "-h")
      {
        std::cout << "BACKBLAST - endless recoil-only terminal platformer\n\n"
                  << "Aim with mouse. Hold left click to fire. Aim DOWN to fly UP.\n"
                  << "P: pause   R: restart   Q / Ctrl-C: quit   Enter: start\n"
                  << "Reload 1 shell per 0.75s up to 4; kills add 3 up to 6.\n"
                  << "Safe landings add 2 ammo per touchdown, capped at 6.\n"
                  << "Firing resets reload progress. Platforms gradually slow sideways motion.\n"
                  << "Brute kills also instantly restore 1 HP. Enemies drop no pickups.\n"
                  << "Usage: backblast [--seed NUMBER]\n"
                  << "Requires a mouse-capable UTF-8 terminal, at least 72 x 24.\n";
        return 0;
      }
      if (a == "--seed" && i + 1 < argc)
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
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    std::signal(SIGHUP, stop);
    bb::Terminal terminal;
    bb::Game game(seed);
    int best = loadBest(), savedBest = best;
    bool started = false, paused = false, held = false;
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
          if (key == 'p' || key == 'P')
          {
            paused = !paused;
            held = false;
          }
          if (key == 'r' || key == 'R' || ((key == '\r' || key == '\n') && game.dead))
          {
            game.reset(seed);
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
        accumulator += std::min(elapsed, 0.1);
        while (accumulator >= step && !game.dead)
        {
          if (held)
            game.shoot(mapping.mouse(mx, my));
          game.update(step);
          accumulator -= step;
        }
      }
      else
      {
        accumulator = 0;
        if (small || game.dead)
          held = false;
      }
      best = std::max(best, game.score());
      if (game.dead && best > savedBest)
      {
        saveBest(best);
        savedBest = best;
      }
      Screen screen(std::max(6, terminal.columns), std::max(8, terminal.rows), game.camera);
      if (small)
      {
        screen.text(0, 1, "BACKBLAST", cyan);
        screen.text(0, 3, "Resize terminal to at least 72 x 24.");
        screen.text(0, 5, "Game frozen. Q to quit.", gold);
      }
      else
      {
        Vec aim = mapping.mouse(mx, my);
        // Small, short-lived camera kick, never applied to physics or mouse aiming.
        if (game.shake > 0)
          screen.bottom += std::sin(game.time * 130) * game.shake * 0.8;
        screen.world(game, aim);
        screen.hud(game, best);
        screen.text(2, terminal.rows - 1, "MOUSE aim / LMB fire   P pause   R retry   Q quit", muted);
        if (!started)
          screen.panel(terminal.rows / 2 - 7, {"B A C K B L A S T", "RECOIL IS YOUR ONLY WAY UP", "",
                                               "Point DOWN. Hold LEFT CLICK. Fly UP.",
                                               "Aim sideways to bounce off the walls.",
                                               "Kills +3, landings +2. Stack ammo up to 6.",
                                               "Brute kills heal 1 HP. Enemies drop no items.",
                                               "Gold + = shells / green + = repair.",
                                               "Shoot hunters. Chain kills. Outrun the meltdown.", "",
                                               "CLICK TO DROP IN  /  ENTER TO START"});
        else if (game.dead)
          screen.panel(terminal.rows / 2 - 5, {"R U N  O V E R", game.deathReason, "",
                                               "HEIGHT " + std::to_string(game.score()) + " METERS",
                                               std::to_string(game.kills) + " HUNTERS DOWN  /  BEST " + std::to_string(best) + " m", "",
                                               "R / ENTER: instant retry    Q: leave"});
        else if (paused)
          screen.panel(terminal.rows / 2 - 2, {"P A U S E D", "P: resume   R: retry   Q: quit"});
        else if (game.time < 5)
          screen.centered(terminal.rows - 7, "AIM BELOW YOURSELF + HOLD LEFT CLICK TO CLIMB", gold);
      }
      terminal.write(screen.frame());
      std::this_thread::sleep_until(frameStart + std::chrono::microseconds(16667));
    }
    if (best > savedBest)
      saveBest(best);
  }
  catch (const std::exception &e)
  {
    std::cerr << "BACKBLAST: " << e.what() << '\n';
    return 1;
  }
}