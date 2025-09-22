# Verilog Parser (C++ / PEGTL)

This repository provides a lightweight Verilog parser implemented in C++20 using [PEGTL](https://github.com/taocpp/PEGTL) and tested with GoogleTest. It is aimed at **structural Verilog** use‑cases: module headers, declarations, continuous assignments, and simple module instantiations.

> **Scope note**: This is a focused subset — it is *not* a complete Verilog-2005 parser.

---

## Overview

- **Language**: C++20
- **Parser**: PEGTL (Parsing Expression Grammar Template Library)
- **Testing**: GoogleTest
- **Folders**:  
  - `include/` – public API (`veriloglib.hpp`) and grammar/actions headers  
  - `src/` – implementation and CLI (`vparse`)  
  - `tests/` – unit tests (GoogleTest)

Build & test:
```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CLI:
```bash
./build/vparse path/to/file.v
```

---

## Public API

Declared in `include/veriloglib.hpp`.

### Data structures

```cpp
struct Number {
  std::optional<int>  length;     // optional size to the left of the base (e.g., 8'hFF) — parsed but not fully enforced
  std::optional<char> base;       // 'b'|'o'|'d'|'h' or std::nullopt
  std::string         mantissa;   // digits as written (no underscores supported)
  int64_t as_integer() const;     // best-effort integer conversion
};

struct Range {                     // [msb:lsb]
  Number start;
  Number end;
  std::vector<int64_t> to_indices() const; // msb..lsb (inclusive)
};

struct Identifier          { std::string name; };
struct IdentifierIndexed   { std::string name; Number index; };                 // a[3]
struct IdentifierSliced    { std::string name; Range range; };                  // a[7:0]
struct Concatenation;                                                          // {a, b[1], c[3:0]}
using Expr = std::variant<Identifier, IdentifierIndexed, IdentifierSliced, std::shared_ptr<Concatenation>>;
struct Concatenation { std::vector<Expr> elements; };

struct NetDeclaration   { std::string net_name; std::optional<Range> range; };
struct OutputDeclaration: NetDeclaration {};
struct InputDeclaration : NetDeclaration {};
struct InoutDeclaration : NetDeclaration {};

struct ContinuousAssign { std::vector<std::pair<Expr, Expr>> assignments; };

struct ModuleInstance {
  std::string module_name;        // type, e.g., "leaf"
  std::string instance_name;      // e.g., "u0"
  std::vector<Expr>         ports_pos;   // positional connections (if used)
  std::map<std::string,Expr> ports_named; // named connections (if used)
};

struct Module {
  std::string module_name;
  std::vector<std::string> port_list;            // from the header's ( ... )
  std::vector<NetDeclaration>    net_declarations;
  std::vector<OutputDeclaration> output_declarations;
  std::vector<InputDeclaration>  input_declarations;
  std::vector<InoutDeclaration>  inout_declarations;
  std::vector<ModuleInstance>    module_instances;
  std::vector<ContinuousAssign>  assignments;

  std::string summary() const; // human-readable dump
};

struct Netlist { std::vector<Module> modules; };
```

### Parse functions

```cpp
Netlist parse_string(std::string_view text);     // throws verilog::parse_error on failure
Netlist parse_file(const std::string& path);     // throws verilog::parse_error on failure
```

---

## Supported Verilog Syntax (Detailed)

This section documents **exactly** what the parser accepts. For a formal grammar, see the downloadable BNF file linked below.

### Modules
- Form:
  ```verilog
  module <identifier> ( <port_list_opt> );
    <module_item>*
  endmodule
  ```
- `<port_list_opt>` is zero or more identifiers separated by commas (no directions in the header — directions appear as declarations inside the body).

### Declarations
- Supported kinds (each ends with `;`):
  ```verilog
  input   [<msb>:<lsb>] <id> ( , <id> )* ;
  output  [<msb>:<lsb>] <id> ( , <id> )* ;
  inout   [<msb>:<lsb>] <id> ( , <id> )* ;
  wire    [<msb>:<lsb>] <id> ( , <id> )* ;
  ```
- Notes:
  - Range bounds are numbers (see **Numbers**). Expressions in ranges are **not** supported.
  - Internally (per tests), each statement is recorded as **one** declaration entry (names collapsed).

### Continuous assignments
- Form:
  ```verilog
  assign <lhs>=<rhs> ( , <lhs>=<rhs> )* ;
  ```
- `<lhs>` / `<rhs>` are limited to the **expression forms** below (no operators).

### Module instantiation
- Forms:
  ```verilog
  <module_type> <instance_name> ( <port_connections_opt> ) ;
  ```
  where `<port_connections_opt>` is one of:
  - Positional: `expr , expr , ...`
  - Named: `.port_name( expr ) , ...`
- Mixing positional and named is **not validated** (the grammar allows a mix; semantic checks are out of scope).

### Expressions (subset)
- Allowed primary expressions:
  - Identifier: `a`
  - Bit-select: `a[<number>]`
  - Part-select: `a[<msb>:<lsb>]` (numeric bounds only)
  - Concatenation: `{ expr , expr , ... }`
- **Not supported**: unary/binary operators (`~ & | ^ + - * / % << >>`, comparisons, ternary, parentheses for grouping, numeric literals as standalone expressions).

### Identifiers
- Normal identifiers: `[A-Za-z_][A-Za-z0-9_$]*`
- Escaped identifiers (Verilog-2005 style): `\` followed by a run of non‑whitespace characters excluding `[]{}().,;=` (trailing whitespace ends the identifier).

### Numbers
- Supported forms (subset):
  - Unsized unsigned decimal-like *hex-digit* sequence: `[0-9A-Fa-f]+`
  - Signed variant: `[+-][0-9A-Fa-f]+`
  - Based numbers with optional size:  
    - `<size>'<base><digits>` where
      - `<size>` = `[0-9A-Fa-f]+` (parsed but not enforced),
      - `<base>` = `b|B|o|O|d|D|h|H`,
      - `<digits>` = `[0-9A-Fa-f]+` (no `_ x z ?` in this subset).
- Numbers are permitted **only** in ranges and indices, not as standalone expressions on the RHS/LHS of assignments.

### Comments & whitespace
- Line comments: `// ...` to end of line
- Block comments: `/* ... */`
- Attribute-style blocks (treated as comments): `(* ... *)`
- Whitespace/comments may appear between all tokens.

### Known limitations
- No procedural blocks (`always`, `initial`), no parameters/defparams, no generate blocks, no attributes binding semantics, no operators in expressions, no numeric literals as standalone expressions, no multi-dimensional arrays, no tasks/functions, no time/scaling, no `assign` drive strengths/delays.

---

## Formal Grammar (BNF)

A full BNF of the supported subset is provided as a separate file for tooling and reference:

**➡️ Download:** [verilog_subset.bnf](verilog_subset.bnf)
