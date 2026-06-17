# Debugger (DAP Adapter) — VM Interface Specification

> **Status:** Refined with VM suspension interface and source map format.

## VM Debug Interface

The VM exposes a debugging interface that the DAP server calls:

```cpp
// include/orgasm/debug_interface.hpp

namespace NG::orgasm {

struct DebugBreakpoint {
    std::string file;
    uint32_t line;
    std::optional<std::string> condition;  // Optional conditional breakpoint
};

struct DebugFrame {
    std::string functionName;
    std::string file;
    uint32_t line;
    uint32_t column;
    uint32_t frameId;
};

struct DebugVariable {
    std::string name;
    std::string type;
    std::string value;         // Human-readable string representation
    // For composite types, children are retrieved lazily:
    std::optional<uint32_t> variablesReference;
};

enum class DebugCommand {
    CONTINUE,
    STEP_OVER,
    STEP_IN,
    STEP_OUT,
    STOP,      // Stop debugging session
};

enum class DebugEvent {
    BREAKPOINT_HIT,
    STEP_COMPLETE,
    EXCEPTION,
    EXIT,
    USER_BREAK,  // Ctrl+C or SIGINT
};

struct DebugStatus {
    DebugEvent event;
    uint32_t threadId = 0;
    std::optional<std::string> exceptionMessage;
};

class DebugInterface {
public:
    virtual ~DebugInterface() = default;

    // Called by DAP server:
    virtual void setBreakpoints(const std::vector<DebugBreakpoint> &bps) = 0;
    virtual void sendCommand(DebugCommand cmd) = 0;
    virtual std::vector<DebugFrame> getStackTrace() = 0;
    virtual std::vector<DebugVariable> getVariables(uint32_t frameId) = 0;
    virtual std::optional<std::string> evaluate(const std::string &expr, uint32_t frameId) = 0;

    // Called by VM when a debug event occurs:
    virtual void onDebugEvent(const DebugStatus &status) = 0;
};

} // namespace NG::orgasm
```

## VM Implementation

### Opcode: `DEBUG_BREAK`

```cpp
// In src/orgasm/VM.cpp

Opcode::DEBUG_BREAK:
    if (debugger) {
        debugger->onDebugEvent(DebugEvent::BREAKPOINT_HIT);
        debugger->waitForCommand();  // Block until CONTINUE/STEP
    }
    break;
```

### Breakpoint Patching

The DAP server patches bytecode to insert `DEBUG_BREAK` opcodes:

```cpp
void DebugInterface::setBreakpoints(const std::vector<DebugBreakpoint> &bps) {
    // 1. Clear all existing breakpoints (restore original opcodes)
    for (auto &bp : activeBreakpoints) {
        bytecode[bp.address] = bp.originalOpcode;
    }
    
    // 2. Map file:line to bytecode addresses using source map
    auto addresses = sourceMap->resolve(bp.file, bp.line);
    
    // 3. Insert DEBUG_BREAK at each address
    for (auto addr : addresses) {
        activeBreakpoints.push_back({
            .address = addr,
            .originalOpcode = bytecode[addr],
        });
        bytecode[addr] = Opcode::DEBUG_BREAK;
    }
}
```

### Source Suspension

The VM suspends execution by blocking on a condition variable:

```cpp
void DebugInterface::waitForCommand() {
    std::unique_lock lock(mutex);
    cv.wait(lock, [this] { return pendingCommand.has_value(); });
    // pendingCommand is set by sendCommand() from the DAP thread
}
```

## Source Map Format

The compiler emits a source map alongside bytecode:

```cpp
struct SourceMapEntry {
    uint32_t bytecodeAddress;  // Offset in bytecode
    uint32_t sourceLine;       // 1-based line in .ng source
    uint32_t sourceColumn;     // 0-based column
    std::string sourceFile;    // Source file path (for multi-module programs)
};

struct SourceMap {
    std::string compileUnitName;
    std::vector<SourceMapEntry> entries;
    
    // Binary format for embedding in .ngo:
    void serialize(std::ostream &os) const;
    static SourceMap deserialize(std::istream &is);
};
```

### Binary Format

```
[4 bytes] Magic: "NGSM"
[4 bytes] Number of entries (N)
For each entry:
  [4 bytes] bytecodeAddress (little-endian)
  [4 bytes] sourceLine (little-endian)
  [4 bytes] sourceColumn (little-endian)
  [2 bytes] sourceFile string length (L)
  [L bytes] sourceFile (UTF-8, not null-terminated)
```

### Compiler Changes (in `src/orgasm/Compiler.cpp`)

The compiler already tracks source positions for error messages. This information is now serialized:

```cpp
void Compiler::emitSourceMapEntry(uint32_t address, const SourcePosition &pos) {
    if (emitSourceMap) {
        sourceMap.entries.push_back({
            .bytecodeAddress = address,
            .sourceLine = pos.line,
            .sourceColumn = pos.col,
            .sourceFile = currentFilename,
        });
    }
}
```

## DAP Server (`ng-dap`)

### Message Flow (MVP)

```
1. Editor sends: initialize
   Server responds: capabilities (supports: setBreakpoints, stackTrace, variables, continue, next, stepIn, stepOut)

2. Editor sends: initialized
   Server responds: (waiting for launch/attach)

3. Editor sends: launch { program: "my_app.ng" }
   Server: starts VM, runs until first breakpoint or end

4. Editor sends: setBreakpoints { file: "main.ng", lines: [10, 15] }
   Server: patches bytecode at lines 10 and 15

5. VM hits breakpoint → Server sends: stopped { reason: "breakpoint", threadId: 1 }

6. Editor sends: stackTrace { threadId: 1 }
   Server responds: { stackFrames: [...] }

7. Editor sends: variables { variablesReference: 1 }
   Server responds: { variables: [...] }

8. Editor sends: continue
   Server: resumes VM

9. VM exits → Server sends: exited { exitCode: 0 }
   Server sends: terminated
```

### Implementation

```
src/dap/
├── main.cpp              # Entry point (runs TCP server on port)
├── DAPServer.cpp/h       # DAP protocol handler
├── Session.cpp/h         # Per-editor session state
├── Protocol.cpp/h        # JSON-RPC message parsing
└── VMAdapter.cpp/h       # Bridge between DAP and VM DebugInterface
```

### Effort Estimate

| Phase | Component | Effort |
|---|---|---|
| **Phase 1** | Source map compiler changes | 1 week |
| | DAP server skeleton + source-map-based stack trace | 2 weeks |
| **Phase 2** | VM debug interface + suspension | 3 weeks |
| | Breakpoint patching | 1 week |
| | DAP JSON-RPC protocol | 2 weeks |
| **Phase 3** | Stack trace + variable inspection | 2 weeks |
| | Continue/step/stop commands | 1 week |
| | Tests | 1 week |
| **Total** | | **13 weeks** |

## Dependency on VM Suspension

Phase 2 (VM suspension) depends on the same VM refactoring needed for [async/await](gap-concurrency.md). Until the VM supports suspension:
- Breakpoints can be **set** (source map addresses known)
- Breakpoints cannot be **hit** (no DEBUG_BREAK opcode execution)
- Solution: Phase 1 delivers source maps + DAP skeleton (usable for error navigation)