#include "terminal.hpp"
#include <cerrno>
#include <cstdio>
#include <poll.h>
#include <stdexcept>
#include <sys/ioctl.h>
#include <unistd.h>

namespace bb
{
  std::vector<Input> InputParser::feed(const std::string &bytes)
  {
    pending += bytes;
    std::vector<Input> result;
    while (!pending.empty())
    {
      if (pending[0] != '\033')
      {
        result.push_back({Input::Key, static_cast<unsigned char>(pending[0])});
        pending.erase(0, 1);
        continue;
      }
      if (pending.size() < 3)
        break;
      if (pending.starts_with("\033[<"))
      {
        auto end = pending.find_first_of("mM", 3);
        if (end == std::string::npos)
        {
          if (pending.size() > 64)
            pending.clear();
          break;
        }
        int button = 0, x = 0, y = 0;
        if (std::sscanf(pending.c_str() + 3, "%d;%d;%d", &button, &x, &y) == 3)
        {
          bool wheel = (button & 64) != 0;
          bool released = pending[end] == 'm' || (!wheel && (button & 3) == 3);
          result.push_back({Input::Mouse, 0, x - 1, y - 1,
                            !wheel && !released && (button & 3) == 0, released});
        }
        pending.erase(0, end + 1);
      }
      else if (pending[1] == '[' || pending[1] == 'O')
      {
        auto end = pending.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz~", 2);
        if (end == std::string::npos)
        {
          if (pending.size() > 64)
            pending.clear();
          break;
        }
        pending.erase(0, end + 1);
      }
      else
      {
        pending.erase(0, 1);
      }
    }
    return result;
  }
  Terminal::Terminal()
  {
    if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
      throw std::runtime_error("Open BACKBLAST in an interactive terminal (not the output panel).");
    if (tcgetattr(STDIN_FILENO, &original) != 0)
      throw std::runtime_error("Cannot read terminal settings");
    auto raw = original;
    raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | IEXTEN));
    raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
      throw std::runtime_error("Cannot enable terminal input");
    active = true;
    write("\033[?1049h\033[?25l\033[?7l\033[?1003h\033[?1006h\033[2J");
    resize();
  }
  Terminal::~Terminal()
  {
    if (active)
    {
      write("\033[0m\033[?1006l\033[?1003l\033[?7h\033[?25h\033[?1049l");
      tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
    }
  }
  void Terminal::write(const std::string &frame)
  {
    size_t offset = 0;
    while (offset < frame.size())
    {
      auto n = ::write(STDOUT_FILENO, frame.data() + offset, frame.size() - offset);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        break;
      offset += static_cast<size_t>(n);
    }
  }
  void Terminal::resize()
  {
    winsize size{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col && size.ws_row)
    {
      columns = size.ws_col;
      rows = size.ws_row;
    }
  }
  std::vector<Input> Terminal::input()
  {
    std::string bytes;
    pollfd fd{STDIN_FILENO, POLLIN, 0};
    char buffer[4096];
    while (poll(&fd, 1, 0) > 0 && (fd.revents & POLLIN))
    {
      auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
      if (count <= 0)
        break;
      bytes.append(buffer, static_cast<size_t>(count));
      if (bytes.size() >= 65536)
        break;
    }
    return parser.feed(bytes);
  }
}