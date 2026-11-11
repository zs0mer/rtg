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

  std::vector<Cell> decodeHud(const Screen &screen, int columns, int rows)
  {
    std::vector<Cell> result;
    RGB fg = ink, bg = ink;
    const std::string frame = screen.frame();
    for (size_t i = 0; i < frame.size();)
    {
      if (frame[i] == '\033')
      {
        check(i + 1 < frame.size() && frame[i + 1] == '[', "valid HUD CSI escape");
        size_t end = frame.find_first_of("Hm", i + 2);
        check(end != std::string::npos, "terminated HUD CSI escape");
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
            check(bool(input) && mode == 2, "HUD truecolor SGR");
            (code == 38 ? fg : bg) = color;
          }
          else if (code == 49)
            bg = ink;
          else
            check(code == 0 || code == 1, "supported HUD SGR");
        }
        i = end + 1;
      }
      else if (frame[i] == '\r' || frame[i] == '\n')
        ++i;
      else
      {
        size_t length = frame.compare(i, 3, "▀") == 0 || frame.compare(i, 3, "▄") == 0 ? 3 : 1;
        result.push_back({fg, bg, frame.substr(i, length)});
        i += length;
      }
    }
    check(result.size() == static_cast<size_t>(columns * rows), "complete decoded HUD frame");
    return result;
  }

  void velocityRendering()
  {
    for (auto [columns, rows] : {std::pair{72, 24}, {104, 36}, {110, 38}, {180, 50}})
      for (size_t theme = 0; theme < themes.size(); ++theme)
        for (Vec velocity : {Vec{}, Vec{3, 4}, Vec{-30, -40}, Vec{56.999, 0}, Vec{0, -57},
                             Vec{57.001, 0}, Vec{-48, 64}, Vec{90, -120}})
        {
          double speed = velocity.length();
          context = "velocity " + std::to_string(columns) + "x" + std::to_string(rows) +
                    " " + themes[theme].name + " speed=" + std::to_string(speed);
          applyTheme(theme, 650);
          auto g = emptyWorld();
          g.player.vel = velocity;
          g.combo = 123;
          Screen screen(columns, rows, 0);
          screen.world(g, {50, -9990});
          screen.hud(g, 987, 42);
          auto actual = decodeHud(screen, columns, rows);
          auto at = [&](int x, int y) -> const Cell & { return actual.at(static_cast<size_t>(y * columns + x)); };
          auto label = [&](int x, int y, const std::string &text, RGB color)
          {
            for (size_t i = 0; i < text.size(); ++i)
              check(at(x + static_cast<int>(i), y).glyph == text.substr(i, 1) &&
                        at(x + static_cast<int>(i), y).fg == color, "meter text and color");
          };
          bool overcap = speed > 57;
          RGB color = overcap ? RGB{255, 55, 20} : cyan;
          label(3, 4, "VELOCITY", cyan);
          label(3, 5, "SOFT 57", RGB{255, 220, 35});
          label(3, 11, overcap ? "OVERCAP" : "u/s", color);
          int filled = std::clamp(static_cast<int>(speed / 57 * 9), 0, 9);
          label(3, 12, std::string(filled, '=') + std::string(9 - filled, '.'), color);
          label(12, 12, "|", RGB{255, 220, 35});
          label(13, 12, overcap ? "!" : ".", overcap ? RGB{255, 55, 20} : muted);
          Screen digits(columns, rows, 0);
          digits.bigNumber(3, 6, static_cast<int>(std::lround(speed)), speed > 0 ? color : muted);
          auto expected = decodeHud(digits, columns, rows);
          for (int y = 6; y <= 10; ++y)
            for (int x = 3; x < 14; ++x)
            {
              const Cell &e = expected.at(static_cast<size_t>(y * columns + x));
              check(at(x, y).glyph == e.glyph && at(x, y).bg == e.bg &&
                        (e.glyph == " " || at(x, y).fg == e.fg), "rounded combined velocity digits");
            }
          label(columns - 14, 4, "STREAK", cyan);
          label(2, rows - 6, "HP", repair);
          label(columns - 26, rows - 6, "AMMO", gold);
          Screen mapping(columns, rows, 0);
          check(screen.point({50, 20}) == mapping.point({50, 20}) && screen.scale == mapping.scale,
                "HUD does not alter world mapping");
        }
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
          digits.bigNumber(x, 6, combo, combo > 0 ? gold : muted);
          auto decoded = decodeHud(screen, columns, rows);
          auto expected = decodeHud(digits, columns, rows);
          for (int row = 6; row <= 10; ++row)
            for (int col = x; col < x + width; ++col)
            {
              size_t index = static_cast<size_t>(row * columns + col);
              check(decoded[index].glyph == expected[index].glyph && decoded[index].bg == expected[index].bg,
                    "current streak digits remain on rows 6 through 10");
            }
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

    saveBest(42);
    saveBest(7, "best-streak");
    auto contents = [&](const char *name)
    {
      std::ifstream file(state.path / "zapy" / name);
      return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    };
    const auto theme = contents("theme");
    auto unchanged = [&]
    {
      check(contents("best-height") == "42\n", "unranked run preserves saved height bytes");
      check(contents("best-streak") == "7\n", "unranked run preserves saved streak bytes");
      check(contents("theme") == theme, "record tracking preserves theme bytes");
      check(!std::filesystem::exists(state.path / "zapy/best-height.tmp") &&
                !std::filesystem::exists(state.path / "zapy/best-streak.tmp"),
            "record tracking leaves no temporary files");
    };
    {
      context = "debug-first run records";
      RunRecords records;
      records.beginRun();
      check(records.best == 42 && records.bestStreak == 7, "loads legitimate run baselines");
      bb::Game game(42);
      check(game.debugSwitchMode() && game.shotMode == bb::ShotMode::Longshot && game.debugUsed,
            "alive debug switch starts deterministic cycle and marks run");
      game.altitude = 100;
      game.combo = 12;
      game.comboTime = 1;
      records.update(game);
      check(records.best == 42 && records.bestStreak == 7, "debug height and combo never become records");
      unchanged();
      check(game.debugSwitchMode() && game.shotMode == bb::ShotMode::Scatter, "second debug switch selects Scatter");
      check(game.debugSwitchMode() && game.shotMode == bb::ShotMode::Normal && game.debugUsed,
            "cycling back to Normal remains unranked");
      records.update(game);
      Screen hud(110, 38, 0);
      hud.hud(game, records.best, records.bestStreak);
      const auto plain = stripAnsi(hud.frame());
      check(plain.find("UNRANKED") != std::string::npos && plain.find("HEIGHT 100 m") != std::string::npos &&
                plain.find("BEST 42 m") != std::string::npos && plain.find("BEST 7") != std::string::npos,
            "unranked HUD keeps current height and legitimate records");
      Screen digits(110, 38, 0);
      digits.bigNumber(98, 6, 12, gold);
      auto actual = decodeHud(hud, 110, 38), expected = decodeHud(digits, 110, 38);
      for (int row = 6; row <= 10; ++row)
        for (int col = 98; col < 105; ++col)
        {
          size_t index = static_cast<size_t>(row * 110 + col);
          check(actual[index].glyph == expected[index].glyph && actual[index].bg == expected[index].bg,
                "unranked HUD still displays current streak digits");
        }
      game.dead = true;
      records.update(game);
      records.finish();
      check(records.best == 42 && records.bestStreak == 7, "debug death and finish cannot award records");
      unchanged();
    }
    {
      context = "midrun debug rollback";
      RunRecords records;
      bb::Game game(42);
      game.altitude = 100;
      game.combo = 12;
      records.update(game);
      check(records.best == 100 && records.bestStreak == 12, "normal updates accumulate provisional records");
      check(contents("best-height") == "42\n", "legitimate height waits for finish");
      check(contents("best-streak") == "12\n", "legitimate streak is persisted immediately");
      check(game.debugSwitchMode(), "midrun debug switch succeeds");
      records.update(game);
      check(records.best == 42 && records.bestStreak == 7, "debug withdraws this run's earlier records");
      unchanged();
      const auto heightTime = std::filesystem::last_write_time(state.path / "zapy/best-height");
      const auto streakTime = std::filesystem::last_write_time(state.path / "zapy/best-streak");
      for (int i = 0; i < 6; ++i)
      {
        check(game.debugSwitchMode(), "repeated alive debug switch succeeds");
        records.update(game);
        records.finish();
      }
      check(std::filesystem::last_write_time(state.path / "zapy/best-height") == heightTime &&
                std::filesystem::last_write_time(state.path / "zapy/best-streak") == streakTime,
            "repeated debug updates and finish do not rewrite records");
      game.reset(42);
      check(!game.debugUsed && game.shotMode == bb::ShotMode::Normal, "reset clears debug state and restores Normal");
      game.altitude = 150;
      game.combo = 20;
      records.update(game);
      records.finish();
      check(records.best == 42 && records.bestStreak == 7, "record lock survives game flag clearing without beginRun");
      unchanged();

      game.reset(42);
      records.beginRun();
      game.altitude = 80;
      game.combo = 9;
      records.update(game);
      check(records.best == 80 && records.bestStreak == 9, "new run is eligible after reset and beginRun");
      check(contents("best-height") == "42\n" && contents("best-streak") == "9\n",
            "new legitimate run keeps deferred height and immediate streak persistence");
      records.finish();
      check(contents("best-height") == "80\n", "finish persists pending legitimate height");
      game.reset(42);
      records.beginRun();
      game.altitude = 90;
      game.combo = 10;
      game.dead = true;
      records.update(game);
      check(contents("best-height") == "90\n" && contents("best-streak") == "10\n",
            "normal death finishes legitimate records");
      check(!game.debugSwitchMode() && !game.debugUsed && game.shotMode == bb::ShotMode::Normal,
            "dead debug is ignored without marking run");
      records.update(game);
      records.finish();
      RunRecords reloaded;
      check(reloaded.best == 90 && reloaded.bestStreak == 10, "dead debug cannot withdraw finished records");
      game.reset(42);
      records.beginRun();
      game.altitude = 110;
      game.combo = 15;
      records.update(game);
      check(game.debugSwitchMode(), "debug switch succeeds in later run");
      records.update(game);
      records.finish();
      check(records.best == 90 && records.bestStreak == 10 &&
                contents("best-height") == "90\n" && contents("best-streak") == "10\n",
            "beginRun refreshes rollback baselines to latest legitimate records");
      check(contents("theme") == theme, "legitimate records preserve theme");
    }
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

  void fireParticleRendering()
  {
    constexpr RGB yellow{255, 220, 35}, burningRed{255, 55, 20};
    for (auto [columns, rows] : {std::pair{72, 24}, {104, 36}, {128, 50}})
      for (size_t theme = 0; theme < themes.size(); ++theme)
        for (double altitude : {0.0, 650.0})
          for (int mode = 0; mode < 3; ++mode)
          {
            context = "fire particles " + std::to_string(columns) + "x" + std::to_string(rows) +
                      " " + themes[theme].name + " altitude=" + std::to_string(altitude) +
                      " mode=" + std::to_string(mode);
            applyTheme(theme, altitude);
            auto g = emptyWorld();
            g.camera = 10;
            g.shotMode = static_cast<bb::ShotMode>(mode);
            g.particles = {{{35, 30}, {0, -8}, 1, 1, 4, g.shotMode},
                           {{45, 30}, {0, -8}, 1, 1, 5, g.shotMode},
                           {{55, 30}, {0, -8}, 0.5, 1, 4, g.shotMode},
                           {{65, 30}, {0, -8}, 0.5, 1, 5, g.shotMode}};
            Screen mapping(columns, rows, g.camera);
            Image image = render(g, columns, rows);
            const RGB expected[] = {yellow, burningRed, dim(yellow, 0.5), dim(burningRed, 0.5)};
            for (size_t i = 0; i < g.particles.size(); ++i)
            {
              auto [x, y] = mapping.point(g.particles[i].pos);
              check(image.at(x, y) == expected[i], "fire particle RGB and lifetime fade ignore theme and shot mode");
            }
          }
  }

  void heatShieldRendering()
  {
    struct Flame
    {
      int x, y; // Relative to the player body.
      RGB color;
    };
    struct Plasma
    {
      RGB nose;
      int size;
    };
    for (auto [columns, rows] : {std::pair{72, 24}, {104, 36}, {128, 50}})
      for (size_t theme = 0; theme < themes.size(); ++theme)
      {
        context = "heat shield " + std::to_string(columns) + "x" + std::to_string(rows) +
                  " " + themes[theme].name;
        applyTheme(theme);
        auto g = emptyWorld();
        g.player.pos = {50, 20};
        g.player.vel = {0, 110};
        g.camera = 10;
        g.modeTarget = {50, 10000};
        Screen mapping(columns, rows, g.camera);
        const auto [bodyX, bodyY] = mapping.point(g.player.pos);
        int cap = std::max(1, static_cast<int>(1.1 * mapping.scale)) + 1;
        // The shield's pixels are exactly those an identical, unshielded frame lacks.
        auto flames = [&](const bb::Game &state)
        {
          auto bare = state;
          bare.overspeedTime = bare.heatShieldBelowTime = bare.heatShieldBreak = 0;
          Image with = render(state, columns, rows), without = render(bare, columns, rows);
          std::vector<Flame> lit;
          for (int y = 0; y < with.height; ++y)
            for (int x = 0; x < with.width; ++x)
              if (!(with.at(x, y) == without.at(x, y)))
                lit.push_back({x - bodyX, y - bodyY, with.at(x, y)});
          return lit;
        };
        auto absent = [&](const bb::Game &state)
        {
          check(flames(state).empty(), "no shield is drawn without charge or break-up");
        };
        auto onRamp = [&](const std::vector<Flame> &lit)
        {
          for (const auto &f : lit)
            check(f.color.r >= 80 && f.color.r >= f.color.g && f.color.g >= f.color.b,
                  "shield stays on the ember-red to white-hot ramp");
        };
        // The heading is in screen space, where rows grow downward. A complete cap is the
        // ignited shield; a charging one has only spread part of the way from the nose.
        auto plasma = [&](const bb::Game &state, int headingX, int headingY, bool complete)
        {
          auto lit = flames(state);
          onRamp(lit);
          Plasma result{ink, static_cast<int>(lit.size())};
          auto burning = [&](int x, int y)
          {
            return std::any_of(lit.begin(), lit.end(), [&](const Flame &f) { return f.x == x && f.y == y; });
          };
          for (const auto &f : lit)
          {
            int forward = f.x * headingX + f.y * headingY, side = f.y * headingX - f.x * headingY;
            check(std::hypot(f.x, f.y) >= cap - 0.5, "shield never covers the character body");
            check(forward <= cap, "nothing burns ahead of the cap");
            check(std::abs(side) <= cap + 1 && forward >= -2 * cap,
                  "shield hugs the body: a cap and thin streamers, not an aura");
            if (f.x == headingX * cap && f.y == headingY * cap)
              result.nose = f.color;
          }
          for (int y = -cap - 1; complete && y <= cap + 1; ++y)
            for (int x = -cap - 1; x <= cap + 1; ++x)
            {
              double distance = std::hypot(x, y);
              if (distance >= cap - 0.5 && distance < cap + 0.5 && x * headingX + y * headingY >= 0.5 * distance)
                check(burning(x, y), "an unbroken cap covers the leading face of the body");
            }
          check(!(result.nose == ink), "the cap faces the direction of travel");
          return result;
        };
        check(bb::Game::heatShieldSpeedThreshold == 60, "shield speed threshold is 60");
        check(bb::Game::softSpeedThreshold == 57, "soft physics threshold remains 57");
        check(!g.heatShieldActive(), "speed alone does not activate the shield");
        absent(g);
        auto chargeStep = [&]
        {
          g.player.pos = {50, 20};
          g.player.vel = {0, 110};
          g.player.invulnerable = 100;
          g.lava = -10000;
          g.update(1.0 / 120);
          g.platforms.clear();
          g.enemies.clear();
          g.bullets.clear();
          g.pickups.clear();
          g.particles.clear();
          // The activation shockwave is an effect too; this fixture isolates the plasma.
          g.rings.clear();
          g.popups.clear();
        };
        // Charging frames are compared at one instant so only the charge differs.
        auto charging = [&]
        {
          auto frozen = g;
          frozen.player.pos = {50, 20};
          frozen.player.invulnerable = 0;
          frozen.time = 0;
          return plasma(frozen, 0, -1, false);
        };
        for (int i = 0; i < 120; ++i)
          chargeStep();
        Plasma warming = charging();
        for (int i = 120; i < 359; ++i)
          chargeStep();
        check(!g.heatShieldActive(), "shield is not active before three seconds");
        Plasma primed = charging();
        check(warming.nose.g < primed.nose.g && warming.size < primed.size,
              "charging glow spreads from the nose and heats up");
        chargeStep();
        chargeStep();
        check(g.heatShieldActive() && !g.dead, "continuous overspeed activates rendering fixture");
        g.player.pos = {50, 20};
        g.player.invulnerable = 0;
        g.time = 0;
        Image visible = render(g, columns, rows);
        Plasma climbing = plasma(g, 0, -1, true);
        check(climbing.nose.g >= 150 && climbing.nose.g > primed.nose.g,
              "ignition takes the nose from a charging glow to yellow or white hot");
        check(!(visible.at(bodyX, bodyY) == ink), "player is visible behind the shield");
        g.time = 0.125;
        check(render(g, columns, rows).pixels != visible.pixels, "shield visibly animates with time");
        plasma(g, 0, -1, true);
        g.time = 1.0 / 18;
        g.player.invulnerable = 1;
        check(render(g, columns, rows).at(bodyX, bodyY) == ink, "invulnerability blink hides player body");
        plasma(g, 0, -1, true);
        g.player.invulnerable = 0;
        auto slowed = g;
        slowed.player.vel = {60, 0};
        check(slowed.heatShieldActive(), "shield remains visible at speed 60 during grace period");
        Plasma sideways = plasma(slowed, 1, 0, true);
        slowed.player.vel = {59, 0};
        slowed.heatShieldBelowTime = 0.999;
        check(slowed.heatShieldActive(), "shield remains visible just before one-second slowdown expiry");
        Plasma cooling = plasma(slowed, 1, 0, true);
        check(cooling.nose.g < sideways.nose.g && cooling.size <= sideways.size,
              "shield cools as the grace period runs out");
        slowed.player.vel = {};
        slowed.heatShieldBelowTime = 1;
        check(!slowed.heatShieldActive(), "shield disappears after one second below threshold");
        absent(slowed);
        auto hottest = [](const std::vector<Flame> &lit)
        {
          int green = 0;
          for (const auto &f : lit)
            green = std::max(green, f.color.g);
          return green;
        };
        slowed.heatShieldBreakHeat = 1;
        slowed.heatShieldBreak = bb::Game::heatShieldBreakSeconds * 0.95;
        auto shards = flames(slowed);
        slowed.heatShieldBreak = bb::Game::heatShieldBreakSeconds * 0.1;
        auto debris = flames(slowed);
        onRamp(shards);
        onRamp(debris);
        check(!shards.empty() && debris.size() <= shards.size() && hottest(debris) < hottest(shards),
              "a broken shield flies apart in shards that cool and crumble away");
        g.heatShieldBreak = bb::Game::heatShieldBreakSeconds / 2;
        g.dead = true;
        check(!g.heatShieldActive(), "dead player has no active shield");
        absent(g);
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
  void decorRendering()
  {
    applyTheme(0);
    context = "decor";
    constexpr int columns = 104, rows = 50;
    auto g = emptyWorld();
    g.camera = 100;
    g.platforms = {{30, 110, 20, false}, {60, 120, 15, false}};
    Screen plain(columns, rows, g.camera);
    plain.world(g, {50, -9990});
    Image bare = decode(plain, columns, rows);
    Screen dressed(columns, rows, g.camera);
    dressed.decor = true;
    dressed.bestHeight = 112;
    dressed.world(g, {50, -9990});
    Image image = decode(dressed, columns, rows);
    int added = 0;
    for (int y = 0; y < image.height; ++y)
      for (int x = 0; x < image.width; ++x)
      {
        if (!(bare.at(x, y) == ink))
        {
          check(image.at(x, y) == bare.at(x, y), "decoration never paints over the gameplay layer");
          continue;
        }
        if (image.at(x, y) == ink)
          continue;
        ++added;
        RGB c = image.at(x, y);
        check(std::max({c.r, c.g, c.b}) <= 115, "decoration stays dim enough never to be read as gameplay");
      }
    check(added > 20, "decoration adds a visible starfield");
    int line = dressed.point({50, 115}).second, dashes = 0;
    for (int x = 0; x < image.width; ++x)
      dashes += image.at(x, line) == dim(gold, 0.45);
    check(dashes > 20, "the height to beat is drawn as a dashed line across the arena");
    auto [ledgeX, ledgeY] = dressed.point({67, 120});
    check(image.at(ledgeX, ledgeY) == cyan, "all safe ledges retain the platform colour");

    g.enemies = {{{50, 115}, {}, 1, 1.25, 0, 100, bb::Enemy::volatileDrone}};

    g.time = 0;
    Image marked = render(g, columns, rows);
    auto [bodyX, bodyY] = dressed.point({51, 115});
    RGB body = marked.at(bodyX, bodyY);
    check(body == fireYellow || body == fireRed, "volatiles flash in fire colours, unlike any themed enemy");
    g.popups = {{{50, 115}, "+HP", 0.9, 0.9, 3}};
    g.banner = "HEAT SHIELD";
    g.bannerTime = 1;
    Screen labelled(columns, rows, g.camera);
    labelled.world(g, {50, -9990});
    labelled.overlay(g);
    const std::string text = stripAnsi(labelled.frame());
    check(text.find("+HP") != std::string::npos && text.find("-=  HEAT SHIELD  =-") != std::string::npos,
          "floating labels and announcements are drawn over the playfield");
  }
}

int main()
{
  try
  {
    decorRendering();
    themeRendering();
    shotRendering();
    heatShieldRendering();
    velocityRendering();
    fireParticleRendering();
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
