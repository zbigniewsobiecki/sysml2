# SysML v2 Parser

A state-of-the-art SysML v2 and KerML parser CLI written in C for validating `.sysml` and `.kerml` files with helpful, actionable error messages.

## Features

- **Hand-written recursive descent parser** - Better error recovery, no dependencies
- **Arena memory allocation** - Fast allocation/deallocation, cache-friendly
- **Clang-style diagnostics** - Clear error messages with source context and suggestions
- **KerML and SysML v2 support** - Parses both language layers
- **JSON semantic graph output** - Elements and relationships for visualization tools
- **Semantic analysis** - Detects undefined references, duplicates, type errors

## Building

Requirements:
- CMake 3.16+
- C11 compiler (GCC, Clang, or MSVC)

```bash
mkdir build && cd build
cmake ..
make
```

For debug builds:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

## Usage

```
sysml2 - SysML v2 Parser and Validator

Usage: sysml2 [options] <file>...

Options:
  -o, --output <file>    Write output to file
  -f, --format <fmt>     Output format: json, xml (default: none)
  --color[=when]         Colorize output (auto, always, never)
  --max-errors <n>       Stop after n errors (default: 20)
  -W<warning>            Enable warning (e.g., -Werror)
  --dump-tokens          Dump lexer tokens
  --dump-ast             Dump parsed AST
  -v, --verbose          Verbose output
  -h, --help             Show help
  --version              Show version
```

### Examples

Validate a KerML file:
```bash
./sysml2 model.kerml
```

Parse and output JSON semantic graph:
```bash
./sysml2 -f json model.sysml > model.json
```

The JSON output includes elements (packages, definitions, usages) and relationships:
```json
{
  "meta": { "version": "1.0", "source": "model.sysml" },
  "elements": [
    { "id": "Pkg::Part", "name": "Part", "type": "PartDef", "parent": "Pkg" }
  ],
  "relationships": []
}
```

Show lexer tokens (for debugging):
```bash
./sysml2 --dump-tokens file.kerml
```

## Error Messages

The parser provides Clang-style error messages with source context:

```
model.kerml:15:23: error[E2001]: expected ';' after feature declaration
   |
15 |     feature engine : Engine
   |                           ^ expected ';'
   |
   = help: add ';' to complete the declaration

model.kerml:20:17: error[E3001]: undefined type 'Engin'
   |
20 |     feature x : Engin;
   |                 ^^^^^ not found
   |
   = help: did you mean 'Engine'?
```

## Supported Language Features

### KerML
- Namespaces and packages
- Types, classifiers, classes, datatypes, structs
- Features with direction (in/out/inout)
- Specialization (`:>`), subsetting (`::>`), redefinition (`:>>`)
- Multiplicity (`[0..1]`, `[*]`)
- Associations, behaviors, functions, predicates
- Comments and documentation

### SysML v2
- Part definitions and usages
- Action definitions and usages
- State definitions and usages
- Requirement and constraint definitions
- Port definitions
- Interface definitions
- Item definitions
- Attribute definitions

## Testing

Run the test suite:
```bash
cd build
ctest --output-on-failure
```

Or use the check target:
```bash
ninja check   # or: make check
```

Run individual test groups:
```bash
./test_lexer            # Lexer unit tests
./test_ast              # AST/builder/JSON unit tests
ctest -R json_output    # JSON fixture tests
```

## Project Structure

```
sysml2/
├── include/sysml2/     # Header files
│   ├── common.h        # Common types and macros
│   ├── arena.h         # Arena allocator
│   ├── intern.h        # String interning
│   ├── token.h         # Token definitions
│   ├── lexer.h         # Lexer interface
│   ├── diagnostic.h    # Error reporting
│   ├── cli.h           # CLI options
│   ├── ast.h           # AST node types
│   ├── ast_builder.h   # AST builder context
│   └── json_writer.h   # JSON serialization
├── src/
│   ├── arena.c         # Arena allocator implementation
│   ├── intern.c        # String interning implementation
│   ├── keywords.c      # Keyword recognition
│   ├── lexer.c         # Lexer implementation
│   ├── diagnostic.c    # Diagnostic reporting
│   ├── ast.c           # AST utilities
│   ├── ast_builder.c   # AST builder implementation
│   ├── json_writer.c   # JSON writer implementation
│   ├── main.c          # CLI entry point
│   └── sysml_parser.c  # PackCC-generated parser
├── grammar/
│   └── sysml.peg       # PEG grammar (source of truth)
├── tests/
│   ├── test_lexer.c           # Lexer unit tests
│   ├── test_ast.c             # AST/builder/JSON unit tests
│   ├── test_packcc_parser.c   # Parser integration tests
│   ├── test_json_output.sh    # JSON output fixture tests
│   └── fixtures/              # Test fixtures
│       ├── json/              # JSON output test pairs
│       ├── official/          # Official SysML v2 examples
│       └── errors/            # Error case tests
└── CMakeLists.txt
```

## License

MIT License

## References

- [SysML v2 Release Repository](https://github.com/Systems-Modeling/SysML-v2-Release)
- [OMG SysML v2 Specification](https://www.omg.org/sysml/sysmlv2/)
