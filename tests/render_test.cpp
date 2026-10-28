#define main zapyApplicationMain
#include "../src/main.cpp"
#undef main

#include <climits>
#include <stdexcept>

namespace
{
  int checks = 0;
  std::string context;
  void check(bool condition, const char *message)
  {
    ++checks;
    if (!condition)
      throw std::runtime_error(context + ": " + message);
  }

  struct Image
  {
    int width, height;
    std::vector<RGB> pixels;
    RGB at(int x, int y) const { return pixels.at(static_cast<size_t>(y * width + x)); }
  };

  // Decode the emitted terminal cells, not Screen's private pixel buffer.
  Image decode(const Screen &screen, int columns, int rows)
  {
    Image image{columns - 4, (rows - 10) * 2, {}};
    image.pixels.assign(static_cast<size_t>(image.width * image.height), ink);
    const std::string frame = screen.frame();
    RGB fg = ink, bg = ink;
    int x = 0, y = 0;
    for (size_t i = 0; i < frame.size();)
    {
      if (frame[i] == '\033')
      {
        check(i + 1 < frame.size() && frame[i + 1] == '[', "valid CSI escape");
        size_t end = frame.find_first_of("Hm", i + 2);
        check(end != std::string::npos, "terminated CSI escape");
        if (frame[end] == 'm')
        {
          std::string parameters = frame.substr(i + 2, end - i - 2);
          std::replace(parameters.begin(), parameters.end(), ';', ' ');
          std::istringstream input(parameters);
          int code = 0;
          input >> code;
          if (code == 38 || code == 48)
          {
            int mode = 0;
            RGB color{};
            input >> mode >> color.r >> color.g >> color.b;
            check(bool(input) && mode == 2, "truecolor SGR");
            (code == 38 ? fg : bg) = color;
          }
          else if (code == 49)
            bg = ink;
          else
            check(code == 0 || code == 1, "supported SGR");
        }
        i = end + 1;
        continue;
      }
      if (frame[i] == '\r')
      {
        x = 0;
        ++i;
        continue;
      }
      if (frame[i] == '\n')
      {
        ++y;
        ++i;
        continue;
      }
      RGB top = bg, bottomPixel = bg;
      if (frame.compare(i, 3, "▀") == 0)
      {
        top = fg;
        i += 3;
      }
      else if (frame.compare(i, 3, "▄") == 0)
      {
        bottomPixel = fg;
        i += 3;
      }
      else
      {
        check(frame[i] == ' ' || frame[i] == '|', "expected world-only glyph");
        ++i;
      }
      if (x >= 2 && x < columns - 2 && y >= 3 && y < rows - 7)
      {
        size_t at = static_cast<size_t>((y - 3) * 2 * image.width + x - 2);
        image.pixels[at] = top;
        image.pixels[at + image.width] = bottomPixel;
      }
      ++x;
    }
    check(y == rows - 1 && x == columns, "complete terminal frame");
    return image;
  }

  bb::Game emptyWorld()
  {
    bb::Game g(42);
    g.platforms.clear();
    g.enemies.clear();
    g.bullets.clear();
    g.pickups.clear();
    g.particles.clear();
    g.lava = -10000;
    g.player.pos = {50, -10000};
    g.modeTarget = {50, -10000};
    return g;
  }

  Image render(const bb::Game &g, int columns, int rows)
  {
    Screen screen(columns, rows, g.camera);
    screen.world(g, {50, -9990});
    return decode(screen, columns, rows);
  }

