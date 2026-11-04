#pragma once
// Shared mod logic; only binary interface details differ between the two games.
namespace bmvr { namespace abi {
enum class Build { Unknown, Stable19042901, Beta25844216, Beta26234270 };
inline Build build = Build::Unknown;
inline bool Beta() { return build == Build::Beta25844216 || build == Build::Beta26234270; }
inline bool Latest() { return build == Build::Beta26234270; }
inline int Select(int stable, int beta) { return Beta() ? beta : stable; }
} }
