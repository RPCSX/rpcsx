#pragma once

namespace rx {
bool isDebuggerPresent();
void waitForDebugger();
void runDebugger();
void breakpoint();
void breakpointIfDebugging();
} // namespace rx
