#include "ui/lcd_text.hpp"

#include <algorithm>
#include <array>
#include <string_view>
#include <utility>

#include "ui/font5x7.hpp"

namespace winampdeck::ui {

namespace {

// Characters A00 has outside ASCII, at their codes there.
constexpr auto kRomCharacters = std::to_array<std::pair<char32_t, char>>({
    {U'°', '\xDF'},  // °
    {U'µ', '\xE4'},  // µ
    {U'ß', '\xE2'},  // ß
    {U'ä', '\xE1'},  // ä
    {U'ñ', '\xEE'},  // ñ
    {U'ö', '\xEF'},  // ö
    {U'ü', '\xF5'},  // ü
});

static_assert(std::ranges::is_sorted(kRomCharacters, {}, &std::pair<char32_t, char>::first));

// Spelt out in ASCII instead. Sorted by code point.
constexpr auto kTransliterations = std::to_array<std::pair<char32_t, std::string_view>>({
    {U'Ä', "Ae"},
    {U'Ö', "Oe"},
    {U'Ü', "Ue"},
    {U'Ё', "Yo"},
    {U'А', "A"},
    {U'Б', "B"},
    {U'В', "V"},
    {U'Г', "G"},
    {U'Д', "D"},
    {U'Е', "E"},
    {U'Ж', "Zh"},
    {U'З', "Z"},
    {U'И', "I"},
    {U'Й', "Y"},
    {U'К', "K"},
    {U'Л', "L"},
    {U'М', "M"},
    {U'Н', "N"},
    {U'О', "O"},
    {U'П', "P"},
    {U'Р', "R"},
    {U'С', "S"},
    {U'Т', "T"},
    {U'У', "U"},
    {U'Ф', "F"},
    {U'Х', "Kh"},
    {U'Ц', "Ts"},
    {U'Ч', "Ch"},
    {U'Ш', "Sh"},
    {U'Щ', "Shch"},
    {U'Ъ', ""},
    {U'Ы', "Y"},
    {U'Ь', ""},
    {U'Э', "E"},
    {U'Ю', "Yu"},
    {U'Я', "Ya"},
    {U'а', "a"},
    {U'б', "b"},
    {U'в', "v"},
    {U'г', "g"},
    {U'д', "d"},
    {U'е', "e"},
    {U'ж', "zh"},
    {U'з', "z"},
    {U'и', "i"},
    {U'й', "y"},
    {U'к', "k"},
    {U'л', "l"},
    {U'м', "m"},
    {U'н', "n"},
    {U'о', "o"},
    {U'п', "p"},
    {U'р', "r"},
    {U'с', "s"},
    {U'т', "t"},
    {U'у', "u"},
    {U'ф', "f"},
    {U'х', "kh"},
    {U'ц', "ts"},
    {U'ч', "ch"},
    {U'ш', "sh"},
    {U'щ', "shch"},
    {U'ъ', ""},
    {U'ы', "y"},
    {U'ь', ""},
    {U'э', "e"},
    {U'ю', "yu"},
    {U'я', "ya"},
    {U'ё', "yo"},
});

static_assert(std::ranges::is_sorted(kTransliterations, {},
                                     &std::pair<char32_t, std::string_view>::first));

template <typename Table>
const typename Table::value_type* lookUp(const Table& table, char32_t codePoint) {
    const auto it = std::ranges::lower_bound(table, codePoint, {}, &Table::value_type::first);
    return it != table.end() && it->first == codePoint ? &*it : nullptr;
}

// ASCII that A00 holds as itself.
bool sameInRom(char32_t codePoint) {
    return codePoint >= U' ' && codePoint <= U'}' && codePoint != U'\\';
}

void append(std::string& codes, char32_t codePoint) {
    if (sameInRom(codePoint)) {
        codes += static_cast<char>(codePoint);
    } else if (codePoint < U' ' || codePoint == U'\x7F') {
        codes += ' ';
    } else if (codePoint == U'\\') {
        codes += '/';
    } else if (codePoint == U'~') {
        codes += '-';
    } else if (const auto* rom = lookUp(kRomCharacters, codePoint)) {
        codes += rom->second;
    } else if (const auto* spelt = lookUp(kTransliterations, codePoint)) {
        codes += spelt->second;
    } else if (const char32_t plain = substitute(codePoint); plain != codePoint && sameInRom(plain)) {
        codes += static_cast<char>(plain);
    } else {
        codes += '?';
    }
}

}  // namespace

std::string encodeLcdText(std::string_view text) {
    std::string codes;
    codes.reserve(text.size());
    for (const char32_t codePoint : decodeUtf8(text)) {
        append(codes, codePoint);
    }
    return codes;
}

}  // namespace winampdeck::ui
