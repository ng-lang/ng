---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "NG"
  text: "The NG Programming Language"
  tagline: "Statically-typed · Multi-paradigm · Modern implementation"
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
    details: Generics, traits, tagged unions, and ownership model — express your domain with confidence and safety.
  - icon: ⚡
    title: One Clean Pipeline
    details: Lexer → Parser → Resolver → Type Checker → FlowIR → QBE IL → native executable — one implementation, no duplicated semantics.
  - icon: 🚀
    title: Native Code Generation
    details: Compile to native executables with `--output <path>`. Produces compact binaries with minimal runtime overhead.
  - icon: 🔧
    title: Compile-Time Programming
    details: Const if, const functions, and type specialization — compute at compile time, not at runtime.
---
