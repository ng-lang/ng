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
      { text: 'Reference', link: '/ref/Internals', activeMatch: '/ref/' },
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
            { text: 'ImGui Integration', link: '/guide/imgui-integration' },
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
