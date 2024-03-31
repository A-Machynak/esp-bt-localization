#pragma once

#include <array>
#include <cstdint>
#include <string>

#include <esp_bt_defs.h>

namespace Util
{
/// @brief Bytes in 128b UUID
constexpr std::size_t UuidByteCount = 16;

/// @brief Convert string representation of a 128b UUID to array
/// @param uuid uuid as a string (format: 01234567-0123-4567-89ab-0123456789ab)
/// @param flip if true, flips the array (to format: ba9876543210-ba98-7654-3210-76543210)
/// @return uuid
constexpr std::array<std::uint8_t, UuidByteCount> UuidToArray(std::string_view uuid, bool flip);

/// @brief Convert string representation of a 128b UUID to the esp_bt_uuid_t struct
/// @param uuid uuid as a string (format: 01234567-0123-4567-89ab-0123456789ab)
/// @param flip if true, flips the array (to format: ba9876543210-ba98-7654-3210-76543210)
/// @return esp_bt_uuid_t struct
constexpr esp_bt_uuid_t UuidToStruct(std::string_view uuid, bool flip);

}  // namespace Util

constexpr bool operator==(const esp_bt_uuid_t & lhs,
                          const std::array<std::uint8_t, Util::UuidByteCount> & rhs);
constexpr bool operator==(const esp_bt_uuid_t & lhs, std::string_view rhs);
constexpr bool operator==(std::string_view lhs,
                          const std::array<std::uint8_t, Util::UuidByteCount> & rhs);
constexpr std::string ToString(esp_bt_uuid_t uuid);

#include "core/utility/uuid.hpp"
