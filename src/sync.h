#pragma once

namespace josts {
// Handles Josts Sync command-line modes before the normal UI/elevated broker.
// Returns false for a normal interactive launch.
bool sync_entry(int& exit_code);
}
