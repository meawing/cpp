#pragma once

#include <cstddef>
#include <array>

namespace cactus {

struct MachineContext {
    // Opaque pointer saved during suspension.
    void* data;
    void* rip;

    // Saved registers.
    void* r15;
    void* r14;
    void* r13;
    void* r12;
    void* rbx;
    void* rbp;

    // After activation, context starts executing this instruction.
    void* saved_rip;
};

struct SwitchArgs;
struct SavedContext;

struct IContextEntry {
    // Run returns saved context that should be activated right after Run is finished.
    virtual SavedContext* Run() = 0;
};

struct SavedContext {
    SavedContext(const SavedContext&) = delete;

    SavedContext();

    // Create new context with a given stack.
    SavedContext(char* stack, size_t stack_size);

    // Activate this context and suspend current context in @me.
    void Activate(SavedContext* suspend_to);

    // Suspended context is always stored on top of the corresponding stack.
    MachineContext* rsp = nullptr;

    static constexpr int kErasedSize = 16;
    // Exception state of suspended context.
    std::array<char, kErasedSize> eh = {};

    // Function run on the first context activation.
    IContextEntry* entry = nullptr;

    void* sanitizer_fake_stack = nullptr;
    void* sanitizer_stack_bottom = nullptr;
    size_t sanitizer_stack_size = 0;
};

}  // namespace cactus
