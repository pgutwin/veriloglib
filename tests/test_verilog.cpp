#include "veriloglib.hpp"
#include <gtest/gtest.h>

using namespace verilog;

TEST(Parse, SimpleModule) {
  std::string data = R"(
module top(a,b,c);
  input a, b;
  output c;
  wire w1;
  assign c = a;
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.module_name, "top");
  EXPECT_EQ(m.port_list.size(), 3u);
  EXPECT_EQ(m.input_declarations.size(), 1u);
  EXPECT_EQ(m.output_declarations.size(), 1u);
  EXPECT_EQ(m.net_declarations.size(), 1u);
  EXPECT_EQ(m.assignments.size(), 1u);
}

TEST(Parse, InoutPort) {
  std::string data = R"(
module test(A);
  inout A;
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  ASSERT_EQ(m.inout_declarations.size(), 1u);
  EXPECT_EQ(m.inout_declarations[0].net_name, "A");
}

TEST(Parse, Comments) {
  std::string data = R"(
module test(a,b);
  // single line comment
  /* block comment */ wire w1; (* attribute-ish comment *)
  input a;
  output b;
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 1u);
  const auto& m = nl.modules[0];
  EXPECT_EQ(m.net_declarations.size(), 1u);
  EXPECT_EQ(m.input_declarations.size(), 1u);
  EXPECT_EQ(m.output_declarations.size(), 1u);
}

TEST(Parse, Instantiation) {
  std::string data = R"(
module leaf(x,y);
  input x; output y;
endmodule

module top(a,b);
  wire n1;
  leaf u0 (a, n1);
  leaf u1 (.x(n1), .y(b));
endmodule
)";
  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 2u);
  const auto& top = nl.modules[1];
  EXPECT_EQ(top.module_instances.size(), 2u);
}
