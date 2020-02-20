# Overview

## Phases of Compilation

The process of compiling is divided into multiple phases, each phases has no dependence
on subsequent phases. This seperation of the passes makes language tools relatively easy to
produce. It is also possible to compress D source by storing it in tokenized form.

 1. Lexical analysis
 2. Syntax analysis
 3. Semantic analysis
 4. Optimization
 5. Code generation
