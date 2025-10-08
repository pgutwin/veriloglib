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
  // const auto& top = nl.modules[1];
  // EXPECT_EQ(top.module_instances.size(), 2u);
  const auto& top = nl.modules[1];
  ASSERT_EQ(top.module_instances.size(), 2u);

  // ---- u0: positional connections (a, n1)
  {
    const auto& u0 = top.module_instances[0];
    EXPECT_EQ(u0.module_name, "leaf");
    EXPECT_EQ(u0.instance_name, "u0");
    ASSERT_EQ(u0.ports_pos.size(), 2u);
    // ports_named should be empty for positional style
    EXPECT_TRUE(u0.ports_named.empty());

    // Helper to get identifier string from Expr
    auto id_of = [](const Expr& e) -> std::string {
      if (std::holds_alternative<Identifier>(e)) {
        return std::get<Identifier>(e).name;
      }
      return expr_to_string(e);
    };

    EXPECT_EQ(id_of(u0.ports_pos[0]), "a");
    EXPECT_EQ(id_of(u0.ports_pos[1]), "n1");
  }

  // ---- u1: named connections (.x(n1), .y(b))
  {
    const auto& u1 = top.module_instances[1];
    EXPECT_EQ(u1.module_name, "leaf");
    EXPECT_EQ(u1.instance_name, "u1");
    // positional list should be empty for named style
    EXPECT_TRUE(u1.ports_pos.empty());
    ASSERT_EQ(u1.ports_named.size(), 2u);

    auto id_of = [](const Expr& e) -> std::string {
      if (std::holds_alternative<Identifier>(e)) return std::get<Identifier>(e).name;
      return expr_to_string(e);
    };

    ASSERT_TRUE(u1.ports_named.count("x"));
    ASSERT_TRUE(u1.ports_named.count("y"));
    EXPECT_EQ(id_of(u1.ports_named.at("x")), "n1");
    EXPECT_EQ(id_of(u1.ports_named.at("y")), "b");
  }
}

