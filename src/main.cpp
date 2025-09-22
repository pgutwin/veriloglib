#include "veriloglib.hpp"
#include <iostream>

int main(int argc, char** argv) {
  if (argc < 2) { std::cerr << "Usage: vparse <file.v>\n"; return 1; }
  try {
    verilog::Netlist nl = verilog::parse_file(argv[1]);
    std::cout << "Parsed modules: " << nl.modules.size() << "\n";
    for (const auto& m : nl.modules) { std::cout << m.summary() << "\n"; }
  } catch (const verilog::parse_error& e) {
    std::cerr << "Parse error: " << e.what() << "\n"; return 2;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n"; return 3;
  }
  return 0;
}
