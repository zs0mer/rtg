#pragma once
#include <string>
#include <termios.h>
#include <vector>

namespace bb
{
  struct Input
  {
    enum Kind
    {
      Key,
      Mouse
    } kind;
    int key = 0, x = 0, y = 0;
    bool down = false, release = false;
  };
  class InputParser
  {
    std::string pending;

  public:
    std::vector<Input> feed(const std::string &bytes);
  };
  class Terminal
  {
    termios original{};
    bool active = false;
    InputParser parser;

  public:
    int columns = 100, rows = 32;
    Terminal();
    ~Terminal();
    Terminal(const Terminal &) = delete;
    Terminal &operator=(const Terminal &) = delete;
    void resize();
    std::vector<Input> input();
    void write(const std::string &frame);
  };
}