TEST(Parse, CommentsEverywhere) {
  using namespace verilog;
  const char* data = R"(
    // Leaf with comments sprinkled in
    module /*blk*/ leaf ( /*h*/ x /*h*/, y /*h*/ ) /*blk*/ ;
      input  /*c*/ x /*c*/ ;
      output /*c*/ y /*c*/ ;
    endmodule

    // Top with comments between every symbol we can think of
    module (* keep = "true" *) top /*mod*/ ( /*h1*/ a /*c*/, /*h2*/ b , c /*tr*/ ) /*trail*/ ;
      // Declarations with comments around tokens and punctuation
      input  /*ix*/ a /*ix*/ ;
      output /*ox*/ c /*ox*/ ;
      wire   /*wx1*/ n1 /*wx2*/ ;

      // Continuous assign with comments between lhs, '=', and rhs
      assign /*A*/ c /*B*/ = /*C*/ a /*D*/ ;

      // Positional instantiation with comments around parens/commas
      leaf /*t*/ u0 /*id*/ ( /*lp*/ a /*,*/ , /*between*/ n1 /*rp*/ ) /*after*/ ;

      // Named instantiation; comments before dot/inside parens + attribute block
      leaf u1 ( .x ( (*attr*) n1 ) , .y ( /* y comment */ c ) ) ;
    endmodule
  )";

  auto nl = parse_string(data);
  ASSERT_EQ(nl.modules.size(), 2u);

  const Module& top = nl.modules[1];
  EXPECT_EQ(top.module_name, "top");

  // Header port list should be intact despite comments.
  ASSERT_EQ(top.port_list.size(), 3u);
  EXPECT_EQ(top.port_list[0], "a");
  EXPECT_EQ(top.port_list[1], "b");
  EXPECT_EQ(top.port_list[2], "c");

  // Declarations: one statement each in this subset implementation
  ASSERT_EQ(top.input_declarations.size(), 1u);
  EXPECT_EQ(top.input_declarations[0].net_name, "a");

  ASSERT_EQ(top.output_declarations.size(), 1u);
  EXPECT_EQ(top.output_declarations[0].net_name, "c");

  ASSERT_EQ(top.net_declarations.size(), 1u);
  EXPECT_EQ(top.net_declarations[0].net_name, "n1");

  // Continuous assignment captured
  ASSERT_EQ(top.assignments.size(), 1u);
  ASSERT_EQ(top.assignments[0].assignments.size(), 1u);
  auto id_of = [](const Expr& e) -> std::string {
    if (std::holds_alternative<Identifier>(e)) return std::get<Identifier>(e).name;
    return ""; // subset test only expects simple identifiers
  };
  EXPECT_EQ(id_of(top.assignments[0].assignments[0].first),  "c");
  EXPECT_EQ(id_of(top.assignments[0].assignments[0].second), "a");

  // Instantiations survived comment soup
  ASSERT_EQ(top.module_instances.size(), 2u);

  // u0: positional
  {
    const auto& u0 = top.module_instances[0];
    EXPECT_EQ(u0.module_name,  "leaf");
    EXPECT_EQ(u0.instance_name,"u0");
    ASSERT_EQ(u0.ports_pos.size(), 2u);
    EXPECT_TRUE(u0.ports_named.empty());
    EXPECT_EQ(id_of(u0.ports_pos[0]), "a");
    EXPECT_EQ(id_of(u0.ports_pos[1]), "n1");
  }

  // u1: named
  {
    const auto& u1 = top.module_instances[1];
    EXPECT_EQ(u1.module_name,  "leaf");
    EXPECT_EQ(u1.instance_name,"u1");
    EXPECT_TRUE(u1.ports_pos.empty());
    ASSERT_EQ(u1.ports_named.size(), 2u);
    ASSERT_TRUE(u1.ports_named.count("x"));
    ASSERT_TRUE(u1.ports_named.count("y"));
    EXPECT_EQ(id_of(u1.ports_named.at("x")), "n1");
    EXPECT_EQ(id_of(u1.ports_named.at("y")), "c");
  }
}

TEST(Parse, InstantiationIndexedPort) {
  using namespace verilog;
  const char* data = R"(
    module INVx1_ASAP7_6t_L (A, Y);
      input A; output Y;
    endmodule

    module top(_04_, counter_out);
      input  _04_;
      output [1:0] counter_out;

      // This was the problematic case: named port connected to bit-select
      INVx1_ASAP7_6t_L _21_ (
        .A(_04_),
        .Y(counter_out[1])
      );
    endmodule
  )";

  auto nl = parse_string(data);
  ASSERT_GE(nl.modules.size(), 2u);
  const auto& top = nl.modules[1];
  ASSERT_EQ(top.module_name, "top");
  ASSERT_EQ(top.module_instances.size(), 1u);

  const auto& inst = top.module_instances[0];
  EXPECT_EQ(inst.module_name,  "INVx1_ASAP7_6t_L");
  EXPECT_EQ(inst.instance_name, "_21_");

  // A is simple identifier; Y should be IdentifierIndexed("counter_out", 1)
  ASSERT_TRUE(inst.ports_pos.empty());
  ASSERT_EQ(inst.ports_named.size(), 2u);
  ASSERT_TRUE(inst.ports_named.count("A"));
  ASSERT_TRUE(inst.ports_named.count("Y"));

  // Check .A(_04_)
  {
    const Expr& a = inst.ports_named.at("A");
    ASSERT_TRUE(std::holds_alternative<Identifier>(a));
    EXPECT_EQ(std::get<Identifier>(a).name, "_04_");
  }
  // Check .Y(counter_out[1])
  {
    const Expr& y = inst.ports_named.at("Y");
    if (std::holds_alternative<IdentifierIndexed>(y)) {
      const auto& yi = std::get<IdentifierIndexed>(y);
      EXPECT_EQ(yi.name, "counter_out");
      EXPECT_EQ(yi.index.as_integer(), 1);
    } else {
      // Provide a helpful failure if the variant isn't what we expect
      ADD_FAILURE() << "Expected IdentifierIndexed for .Y, got: " << expr_to_string(y);
    }
  }
}

