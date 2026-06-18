import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: 'NG Language',
  titleTemplate: ':title — NG Programming Language',
  description: 'A nostalgic, statically-typed, multi-paradigm programming language',

  appearance: 'dark',

  // Clean URLs
  cleanUrls: true,

  lastUpdated: true,

  head: [
    ['link', { rel: 'icon', href: '/logo.svg' }],
    ['meta', { property: 'og:type', content: 'website' }],
    ['meta', { property: 'og:title', content: 'NG Programming Language' }],
    ['meta', { property: 'og:description', content: 'A nostalgic, statically-typed, multi-paradigm programming language' }],
  ],

  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    search: {
      provider: 'local',
    },

    logo: '/logo.svg',

    nav: [
      { text: 'Guide', link: '/guide/language_guide', activeMatch: '/guide/' },
      { text: 'Design', link: '/design/README', activeMatch: '/design/' },
      { text: 'Reference', link: '/ref/Internals', activeMatch: '/ref/' },
      {
        text: 'More',
        items: [
          { text: 'Code Review (2026-06)', link: '/code_review_2026_06' },
          { text: 'Refactoring Plan (2026-06)', link: '/refactoring_plan_2026_06' },
        ],
      },
      {
        text: 'Resources',
        items: [
          { text: 'GitHub', link: 'https://github.com/ng-lang/ng' },
          { text: 'Issues', link: 'https://github.com/ng-lang/ng/issues' },
          { text: 'Discussions', link: 'https://github.com/ng-lang/ng/discussions' },
        ],
      },
    ],

    sidebar: {
      '/guide/': [
        {
          text: 'Guide',
          items: [
            { text: 'Language Guide (Overview)', link: '/guide/language_guide' },
            { text: 'Getting Started', link: '/guide/getting-started' },
            { text: 'Basic Syntax', link: '/guide/basic-syntax' },
            { text: 'Control Flow', link: '/guide/control-flow' },
            { text: 'Functions', link: '/guide/functions' },
            { text: 'Data Structures', link: '/guide/data-structures' },
            { text: 'Modules and Imports', link: '/guide/modules-and-imports' },
            { text: 'Generics', link: '/guide/generics' },
            { text: 'References, Moves & Ownership', link: '/guide/references-moves' },
            { text: 'Traits', link: '/guide/traits' },
            { text: 'Type System in Depth', link: '/guide/type-system-in-depth' },
            { text: 'Compile-Time Programming', link: '/guide/compile-time-programming' },
            { text: 'Advanced Generics', link: '/guide/advanced-generics' },
            { text: 'Standard Library', link: '/guide/standard-library' },
            { text: 'Memory Management', link: '/guide/memory-management' },
            { text: 'ORGASM Backend', link: '/guide/orgasm-backend' },
            { text: 'ImGui Integration', link: '/guide/imgui-integration' },
          ],
        },
      ],

      '/design/': [
        {
          text: 'Design Documents',
          link: '/design/README',
          items: [
            {
              text: 'Active Follow-Ups',
              collapsed: false,
              items: [
                { text: 'Enhanced Tuple Types', link: '/design/enhanced_tuples' },
                { text: 'Auto & Derive Traits', link: '/design/auto_derive_traits' },
                { text: 'Ranges, Slicing & Pipeline', link: '/design/ranges_slicing_pipeline' },
                { text: 'Symbol Import Aliases', link: '/design/symbol_import_aliases' },
              ],
            },
            {
              text: 'Gap Proposals',
              collapsed: false,
              items: [
                { text: 'Error Handling', link: '/design/gap-error-handling' },
                { text: 'Test Framework', link: '/design/gap-test-framework' },
                { text: 'Build System', link: '/design/gap-build-system' },
                { text: 'C FFI', link: '/design/gap-c-ffi' },
                { text: 'Package Manager', link: '/design/gap-package-manager' },
                { text: 'Standard Library Expansion', link: '/design/gap-stdlib-expansion' },
                { text: 'Runtime Optimization', link: '/design/gap-runtime-optimization' },
                { text: 'Concurrency', link: '/design/gap-concurrency' },
                { text: 'Type System Enhancements', link: '/design/gap-type-system-enhancements' },
                { text: 'Syntax Ergonomics', link: '/design/gap-syntax-ergonomics' },
                { text: 'LSP / IDE Support', link: '/design/gap-lsp-ide' },
                { text: 'Formatter', link: '/design/gap-formatter' },
                { text: 'Documentation Generator', link: '/design/gap-docgen' },
                { text: 'Debugger', link: '/design/gap-debugger' },
                { text: 'Community Infrastructure', link: '/design/gap-community-infrastructure' },
              ],
            },
            {
              text: 'Archived Designs',
              collapsed: true,
              items: [
                { text: 'Auto Derive Traits (Baseline)', link: '/design/archive/auto_derive_traits_baseline' },
                { text: 'Bytecode Module Loading', link: '/design/archive/bytecode_module_loading' },
                { text: 'Constant Generic Parameters', link: '/design/archive/constant_generic_parameters' },
                { text: 'Const Functions', link: '/design/archive/const_fun' },
                { text: 'Enhanced Tuples (Baseline)', link: '/design/archive/enhanced_tuples_baseline' },
                { text: 'Generalized Delete', link: '/design/archive/generalized_delete' },
                { text: 'Module Artifact Typechecker', link: '/design/archive/module_artifact_typechecker' },
                { text: 'Module System', link: '/design/archive/module_system' },
                { text: 'Native Module Artifacts', link: '/design/archive/native_module_artifacts' },
                { text: 'Partial Move Semantics', link: '/design/archive/partial_move_semantics' },
                { text: 'Ranges Slicing Pipeline (Baseline)', link: '/design/archive/ranges_slicing_pipeline_baseline' },
                { text: 'Stdlib Modularization', link: '/design/archive/stdlib_modularization' },
                { text: 'Tuples', link: '/design/archive/tuples' },
              ],
            },
          ],
        },
      ],

      '/ref/': [
        {
          text: 'Reference',
          items: [
            { text: 'Internals', link: '/ref/Internals' },
            { text: 'Memory', link: '/ref/Memory' },
            { text: 'C++ Compatibility', link: '/ref/cxx-compatibility' },
          ],
        },
      ],
    },

    socialLinks: [
      { icon: 'github', link: 'https://github.com/ng-lang/ng' },
    ],

    footer: {
      message: 'Made with ❤️ by the NG community.',
      copyright: 'Copyright © 2026 NG Language Contributors',
    },

    outline: {
      level: [2, 3],
    },

    editLink: {
      pattern: 'https://github.com/ng-lang/ng/edit/main/docs/:path',
      text: 'Edit this page on GitHub',
    },
  },
})
