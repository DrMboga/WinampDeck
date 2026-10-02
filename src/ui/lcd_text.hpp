#pragma once

#include <string>
#include <string_view>

namespace winampdeck::ui {

// UTF-8 text as character codes for the HD44780's A00 ROM, the Japanese one
// these modules usually carry:
//
// - ASCII as itself, except `\` and `~`, whose codes hold ¥ and → in A00:
//   they become / and -. Control characters become spaces.
// - ä ö ü ß ñ ° µ as the ROM's own characters; Ä Ö Ü as Ae Oe Ue.
// - Cyrillic transliterated (Кино as Kino), since A00 has none.
// - Otherwise the plain character the TFT's font substitutes (é as e, — as -),
//   else '?'.
//
// So one character may become several codes, and the codes never include
// 0x00–0x07, the controller's user-defined characters.
std::string encodeLcdText(std::string_view text);

}  // namespace winampdeck::ui