// helper to check variant structurally (no stringification)
static bool is_slice_structural(const Expr& e, const char* base, int msb, int lsb) {
    if (auto p = std::get_if<IdentifierSliced>(&e)) {
        return p->name == base &&
               p->range.start.as_integer() == msb &&
               p->range.end.as_integer()   == lsb;
    }
    return false;
}
TEST(Parse, InstantiationBitSelectAndSlice) {
  using namespace verilog;

  const char* data = R"(
    // Leaf takes two ports
    module BUF2 (A, Y);
      input A; output Y;
    endmodule

    module top(sig, bus, counter_out);
      input  sig;
      output [7:0] bus;
      output [1:0] counter_out;

      // Named: bit-select on output, simple ident on input
      BUF2 u0 (
        .A(sig),
        .Y(counter_out[1])
      );

      // Positional: slice on second arg (just to ensure slices are also handled)
      BUF2 u1 ( sig, bus[7:0] );
    endmodule
  )";

  auto nl = parse_string(data);
  ASSERT_GE(nl.modules.size(), 2u);
  const auto& top = nl.modules[1];
  ASSERT_EQ(top.module_name, "top");
  ASSERT_EQ(top.module_instances.size(), 2u);

  // Helper for variant inspection
  auto is_id = [](const Expr& e, const std::string& n) {
    return std::holds_alternative<Identifier>(e) &&
           std::get<Identifier>(e).name == n;
  };
  auto is_index = [](const Expr& e, const std::string& n, int64_t idx) {
    if (!std::holds_alternative<IdentifierIndexed>(e)) return false;
    const auto& ii = std::get<IdentifierIndexed>(e);
    return ii.name == n && ii.index.as_integer() == idx;
  };
  // auto is_slice = [](const Expr& e, const std::string& n, int64_t msb, int64_t lsb) {
  //   if (!std::holds_alternative<IdentifierSliced>(e)) return false;
  //   const auto& s = std::get<IdentifierSliced>(e);
  //   return s.name == n &&
  //          s.range.msb.as_integer() == msb &&
  //          s.range.lsb.as_integer() == lsb;
  // };
auto is_slice = [](const Expr& e, const std::string& n, int64_t msb, int64_t lsb) {
  if (!std::holds_alternative<IdentifierSliced>(e)) return false;
  const auto s = expr_to_string(e);
  return s == (n + "[" + std::to_string(msb) + ":" + std::to_string(lsb) + "]");
};

  // --- u0: named connections: .A(sig), .Y(counter_out[1])
  {
    const auto& u0 = top.module_instances[0];
    EXPECT_EQ(u0.module_name,  "BUF2");
    EXPECT_EQ(u0.instance_name,"u0");
    EXPECT_TRUE(u0.ports_pos.empty());
    ASSERT_EQ(u0.ports_named.size(), 2u);
    ASSERT_TRUE(u0.ports_named.count("A"));
    ASSERT_TRUE(u0.ports_named.count("Y"));
    EXPECT_TRUE(is_id(u0.ports_named.at("A"), "sig"));
    EXPECT_TRUE(is_index(u0.ports_named.at("Y"), "counter_out", 1));
  }

  // --- u1: positional connections: (sig, bus[7:0])
  {
    const auto& u1 = top.module_instances[1];
    EXPECT_EQ(u1.module_name,  "BUF2");
    EXPECT_EQ(u1.instance_name,"u1");
    ASSERT_EQ(u1.ports_pos.size(), 2u);
    EXPECT_TRUE(u1.ports_named.empty());
    EXPECT_TRUE(is_id(u1.ports_pos[0], "sig"));


// right above the current failing line:
EXPECT_TRUE(is_slice_structural(u1.ports_pos[1], "bus", 7, 0));
    
    EXPECT_TRUE(is_slice(u1.ports_pos[1], "bus", 7, 0));
  }
}
