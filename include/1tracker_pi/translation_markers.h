#pragma once

// Translation marker for string literals in core (non-wx) code that get
// rendered in the UI. At runtime the macro is a no-op — the literal is
// used as-is. At build time xgettext is told (via
// cmake/PluginLocalization.cmake: --keyword=TR_NOOP) that TR_NOOP's
// argument is a translation key, so the literal lands in the .pot file
// next to strings marked with the UI-layer `_()` macro.
//
// Pair each TR_NOOP in core with `wxGetTranslation(...)` at the UI call
// site that displays the string; that is what actually looks the key up
// in the compiled .mo catalog and returns the localised text.
#define TR_NOOP(x) x
