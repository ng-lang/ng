# Symbol Import Aliases

## Status

Future design. This is intentionally separated from current module artifact,
stdlib modularization, and bytecode loading work. Current implemented conflict
avoidance is module-qualified access:

```ng
import first.module as first;
import second.module as second;

first.name();
second.name();
```

## Goal

Allow callers to keep short local names when two modules export symbols with the
same public name, without weakening import provenance or trait/impl coherence.

## Proposed Syntax

```ebnf
ImportDecl := ExportPrefix? "import" ModulePath ImportAlias? ImportList? ";"
ExportPrefix := "export"
ImportAlias := "as" Ident
ImportList := "(" ("*" | ImportItem ("," ImportItem)*) ")"
ImportItem := Ident ("as" Ident)?
```

Examples:

```ng
import first.module (name as firstName);
import second.module (name as secondName);

export import std.string (join as stringJoin);
```

## Semantics

- `import m (aaa as bbb);` binds local name `bbb` to exported symbol `aaa`.
- Provenance records the canonical source module ID and original exported name
  separately from the local alias.
- Conflict checks are local-name based. `aaa as bbb` conflicts with any other
  imported or local symbol named `bbb` from a different source.
- Re-importing the same original symbol from the same canonical source under the
  same local alias is idempotent.
- Diagnostics must report the local alias, original symbol, and canonical source
  module.
- `export import m (aaa as bbb);` exports `bbb` as this module's public symbol,
  while preserving provenance to `m::aaa`.
- Module-qualified imports remain available and are preferred when many symbols
  from both modules are needed.
- `use impl Trait for Type` continues to use module qualifiers and canonical
  module IDs. Symbol aliases do not create new trait or type identities.

## Required Data

- AST import item structure with `exportedName` and optional `localName`.
- Runtime/module artifact import provenance entries containing:
  local name, original exported name, canonical source module ID.
- Typechecker import metadata that distinguishes local alias from original
  export name for diagnostics and re-export artifact publication.
- ORGASM import metadata that can emit `CALL_IMPORT` by original exported name
  while binding the local alias during compilation.

## Acceptance Criteria

- Parser accepts `import m (aaa as bbb);` and rejects malformed alias items.
- STUPID and ORGASM both execute calls through aliased imported functions.
- Imported values, functions, types, traits, and impl evidence preserve original
  module provenance through aliases.
- Conflicts are detected by local name and include both source modules in the
  diagnostic.
- `export import m (aaa as bbb);` re-exports `bbb` and downstream imports can use
  `bbb` without knowing the original `aaa` name.
- `.ngo` artifacts preserve alias metadata so source and bytecode modules expose
  the same public symbols.
