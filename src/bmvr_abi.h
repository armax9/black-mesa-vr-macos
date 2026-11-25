#pragma once
// Shared mod logic; only binary interface details differ between the two games.
namespace bmvr { namespace abi {
enum class Build { Unknown, Stable19042901, Beta25844216, Beta26234270, Beta26627981, Stable26713286 };
inline Build build = Build::Unknown;
inline bool Stable267() { return build == Build::Stable26713286; }
inline bool Current() { return Stable267() || build == Build::Beta26627981; }
inline bool Beta() { return Current() || build == Build::Beta25844216 || build == Build::Beta26234270; }
inline bool Latest() { return Current() || build == Build::Beta26234270; }
inline int Select(int stable, int beta) { return Beta() ? beta : stable; }
} }