  void triangle(const Image &image, int center, int size, RGB color)
  {
    int count = 0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x)
      {
        bool expected = y <= size && std::abs(x - center) <= size - y;
        check((image.at(x, y) == color) == expected, "exact clipped downward warning triangle");
        count += expected;
      }
    check(count > 0, "warning is on screen");
  }

  RGB midpoint(RGB a, RGB b) { return {(a.r + b.r) / 2, (a.g + b.g) / 2, (a.b + b.b) / 2}; }

  void themeRendering()
  {
    for (size_t theme = 0; theme < themes.size(); ++theme)
    {
      const Theme &a = themes[theme], &b = themes[(theme + 1) % themes.size()];
      for (RGB color : {a.enemy, a.brute})
        check(std::max({color.r, color.g, color.b}) >= 235 &&
                  std::max({color.r, color.g, color.b}) - std::min({color.r, color.g, color.b}) >= 100,
              "enemy palettes stay vivid");
      for (size_t previous = 0; previous < theme; ++previous)
      {
        check(!(a.enemy == themes[previous].enemy), "regular enemy palette varies by theme");
        check(!(a.brute == themes[previous].brute), "brute palette varies by theme");
      }
      for (bool fade : {false, true})
      {
        context = std::string(a.name) + (fade ? " midpoint" : " endpoint");
        applyTheme(theme, fade ? 650 : 0);
        RGB expectedEnemy = fade ? midpoint(a.enemy, b.enemy) : a.enemy;
        RGB expectedBrute = fade ? midpoint(a.brute, b.brute) : a.brute;

        check(enemyColor == expectedEnemy && brute == expectedBrute, "theme blends both enemy colors");
        check(!(expectedEnemy == expectedBrute), "enemy classes remain distinct");

        auto g = emptyWorld();
        Screen screen(104, 50, g.camera);
        double top = screen.bottom + 79 / screen.scale;
        for (bool cooling : {false, true})
        {
          g.modeTarget = {25, top + bb::Game::modeTargetRadius + 10};
          g.modeTargetCooldown = cooling ? 1 : 0;
          triangle(render(g, 104, 50), screen.point(g.modeTarget).first, 3,
                   cooling ? RGB{122, 122, 122} : RGB{255, 255, 255});
        }
        g.modeTarget = {50, -10000};
        for (int type : {0, 1, 2})
        {
          double radius = type == 2 ? 2.6 : 1.2;
          g.enemies = {{{25, top + radius + 10}, {}, 1, radius, 0, 0, type}};
          triangle(render(g, 104, 50), screen.point(g.enemies[0].pos).first, 1,
                   type == 2 ? expectedBrute : expectedEnemy);
        }

        g.enemies = {{{25, 20}, {}, 1, 2, 0, 0, 0},
                     {{45, 20}, {}, 1, 2, 0, 0, 1},
                     {{65, 20}, {}, bb::Enemy::bruteHp / 2, 2.6, 0, 0, 2}};
        g.bullets = {{{35, 10}, {20, 0}, 1, true}, {{55, 10}, {20, 0}, 1, false}};
        Image image = render(g, 104, 50);
        for (const auto &enemy : g.enemies)
        {
          auto [x, y] = screen.point(enemy.pos + Vec{1, 0});
          check(image.at(x, y) == (enemy.type == 2 ? expectedBrute : expectedEnemy), "enemy body color");
        }
        auto [barX, barY] = screen.point(g.enemies.back().pos - Vec{1.5, 2.8});
        check(image.at(barX, barY) == expectedBrute, "brute health bar color");
        auto [emptyX, emptyY] = screen.point(g.enemies.back().pos + Vec{1.5, -2.8});
        check(image.at(emptyX, emptyY) == ink, "half-health brute bar is not full");
        for (const auto &bullet : g.bullets)
        {
          auto [x, y] = screen.point(bullet.pos);
          check(image.at(x, y) == (bullet.hostile ? expectedEnemy : RGB{0, 240, 255}), "hostile/default-normal bullet colors");
        }
      }
    }
  }

  void shotRendering()
  {
    constexpr int columns = 204, rows = 90;
    for (size_t theme = 0; theme < themes.size(); ++theme)
      for (bool fade : {false, true})
      {
        const Theme &a = themes[theme], &b = themes[(theme + 1) % themes.size()];
        applyTheme(theme, fade ? 650 : 0);
        const RGB colors[] = {{0, 240, 255}, {45, 80, 255}, fade ? midpoint(a.ammo, b.ammo) : a.ammo};
        for (int fired = 0; fired < 3; ++fired)
        {
          auto g = emptyWorld();
          g.player.pos = {20, 10};
          g.modeTarget = {50, 10000};
          g.shotMode = static_cast<bb::ShotMode>(fired);
          context = std::string(a.name) + (fade ? " midpoint" : " endpoint") + " fired=" + std::to_string(fired);
          check(g.shoot({80, 10}), "color test fires a real shot");
          bb::Bullet pellet = g.bullets.front();
          pellet.pos = {70, 20};
          pellet.vel = {};
          g.bullets = {pellet};
          // Separate real muzzle particles so every emitted particle can be sampled.
          for (size_t i = 0; i < g.particles.size(); ++i)
            g.particles[i].pos = {20 + 2.0 * i, 30};
          g.shotMode = static_cast<bb::ShotMode>((fired + 1) % 3);
          g.enemies = {{pellet.pos, {}, bb::Enemy::bruteHp, 1.25, 0, 100, 0}};
          g.update(0);
          check(g.bullets.empty() && g.particles.size() == 14, "real pellet produces four impact particles after switching modes");
          g.enemies.clear();
          for (size_t i = 10; i < g.particles.size(); ++i)
            g.particles[i].pos = {65 + 2.0 * (i - 10), 30};
          // Keep the fired pellet's tag, but give its rendered trail a measurable length.
          pellet.pos = {50, 20};
          pellet.vel = {250, 0};
          g.bullets = {pellet};
          bb::Bullet hostile = pellet;
          hostile.pos = {80, 20};
          hostile.hostile = true;
          g.bullets.push_back(hostile);
          for (int color = 1; color <= 3; ++color)
            g.particles.push_back({{40 + 5.0 * color, 40}, {}, 0.5, 1, color, pellet.shotMode});
          // Also exercise lifetime dimming independently of the theme blend.
          auto fading = g.particles.front();
          fading.pos = {35, 40};
          fading.life = fading.total * 0.5;
          g.particles.push_back(fading);

          for (int current = 0; current < 3; ++current)
          {
            context = std::string(a.name) + (fade ? " midpoint" : " endpoint") +
                      " fired=" + std::to_string(fired) + " current=" + std::to_string(current);
            g.shotMode = static_cast<bb::ShotMode>(current);
            Screen screen(columns, rows, g.camera);
            Vec aim{80, 10};
            screen.world(g, aim);
            Image image = decode(screen, columns, rows);
            auto at = [&](Vec pos) {
              auto [x, y] = screen.point(pos);
              return image.at(x, y);
            };
            auto [startX, trailY] = screen.point(pellet.pos - pellet.vel * 0.016);
            int endX = screen.point(pellet.pos).first;
            check(endX > startX, "pellet trail spans multiple pixels");
            for (int x = startX; x <= endX; ++x)
              check(image.at(x, trailY) == colors[fired], "entire pellet trail retains its firing mode color");
            auto [hostileX, hostileY] = screen.point(hostile.pos - hostile.vel * 0.016);
            for (int x = hostileX; x <= screen.point(hostile.pos).first; ++x)
              check(image.at(x, hostileY) == enemyColor, "hostile trail ignores shot mode and retains enemy color");
            for (size_t i = 0; i < 14; ++i)
              check(at(g.particles[i].pos) == colors[fired], "real muzzle and impact particles retain the firing mode color");
            check(at(fading.pos) == dim(colors[fired], 0.5), "shot particles dim their retained mode color with lifetime");
            const RGB otherColors[] = {cyan, red, repair};
            for (int color = 1; color <= 3; ++color)
              check(at({40 + 5.0 * color, 40}) == dim(otherColors[color - 1], 0.5),
                    "non-shot particles retain their existing palette and lifetime fade");

            auto [bodyX, bodyY] = screen.point(g.player.pos);
            auto [tipX, tipY] = screen.point(g.player.pos + Vec{2.6, 0});
            check(tipX > bodyX + 1 && tipY == bodyY, "horizontal gun has a visible body and separate emitter");
            for (int x = bodyX; x < tipX; ++x)
              check(image.at(x, bodyY) == RGB{0, 0, 0}, "gun body is pure black, not transparent or theme-colored");
            check(image.at(tipX, tipY) == colors[current], "single muzzle emitter pixel follows the current mode");
            check(at(g.player.pos + Vec{3.1, 0}) == colors[fired], "active muzzle flash retains the last successful shot's color");
            for (int distance : {5, 7, 9})
              check(at(g.player.pos + Vec{double(distance), 0}) == dim(colors[current], 0.38),
                    "aim trail follows the current mode rather than the last shot");
            check(at(aim) == colors[current] && at(aim + Vec{1, 0}) == colors[current] &&
                      at(aim + Vec{0, 1}) == colors[current], "crosshair follows the current mode");
            screen.hud(g, 0);
            RGB energy = colors[current];
            std::string modeText = "\033[38;2;" + std::to_string(energy.r) + ";" + std::to_string(energy.g) + ";" +
                                   std::to_string(energy.b) + "m" + g.shotModeName();
            check(screen.frame().find(modeText) != std::string::npos, "mode HUD emits current-mode colored text");

            g.flash = 0;
            Screen unlit(columns, rows, g.camera);
            unlit.world(g, aim);
            Image noFlash = decode(unlit, columns, rows);
            auto [flashX, flashY] = screen.point(g.player.pos + Vec{3.1, 0});
            check(noFlash.at(flashX, flashY) == dim(colors[current], 0.38), "expired flash reveals the current-mode aim trail");
            check(noFlash.at(tipX, tipY) == colors[current], "emitter stays colored without a muzzle flash");
            g.flash = 0.065;
          }
        }
      }
  }

  std::string stripAnsi(const std::string &frame)
  {
    std::string plain;
    for (size_t i = 0; i < frame.size();)
    {
      if (frame[i] == '\033')
      {
        check(i + 1 < frame.size() && frame[i + 1] == '[', "valid intro CSI escape");
        size_t end = frame.find_first_of("Hm", i + 2);
        check(end != std::string::npos, "terminated intro CSI escape");
        i = end + 1;
      }
      else
        plain += frame[i++];
    }
    return plain;
  }

  void streakRendering()
  {
    applyTheme(0);
    auto lines = [](const Screen &screen)
    {
      std::istringstream input(stripAnsi(screen.frame()));
      std::vector<std::string> result;
      for (std::string line; std::getline(input, line);)
      {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        result.push_back(line);
      }
      return result;
    };
    for (auto [columns, rows] : {std::pair{72, 24}, {110, 38}, {180, 50}})
      for (int record : {0, 42, INT_MAX})
      {
        auto g = emptyWorld();
        for (int combo : {7, 123, 0})
        {
          context = "streak " + std::to_string(columns) + "x" + std::to_string(rows) +
                    " record=" + std::to_string(record) + " combo=" + std::to_string(combo);
          g.combo = combo;
          g.comboTime = combo ? 1.0 : 0.0;
          Screen screen(columns, rows, 0);
          screen.hud(g, 987, record);
          auto actual = lines(screen);
          const std::string label = "BEST " + std::to_string(record);
          int width = std::max({9, static_cast<int>(std::to_string(combo).size()) * 4 - 1,
                                static_cast<int>(label.size())});
          int x = columns - width - 3;
          check(actual.size() == static_cast<size_t>(rows), "HUD preserves frame height");
          check(actual.at(1).find("BEST 987 m") != std::string::npos, "height record remains separate");
          check(actual.at(4).substr(x, 6) == "STREAK", "streak heading stays on row 4");
          check(actual.at(5).size() == static_cast<size_t>(columns) &&
                    actual.at(5).substr(x, width) == label + std::string(width - label.size(), ' '),
                "full streak record fits directly below heading, including after combo expires");
          Screen digits(columns, rows, 0);
          digits.bigNumber(x, 6, combo, gold);
          auto expected = lines(digits);
          for (int row = 6; row <= 10; ++row)
            check(actual.at(row) == expected.at(row), "current streak digits remain on rows 6 through 10");
          int remaining = combo ? width / 2 : 0;
          check(actual.at(12).substr(x, width) == std::string(remaining, '=') + std::string(width - remaining, '.'),
                "countdown remains on row 12 with the full panel width");
        }
      }
  }

  void streakPersistence()
  {
    context = "streak persistence";
    struct State
    {
      std::optional<std::string> previous;
      std::filesystem::path path;
      State()
      {
        if (const char *value = std::getenv("XDG_STATE_HOME"))
          previous = value;
        char pattern[] = "/tmp/zapy-render-XXXXXX";
        const char *created = mkdtemp(pattern);
        check(created != nullptr, "create isolated state directory");
        path = created;
        if (setenv("XDG_STATE_HOME", path.c_str(), 1) != 0)
        {
          std::filesystem::remove_all(path);
          throw std::runtime_error("set isolated XDG_STATE_HOME");
        }
      }
      ~State()
      {
        if (previous)
          setenv("XDG_STATE_HOME", previous->c_str(), 1);
        else
          unsetenv("XDG_STATE_HOME");
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
      }
    } state;
    check(loadBest("best-streak") == 0, "missing streak record defaults to zero");
    saveBest(987);
    saveTheme(themeIndex("prism"));
    for (int record : {7, 42, INT_MAX})
    {
      saveBest(record, "best-streak");
      check(loadBest("best-streak") == record, "updated streak record round-trips");
      check(loadBest() == 987, "streak save does not overwrite height");
      check(loadTheme() == themeIndex("prism"), "streak save does not overwrite theme");
      check(!std::filesystem::exists(state.path / "zapy/best-streak.tmp"), "atomic save leaves no temporary file");
    }
    {
      std::ofstream file(state.path / "zapy/best-streak");
      file << "-7\n";
    }
    check(loadBest("best-streak") == 0, "negative streak record is clamped to zero");
  }

  void introRendering()
  {
    applyTheme(0);
    check(titleArt.size() == 6, "six-line Zapy title");
    size_t titleWidth = 0;
    for (const auto *line : titleArt)
    {
      std::string art(line);
      check(art.find_first_not_of(" \t") != std::string::npos, "nonempty title art row");
      check(std::all_of(art.begin(), art.end(), [](unsigned char c) { return c >= 32 && c < 127; }),
            "title uses single-column ASCII glyphs");
      titleWidth = std::max(titleWidth, art.size());
    }
    for (auto [columns, rows] : {std::pair{72, 24}, {104, 36}, {180, 36}, {128, 50}})
    {
      context = "intro " + std::to_string(columns) + "x" + std::to_string(rows);
      Screen screen(columns, rows, 0);
      for (int row = 0; row < rows; ++row)
        screen.text(0, row, std::string(columns, '#'));
      screen.intro();
      std::string frame = screen.frame();
      std::istringstream input(stripAnsi(frame));
      std::vector<std::string> lines;
      for (std::string line; std::getline(input, line);)
      {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        check(line.size() == static_cast<size_t>(columns), "intro frame has no wrapping or truncated rows");
        lines.push_back(line);
      }
      check(lines.size() == static_cast<size_t>(rows), "intro preserves terminal row count");
      constexpr int panelWidth = 58;
      int panelX = (columns - panelWidth) / 2;
      int panelY = std::max(3, (rows - static_cast<int>(titleArt.size()) - 9) / 2);
      int titleX = (columns - static_cast<int>(titleWidth)) / 2;
      check(titleX >= panelX && titleX + static_cast<int>(titleWidth) <= panelX + panelWidth,
            "full title fits inside the 58-column panel");
      for (size_t row = 0; row < titleArt.size(); ++row)
      {
        std::string expected(panelWidth, ' ');
        expected.replace(static_cast<size_t>(titleX - panelX), std::string(titleArt[row]).size(), titleArt[row]);
        check(lines.at(panelY + 1 + row).substr(panelX, panelWidth) == expected,
              "actual title art retains fixed-column alignment on a cleared panel");
        const char *color = row % 2 == 0 ? "\033[38;2;0;240;255m" : "\033[38;2;255;255;255m";
        check(frame.find(std::string(color) + titleArt[row]) != std::string::npos,
              "alternating title rows use fixed cyan and white");
      }
      const std::string tagline = "Zapy // HIGH VOLTAGE. NO BRAKES.";
      const std::string marker = "CLICK TO DROP IN  /  ENTER TO START";
      check(lines.at(panelY + 7).substr((columns - tagline.size()) / 2, tagline.size()) == tagline,
            "full Zapy tagline is visible below the title");
      check(lines.at(panelY + 13).substr((columns - marker.size()) / 2, marker.size()) == marker,
            "full start marker remains visible");
      check(lines.at(panelY).substr(panelX, panelWidth) == std::string(panelWidth, ' ') &&
                lines.at(panelY + 14).substr(panelX, panelWidth) == std::string(panelWidth, ' '),
            "panel clears its top and bottom padding");
    }
  }

  void warningBounds()
  {
    applyTheme(0);
    for (auto [columns, rows] : {std::pair{72, 24}, {104, 36}, {180, 36}, {128, 50}})
    {
      context = std::to_string(columns) + "x" + std::to_string(rows);
      auto g = emptyWorld();
      g.camera = 137;
      Screen screen(columns, rows, g.camera);
      double top = screen.bottom + ((rows - 10) * 2 - 1) / screen.scale;
      for (double x : {0.0, 1.0, 9.0, 25.0, 75.0, 91.0, 99.0})
      {
        int center = static_cast<int>(std::lround((columns - 4 - bb::Game::width * screen.scale) / 2 + x * screen.scale));
        for (auto [gap, size] : {std::pair{120.0, 0}, {90.01, 0}, {90.0, 1}, {60.01, 1},
                                 {60.0, 2}, {30.01, 2}, {30.0, 3}, {0.01, 3}})
        {
          g.modeTarget = {x, top + bb::Game::modeTargetRadius + gap};
          triangle(render(g, columns, rows), center, size, RGB{255, 255, 255});
        }
      }
      for (double y : {top - 15, top + bb::Game::modeTargetRadius + 120.01, screen.bottom - 20})
      {
        g.modeTarget = {25, y};
        Image image = render(g, columns, rows);
        for (int x = 0; x < image.width; ++x)
          check(!(image.at(x, 0) == RGB{255, 255, 255}), "no modeball warning when visible, distant, or below");
      }
      g.modeTarget = {25, top + bb::Game::modeTargetRadius};
      Image touching = render(g, columns, rows);
      int center = screen.point(g.modeTarget).first;
      // A tangent body can touch row zero, but must not gain the seven-wide indicator.
      check(!(touching.at(center - 3, 0) == RGB{255, 255, 255}) && !(touching.at(center + 3, 0) == RGB{255, 255, 255}),
            "zero gap does not produce a warning");

      g.modeTarget = {50, -10000};
      for (int type : {0, 2})
      {
        RGB color = type == 2 ? brute : enemyColor;
        for (double hp : {0.0, -1.0, 1.0})
        {
          g.enemies = {{{75, top + 12}, {}, hp, 2, 0, 0, type}};
          Image image = render(g, columns, rows);
          if (hp > 0)
            triangle(image, screen.point(g.enemies[0].pos).first, 1, color);
          else
            check(std::find(image.pixels.begin(), image.pixels.end(), color) == image.pixels.end(),
                  "dead enemy warning stays hidden");
        }
        g.enemies = {{{75, top + 32}, {}, 1, 2, 0, 0, type}};
        triangle(render(g, columns, rows), screen.point(g.enemies[0].pos).first, 1, color);
        for (double y : {top - 15, top + 32.01, screen.bottom - 20})
        {
          g.enemies = {{{75, y}, {}, 1, 2, 0, 0, type}};
          Image image = render(g, columns, rows);
          for (int x = 0; x < image.width; ++x)
            check(!(image.at(x, 0) == color), "enemy warning remains range-limited and above-only");
        }
      }
    }
  }
}

int main()
{
  try
  {
    themeRendering();
    shotRendering();
    warningBounds();
    introRendering();
    streakRendering();
    streakPersistence();
    std::cout << checks << " rendering checks passed\n";
    return 0;
  }
  catch (const std::exception &error)
  {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
  }
}
