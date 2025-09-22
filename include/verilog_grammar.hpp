#pragma once
#include <tao/pegtl.hpp>

namespace verilog { namespace grammar {
using namespace tao::pegtl;

// Whitespace/comments
struct space : sor< one<' '>, one<'\t'>, one<'\r'>, one<'\n'> > {};
struct line_comment : if_must< string<'/','/'>, until< eolf > > {};
struct block_comment1 : if_must< string<'/','*'>, until< string<'*','/'> > > {};
struct block_comment2 : if_must< string<'(','*'>, until< string<'*',')'> > > {};
struct sep  : star< sor< space, line_comment, block_comment1, block_comment2 > > {};
struct seps : plus< sor< space, line_comment, block_comment1, block_comment2 > > {};

// Identifiers
struct ident_start : sor< alpha, one<'_'> > {};
struct ident_more  : sor< alnum, one<'_','$'> > {};
struct ident_norm  : seq< ident_start, star< ident_more > > {};
struct ident_esc   : seq< one<'\\'>, plus< not_one<' ','\t','\r','\n','[',']','{','}','(',')','.',',',';','='> > > {};
struct identifier  : sor< ident_esc, ident_norm > {};

// Numbers
struct HEXDIG : ranges<'0','9','a','f','A','F'> {};
struct unsigned_hex_str : plus< HEXDIG > {};
struct sign : one<'+','-'> {};
struct signed_hex_str : seq< sign, unsigned_hex_str > {};
struct base_char : one<'b','B','h','H','o','O','d','D'> {};
struct base : seq< one<'\''>, base_char > {};
struct number_1 : sor<
  unsigned_hex_str,
  signed_hex_str,
  seq< unsigned_hex_str, base, unsigned_hex_str >,
  seq< base, unsigned_hex_str >
> {};

// Ranges
struct lbrack : one<'['> {}; struct rbrack : one<']'> {}; struct colon : one<':'> {};
struct range_core : if_must< lbrack, number_1, colon, number_1, rbrack > {};

// Expr (subset)
struct lbrace : one<'{'> {}; struct rbrace : one<'}'> {};
struct lparen : one<'('> {}; struct rparen : one<')'> {};
struct dot    : one<'.'> {}; struct comma  : one<','> {};
struct semi   : one<';'> {}; struct equal  : one<'='> {};
struct expression;
struct ident_indexed : seq< identifier, sep, lbrack, sep, number_1, sep, rbrack > {};
struct ident_sliced  : seq< identifier, sep, range_core > {};
struct concat_list;
struct concat : seq< lbrace, sep, concat_list, sep, rbrace > {};
struct primary_expr : sor< concat, ident_sliced, ident_indexed, identifier > {};
struct expression : primary_expr {};
struct concat_list : list_must< expression, seq< sep, comma, sep > > {};

// Ports
struct port_list : list< identifier, seq< sep, comma, sep > > {};

// Keywords
struct kw_wire   : TAO_PEGTL_STRING("wire") {};
struct kw_input  : TAO_PEGTL_STRING("input") {};
struct kw_output : TAO_PEGTL_STRING("output") {};
struct kw_inout  : TAO_PEGTL_STRING("inout") {};
struct kw_assign : TAO_PEGTL_STRING("assign") {};
struct kw_module : TAO_PEGTL_STRING("module") {};
struct kw_endmodule : TAO_PEGTL_STRING("endmodule") {};
struct not_keyword : not_at< sor< kw_wire, kw_input, kw_output, kw_inout, kw_assign, kw_module, kw_endmodule > > {};

struct list_of_variables : list_must< identifier, seq< sep, comma, sep > > {};
struct opt_range : opt< seq< sep, range_core, sep > > {};

struct net_declaration    : if_must< kw_wire,   seps, opt_range, list_of_variables, sep, semi > {};
struct input_declaration  : if_must< kw_input,  seps, opt_range, list_of_variables, sep, semi > {};
struct output_declaration : if_must< kw_output, seps, opt_range, list_of_variables, sep, semi > {};
struct inout_declaration  : if_must< kw_inout,  seps, opt_range, list_of_variables, sep, semi > {};

// assign
struct assignment : if_must< expression, sep, equal, sep, expression > {};
struct assignment_list : list_must< assignment, seq< sep, comma, sep > > {};
struct continuous_assign : if_must< kw_assign, seps, assignment_list, sep, semi > {};

// instantiation
struct module_port_connection : expression {};
struct named_port_connection : if_must< dot, identifier, sep, lparen, sep, expression, sep, rparen > {};
struct list_of_module_connections : list_must< sor< named_port_connection, module_port_connection >, seq< sep, comma, sep > > {};
struct module_instance : if_must< identifier, sep, lparen, sep, opt< list_of_module_connections >, sep, rparen > {};
struct module_instance_list : list_must< module_instance, seq< sep, comma, sep > > {};
struct module_inst_head : identifier {};
struct module_instantiation : if_must< not_keyword, module_inst_head, seps, module_instance_list, sep, semi > {};

// module
struct module_header_ports : if_must< lparen, sep, opt< port_list >, sep, rparen > {};
struct module_name_tok : identifier {};
struct module_header : if_must< kw_module, seps, module_name_tok, sep, opt< module_header_ports >, sep, semi > {};
struct module_item : sor< input_declaration, output_declaration, inout_declaration, net_declaration, continuous_assign, module_instantiation > {};
struct module : seq< module_header, star< seq< sep, module_item > >, sep, kw_endmodule > {};
struct start : star< seq< sep, module, sep > > {};

}} // namespace verilog::grammar
