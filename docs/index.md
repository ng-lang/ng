---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "NG"
  text: "A NostalGic Programming Language"
  tagline: "Statically-typed · Multi-paradigm · Modern C++23 implementation"
  image:
    src: /logo.svg
    alt: NG Logo
  actions:
    - theme: brand
      text: Get Started
      link: /guide/getting-started
    - theme: alt
      text: Language Guide
      link: /guide/language_guide
    - theme: alt
      text: View on GitHub
      link: https://github.com/ng-lang/ng

features:
  - icon: 🎯
    title: Rich Type System
    details: Generics, traits, tagged unions, nominal types, higher-kinded types — express your domain with confidence.
  - icon: 🔒
    title: Safe by Default
    details: Prevents null pointer dereferences, buffer overflows, and use-after-move errors at compile time.
  - icon: ⚡
    title: One Clean Pipeline
    details: Lexer → Parser → Resolver → Type Checker → FlowIR → Bytecode → VM — one implementation, no duplicated semantics.
  - icon: 🔧
    title: Compile-Time Metaprogramming
    details: Const if, const predicates, const functions, and type specialization — compute at compile time, not at runtime.
  - icon: 🖥️
    title: Native FFI
    details: Seamless C++ function binding. Call into existing C/C++ libraries with zero overhead.
  - icon: 🎨
    title: ImGui Integration
    details: Build immediate-mode GUI applications using Dear ImGui, directly from NG code.
  - icon: 🧩
    title: Ownership Model
    details: Value semantics, references, moves, and partial moves — fine-grained control over memory without a garbage collector.
  - icon: 📦
    title: Module System
    details: Export/import visibility, clean namespace management. Every file is a module.
  - icon: 🚀
    title: Verified Bytecode VM
    details: Versioned, verified bytecode with per-run fuel budgets, trait-view dispatch tables, and re-entrant compilation (`runNgi`).
---
