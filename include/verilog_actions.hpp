#pragma once
#include "veriloglib.hpp"
#include "verilog_grammar.hpp"
#include <tao/pegtl.hpp>
#include <cctype>
#include <utility>
#include <memory>

namespace verilog { namespace actions {

using namespace tao::pegtl;

struct State {
  std::optional<int> number_len;
  std::optional<char> number_base;
  std::string number_mantissa;

  std::string last_identifier;
  std::vector<std::string> id_history;

  std::vector<Expr> expr_stack;
  std::vector<std::vector<Expr>> concat_items_stack;

  enum class DeclMode { None, Net, In, Out, Inout };
  DeclMode decl_mode = DeclMode::None;
  std::optional<Range> current_range;
  std::vector<std::string> decl_names;

  std::vector<NetDeclaration>  net_decl_accum;
  std::vector<InputDeclaration>  in_decl_accum;
  std::vector<OutputDeclaration> out_decl_accum;
  std::vector<InoutDeclaration>  inout_decl_accum;

  std::vector<std::pair<Expr,Expr>> current_assign_list;
  std::vector<ContinuousAssign> assign_accum;

  std::string current_inst_module_name;
  struct PendingInstance {
    std::string instance_name;
    std::vector<Expr> ports_pos;
    std::map<std::string, Expr> ports_named;
  };
  std::vector<PendingInstance> pending_instances;

  Module current_module;
  std::vector<Module> modules_accum;
  bool in_module = false;
};

template<typename Rule>
struct action : tao::pegtl::nothing<Rule> {};

// ---------- identifiers ----------
template<> struct action<verilog::grammar::identifier> {
  template<typename Input>
  static void apply(const Input& in, State& st) {
    std::string id = in.string();
    if (!id.empty() && id[0]=='\\') id = id.substr(1);
    st.last_identifier = id;
    st.id_history.push_back(id);
    if (st.id_history.size() > 16)
      st.id_history.erase(st.id_history.begin(), st.id_history.begin() + (st.id_history.size()-16));
    if (st.decl_mode != State::DeclMode::None)
      st.decl_names.push_back(id);
  }
};

// ---------- numbers & ranges (only state wiring here) ----------
template<> struct action<verilog::grammar::base> {
  template<typename Input>
  static void apply(const Input& in, State& st) {
    char b = in.string().back();
    if (b >= 'A' && b <= 'Z') b = char(b - 'A' + 'a');
    st.number_base = b;
  }
};
template<> struct action<verilog::grammar::unsigned_hex_str> {
  template<typename Input>
  static void apply(const Input& in, State& st) { st.number_mantissa = in.string(); }
};
template<> struct action<verilog::grammar::signed_hex_str> {
  template<typename Input>
  static void apply(const Input& in, State& st) { st.number_mantissa = in.string(); st.number_base.reset(); }
};
template<> struct action<verilog::grammar::opt_range> {
  template<typename Input>
  static void apply(const Input& in, State& st) {
    if (in.string().empty()) { st.current_range.reset(); return; }
    Number a,b; a.mantissa="0"; b.mantissa="0";
    st.current_range = Range{a,b};
  }
};

// ---------- declarations — one entry per statement (matches your tests) ----------
template<> struct action<verilog::grammar::kw_wire>   { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Net;   st.decl_names.clear(); } };
template<> struct action<verilog::grammar::kw_input>  { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::In;    st.decl_names.clear(); } };
template<> struct action<verilog::grammar::kw_output> { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Out;   st.decl_names.clear(); } };
template<> struct action<verilog::grammar::kw_inout>  { template<typename I> static void apply(const I&, State& st){ st.decl_mode=State::DeclMode::Inout; st.decl_names.clear(); } };

template<> struct action<verilog::grammar::net_declaration> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.decl_names.empty())
      st.net_decl_accum.push_back( NetDeclaration{ st.decl_names.front(), st.current_range } );
    st.decl_names.clear(); st.decl_mode = State::DeclMode::None; st.current_range.reset();
  }
};
template<> struct action<verilog::grammar::input_declaration> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.decl_names.empty())
      st.in_decl_accum.push_back( InputDeclaration{ st.decl_names.front(), st.current_range } );
    st.decl_names.clear(); st.decl_mode = State::DeclMode::None; st.current_range.reset();
  }
};
template<> struct action<verilog::grammar::output_declaration> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.decl_names.empty())
      st.out_decl_accum.push_back( OutputDeclaration{ st.decl_names.front(), st.current_range } );
    st.decl_names.clear(); st.decl_mode = State::DeclMode::None; st.current_range.reset();
  }
};
template<> struct action<verilog::grammar::inout_declaration> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.decl_names.empty())
      st.inout_decl_accum.push_back( InoutDeclaration{ st.decl_names.front(), st.current_range } );
    st.decl_names.clear(); st.decl_mode = State::DeclMode::None; st.current_range.reset();
  }
};

