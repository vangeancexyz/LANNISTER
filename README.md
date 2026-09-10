# LANNISTER

LANNISTER is a Linux x86_64 internal instrumentation project for Counter-Strike: Source. It was built as a practical study of ELF process injection, Source Engine internals and C++ VTable hooking.

The project loads `lannister.so` into the running game process through GDB and `ptrace`. Once resident, the library resolves engine interfaces at runtime and installs hooks directly into the game's existing execution paths.

## Current features

- Runtime interface discovery through `dlopen`, `dlsym` and `CreateInterface`
- Module base discovery through `/proc/self/maps`
- `IVModelRender::DrawModelExecute` hook for player chams
- Custom X-RAY material using the Source material system
- `IClientMode::CreateMove` hook for bunny hop input handling
- Internal overlay rendered through `IEngineVGui::Paint` and `ISurface`
- Engine-native debug output through `libtier0.so::Msg`

## Internals

LANNISTER does not create a separate window or render context. Its code runs inside the target address space and reuses the interfaces, objects and rendering pipeline already owned by the game.

The hooks are installed by replacing selected VTable entries after temporarily changing the page protection with `mprotect`. Original function pointers are retained so normal engine execution can continue after the custom logic runs.

Some interfaces are resolved through the engine's versioned factory system. `IClientMode`, which is not exposed by `CreateInterface`, is reached from the ASLR-adjusted base of `client.so` using an offset recovered during reverse engineering.

The same primitives appear in malware and Red Team tooling—process injection, shared-object loading, runtime symbol resolution and execution-flow redirection. LANNISTER applies them to a controlled game environment as a reverse-engineering study.

## Project status

This code targets the tested 64-bit Linux build of Counter-Strike: Source. VTable indices, structure layouts and offsets are tied to that build and may break after a game update.

The current implementation includes functional render and movement hooks. The internal menu is still experimental.

## Technical write-up

The full research notes, failed approaches and reverse-engineering process are documented here:

[LANNISTER: The Intersection of Malware Development and Game Hacking on Linux](https://vangeancexyz.blogspot.com/2026/08/lannister.html)

## Scope

This project was created for educational and research purposes in a controlled environment. It is not intended as a step-by-step cheating or intrusion guide.
