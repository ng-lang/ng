// AI-generated code; reviewed for this repository's vNext rewrite.
#include "driver.hpp"
#include "imgui_natives.hpp"

#include <iostream>
#include <string_view>
#include <vector>

int main(int argc, char **argv)
{
  std::vector<std::string_view> arguments;
  arguments.reserve(static_cast<size_t>(argc > 0 ? argc - 1 : 0));
  for (int index = 1; index < argc; ++index)
  {
    arguments.emplace_back(argv[index]);
  }
  return NG::runDriverWithNatives(arguments, std::cout, std::cerr, NG::registerImguiNatives);
}
