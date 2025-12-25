#include "StrUtil.hpp"

#include <algorithm>
#include <codecvt>
#include <locale>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

std::wstring rx::toWchar(std::string_view src) {
#ifdef _WIN32
  std::wstring wchar_string;
  const int size = static_cast<int>(src.size());
  const auto tmp_size =
      MultiByteToWideChar(CP_UTF8, 0, src.data(), size, nullptr, 0);
  wchar_string.resize(tmp_size);
  MultiByteToWideChar(CP_UTF8, 0, src.data(), size, wchar_string.data(),
                      tmp_size);
  return wchar_string;
#else
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> converter{};
  return converter.from_bytes(src.data());
#endif
}

std::string rx::toUtf8(std::wstring_view src) {
#ifdef _WIN32
  std::string utf8_string;
  const int size = static_cast<int>(src.size());
  const auto tmp_size = WideCharToMultiByte(CP_UTF8, 0, src.data(), size,
                                            nullptr, 0, nullptr, nullptr);
  utf8_string.resize(tmp_size);
  WideCharToMultiByte(CP_UTF8, 0, src.data(), size, utf8_string.data(),
                      tmp_size, nullptr, nullptr);
  return utf8_string;
#else
  std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter{};
  return converter.to_bytes(src.data());
#endif
}

std::string rx::toUtf8(std::u16string_view src) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter{};
  return converter.to_bytes(src.data());
}

std::u16string rx::toUtf16(std::string_view src) {
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter{};
  return converter.from_bytes(src.data());
}

#ifdef _MSC_VER
#pragma warning(pop)
#else
#pragma GCC diagnostic pop
#endif

std::string rx::replaceAll(std::string_view src, std::string_view from,
                           std::string_view to, std::size_t count) {
  std::string target;
  target.reserve(src.size() + to.size());

  for (std::size_t i = 0, replaced = 0; i < src.size();) {
    const std::size_t pos = src.find(from, i);

    if (pos == std::string_view::npos || replaced++ >= count) {
      // No match or too many encountered, append the rest of the string as is
      target.append(src.substr(i));
      break;
    }

    // Append source until the matched string position
    target.append(src.substr(i, pos - i));

    // Replace string
    target.append(to);
    i = pos + from.size();
  }

  return target;
}

std::string rx::toUpper(std::string_view string) {
  std::string result;
  result.resize(string.size());
  std::ranges::transform(string, result.begin(), ::toupper);
  return result;
}

std::string rx::toLower(std::string_view string) {
  std::string result;
  result.resize(string.size());
  std::ranges::transform(string, result.begin(), ::tolower);
  return result;
}

std::string rx::truncateString(std::string_view src, std::size_t length) {
  return {src.begin(), src.begin() + std::min(src.size(), length)};
}

bool rx::matchString(std::string_view source, std::string_view mask) {
  std::size_t source_position = 0, mask_position = 0;

  for (; source_position < source.size() && mask_position < mask.size();
       ++mask_position, ++source_position) {
    switch (mask[mask_position]) {
    case '?':
      break;

    case '*':
      for (std::size_t test_source_position = source_position;
           test_source_position < source.size(); ++test_source_position) {
        if (matchString(source.substr(test_source_position),
                        mask.substr(mask_position + 1))) {
          return true;
        }
      }
      return false;

    default:
      if (source[source_position] != mask[mask_position]) {
        return false;
      }

      break;
    }
  }

  if (source_position != source.size())
    return false;

  if (mask_position != mask.size())
    return false;

  return true;
}
