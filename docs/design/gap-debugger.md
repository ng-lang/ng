# Debugger: DAP Adapter And Runtime Inspection

## Order

Recommended implementation order: **7**.

## Goal

Provide a debugging experience for NG through the Debug Adapter Protocol (DAP), enabling breakpoints, step-through execution, variable inspection, and call stack navigation.

## Motivation

Currently, debugging NG code means adding `print()` statements. There is no:
- Breakpoint support
- Step-over / step-into / step-out
- Variable watch / inspection
- Call stack visualization
- Conditional breakpoints

## Proposed Design

### Architecture

```
Editor (VS Code)  ←DAP→  ng-dap  ←VM→  ngi / ORGASM VM
```

The DAP server sits between the editor and the runtime, translating debugging commands into VM operations.

### Key Capabilities

| Feature | DAP Request | Implementation |
|---|---|---|
| Set breakpoint | `setBreakpoints` | Patch bytecode at target location with `DEBUG_BREAK` opcode |
| Continue | `continue` | Resume VM execution |
| Next (step over) | `next` | Run until next line in same frame |
| Step in | `stepIn` | Run until entering a new frame |
| Step out | `stepOut` | Run until returning from current frame |
| Stack trace | `stackTrace` | Read VM call frame list |
| Variables | `variables` | Read local and global slot values |
| Evaluate | `evaluate` | Execute NG expression in current scope |
| Disconnect | `disconnect` | Terminate debug session |

### VM Changes

New ORGASM opcode: `DEBUG_BREAK`

```cpp
case Opcode::DEBUG_BREAK:
    // Suspend VM, notify DAP, wait for continue/step command
    vm->suspendAndWait();
```

When a breakpoint is hit:
1. VM state (registers, stack, frames) is frozen
2. DAP sends `stopped` event to editor
3. Editor requests stack trace, variables, etc.
4. User chooses continue / step
5. VM resumes from frozen state

### Source Mapping

The compiler emits a source map that maps bytecode addresses back to source locations:

```json
{
  "version": 1,
  "file": "main.ng",
  "mappings": [
    {"addr": 0, "line": 1, "col": 1},
    {"addr": 12, "line": 3, "col": 5},
    ...
  ]
}
```

### Conditional Breakpoints

```ng
// In editor: set breakpoint at line 10 with condition "x > 5"
// VM executes: push condition, JUMP_IF_FALSE past DEBUG_BREAK
```

### Watch Expressions

```ng
// User types "x + y" in watch panel
// VM evaluates the expression in the current frame's scope
// Result is displayed in the watch panel
```

## Dependencies

- Requires source map emission from the ORGASM compiler (mostly exists in debug output).
- Requires opcode patching in the VM (no existing infrastructure for this).
- Unblocks: production debugging, bug reproduction.

## Scope

**In scope:**
- DAP server (`ng-dap`) as a new executable
- `DEBUG_BREAK` opcode
- Source map generation
- Breakpoint, step, continue support
- Variable inspection (locals, globals, stack)
- Call stack visualization
- Evaluate expression in debug context
- VS Code launch configuration template

**Out of scope:**
- Hot reload / edit-and-continue
- Reverse debugging / time travel
- Memory inspection (hex view)
- Multi-threaded debugging (deferred until concurrency exists)
- Profiling integration

## Acceptance Criteria

- `ng-dap` starts and responds to DAP initialize handshake
- A breakpoint set in VS Code stops execution at the correct line
- Step-over advances to the next line in the same function
- Step-into enters a called function
- Variables view shows local variables with correct values
- Call stack shows all active frames with correct source locations
- An evaluated expression returns the correct typed value
- Disconnecting cleanly terminates the VM
- Debugging does not affect program behavior outside breakpoints

## Potential Challenges

- The VM is currently a monolithic execute loop — cleanly suspending and resuming requires refactoring.
- The STUPID interpreter has even less infrastructure for debugging (no bytecode addresses).
- Source maps must be generated and attached to bytecode modules.
- Variable inspection requires mapping VM slot indices back to source-level variable names.
- Evaluate expressions in debug context requires a lightweight parse + check + execute path.
- DAP specification is large (~150 requests/events) — MVP covers ~20 key ones.