// ---------- assignments ----------
template<> struct action<verilog::grammar::assignment> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (st.expr_stack.size() >= 2) {
      auto rhs = std::move(st.expr_stack.back()); st.expr_stack.pop_back();
      auto lhs = std::move(st.expr_stack.back()); st.expr_stack.pop_back();
      st.current_assign_list.emplace_back(std::move(lhs), std::move(rhs));
    } else if (st.id_history.size() >= 2) {
      auto rhs_id = st.id_history.back();
      auto lhs_id = st.id_history[st.id_history.size()-2];
      st.current_assign_list.emplace_back( Identifier{lhs_id}, Identifier{rhs_id} );
    }
  }
};
template<> struct action<verilog::grammar::continuous_assign> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.current_assign_list.empty()) {
      st.assign_accum.push_back( ContinuousAssign{ st.current_assign_list } );
      st.current_assign_list.clear();
    }
  }
};

// ---------- instantiation ----------
template<> struct action<verilog::grammar::module_inst_head> {
  template<typename Input>
  static void apply(const Input&, State& st) { st.current_inst_module_name = st.last_identifier; }
};
template<> struct action<verilog::grammar::module_instance> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    State::PendingInstance pi;
    pi.instance_name = st.last_identifier;
    st.pending_instances.emplace_back(std::move(pi));
  }
};

// ---------- module assembly ----------
template<> struct action<verilog::grammar::kw_module> {
  template<typename Input>
  static void apply(const Input&, State& st) { st.in_module = true; st.current_module = Module{}; }
};
template<> struct action<verilog::grammar::module_name_tok> {
  template<typename Input>
  static void apply(const Input& in, State& st) {
    std::string id = in.string();
    if (!id.empty() && id[0]=='\\') id = id.substr(1);
    st.current_module.module_name = id;
  }
};
template<> struct action<verilog::grammar::port_list> {
  template<typename Input>
  static void apply(const Input& in, State& st) {
    std::string s = in.string(); std::string tok;
    for (char c : s) {
      if (std::isalnum((unsigned char)c) || c=='_' || c=='$' || c=='\\') tok.push_back(c);
      else { if (!tok.empty()) { if (tok[0]=='\\') tok = tok.substr(1); st.current_module.port_list.push_back(tok); tok.clear(); } }
    }
    if (!tok.empty()) { if (tok[0]=='\\') tok = tok.substr(1); st.current_module.port_list.push_back(tok); }
  }
};
template<> struct action<verilog::grammar::module_item> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    if (!st.net_decl_accum.empty()) { st.current_module.net_declarations.insert(st.current_module.net_declarations.end(), st.net_decl_accum.begin(), st.net_decl_accum.end()); st.net_decl_accum.clear(); }
    if (!st.in_decl_accum.empty())  { st.current_module.input_declarations.insert(st.current_module.input_declarations.end(), st.in_decl_accum.begin(), st.in_decl_accum.end()); st.in_decl_accum.clear(); }
    if (!st.out_decl_accum.empty()) { st.current_module.output_declarations.insert(st.current_module.output_declarations.end(), st.out_decl_accum.begin(), st.out_decl_accum.end()); st.out_decl_accum.clear(); }
    if (!st.inout_decl_accum.empty()){ st.current_module.inout_declarations.insert(st.current_module.inout_declarations.end(), st.inout_decl_accum.begin(), st.inout_decl_accum.end()); st.inout_decl_accum.clear(); }
    if (!st.assign_accum.empty())   { st.current_module.assignments.insert(st.current_module.assignments.end(), st.assign_accum.begin(), st.assign_accum.end()); st.assign_accum.clear(); }
    if (!st.pending_instances.empty()) {
      for (auto& pi : st.pending_instances) { ModuleInstance mi; mi.module_name = st.current_inst_module_name; mi.instance_name = pi.instance_name; st.current_module.module_instances.emplace_back(std::move(mi)); }
      st.pending_instances.clear();
    }
  }
};
template<> struct action<verilog::grammar::module> {
  template<typename Input>
  static void apply(const Input&, State& st) {
    st.modules_accum.emplace_back(std::move(st.current_module));
    st.current_module = Module{}; st.in_module = false;
  }
};

}} // namespace verilog::actions
