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
  -f, --format <fmt>     Output format: json, xml, sysml (default: none)
  -I <path>              Add library search path for imports
      --fix              Format and rewrite files in place
  -P, --parse-only       Parse only, skip semantic validation
      --no-validate      Same as --parse-only
      --no-resolve       Disable automatic import resolution
  --color[=when]         Colorize output (auto, always, never)
  --max-errors <n>       Stop after n errors (default: 20)
  -W<warning>            Enable warning (e.g., -Werror)
  --dump-tokens          Dump lexer tokens
  --dump-ast             Dump parsed AST
  -v, --verbose          Verbose output
  -h, --help             Show help
  --version              Show version

Environment:
  SYSML2_LIBRARY_PATH    Colon-separated list of library search paths
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

## Semantic Validation

The parser performs semantic validation to catch errors beyond syntax:

### Error Types

| Code | Description |
|------|-------------|
| E3001 | Undefined type reference |
| E3004 | Duplicate definition in same scope |
| E3005 | Circular specialization chain |
| E3006 | Type compatibility mismatch |

### Cross-File Import Resolution

The parser supports two modes of cross-file import resolution:

#### Automatic Import Resolution (Recommended)

Use the `-I` flag to specify library search paths. The parser will automatically find and parse imported files:

```bash
# Specify library paths with -I
./sysml2 -I /path/to/library model.sysml

# Multiple library paths
./sysml2 -I /path/to/kernel -I /path/to/domain model.sysml

# Using environment variable
export SYSML2_LIBRARY_PATH="/path/to/kernel:/path/to/domain"
./sysml2 model.sysml
```

Example with verbose output:
```bash
$ ./sysml2 -v -I ./my-library test.sysml
note: added library path: /home/user/my-library
Processing: test.sysml
note: resolving import 'MyTypes' -> /home/user/my-library/MyTypes.sysml
```

The resolver searches library paths for files matching the package name:
- For `import Foo::*;`, searches for `Foo.sysml` or `Foo.kerml`
- Searches recursively in subdirectories (up to 5 levels deep)
- Caches parsed files to avoid re-parsing

Use `--no-resolve` to disable automatic resolution:
```bash
./sysml2 --no-resolve model.sysml  # Like the old behavior
```

#### Manual Multi-File Mode

Alternatively, provide all files explicitly on the command line:

```bash
# File A defines types, File B imports them
./sysml2 package_a.sysml package_b.sysml
```

```sysml
// package_a.sysml
package A {
    part def Engine;
    datatype Real;
}

// package_b.sysml
package B {
    import A::*;
    part car : Engine;      // Resolves via import
    attribute weight : Real; // Resolves via import
}
```

Supported import patterns:
- `import A::Engine;` - Direct element import
- `import A::*;` - Namespace import (all direct members)
- `import A::**;` - Recursive import (all nested members)

### Validation Options

```bash
./sysml2 model.sysml                  # Full validation (default)
./sysml2 --parse-only model.sysml     # Syntax check only, skip validation
./sysml2 --no-validate model.sysml    # Same as --parse-only
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
- Enumeration definitions
- Datatype definitions (KerML primitive types)

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
./test_validator        # Validator unit tests
ctest -R json_output    # JSON fixture tests
ctest -R validation     # Validation fixture tests
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
│   ├── json_writer.h       # JSON serialization
│   ├── import_resolver.h   # Automatic import resolution
│   └── validator.h         # Semantic validator
├── src/
│   ├── arena.c         # Arena allocator implementation
│   ├── intern.c        # String interning implementation
│   ├── keywords.c      # Keyword recognition
│   ├── lexer.c         # Lexer implementation
│   ├── diagnostic.c    # Diagnostic reporting
│   ├── ast.c           # AST utilities
│   ├── ast_builder.c   # AST builder implementation
│   ├── json_writer.c       # JSON writer implementation
│   ├── import_resolver.c   # Import resolution implementation
│   ├── validator.c         # Semantic validation
│   ├── main.c              # CLI entry point
│   └── sysml_parser.c  # PackCC-generated parser
├── grammar/
│   └── sysml.peg       # PEG grammar (source of truth)
├── tests/
│   ├── test_lexer.c           # Lexer unit tests
│   ├── test_ast.c             # AST/builder/JSON unit tests
│   ├── test_validator.c       # Validator unit tests
│   ├── test_packcc_parser.c   # Parser integration tests
│   ├── test_json_output.sh    # JSON output fixture tests
│   ├── test_validation.sh     # Validation fixture tests
│   └── fixtures/              # Test fixtures
│       ├── json/              # JSON output test pairs
│       ├── validation/        # Validation test cases
│       ├── official/          # Official SysML v2 examples
│       └── errors/            # Error case tests
└── CMakeLists.txt
```

## License

MIT License

## References

- [SysML v2 Release Repository](https://github.com/Systems-Modeling/SysML-v2-Release)
- [OMG SysML v2 Specification](https://www.omg.org/sysml/sysmlv2/)
