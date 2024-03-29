#pragma once

#include "core/utility/uuid.h"

#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace Util
{

constexpr bool IsAlnum(const char c)
{
	return (c >= '0' && '9' >= c) || (c >= 'a' && 'z' >= c) || (c >= 'A' && 'Z' >= c);
}

constexpr bool IsDigit(const char c)
{
	return (c >= '0' && '9' >= c);
}

constexpr char ToLower(const char c)
{
	return (c >= 'a' && 'z' >= c) ? c : (c + ('a' - 'A'));
}

constexpr std::array<std::uint8_t, 16> UuidToArray(std::string_view uuid, bool flip = false)
{
	// "01234567-0123-4567-89ab-0123456789ab"
	constexpr std::size_t UuidSize = 36;  // String representation size
	// assert(uuid.size() == UuidSize);

	std::array<std::uint8_t, 16> arr;

	const auto HexToUint = [](char c) -> std::uint8_t {
		return IsDigit(c) ? static_cast<uint8_t>(c - '0')
		                  : static_cast<uint8_t>(ToLower(c) - 'a') + 10;
	};

	std::size_t index = 0;
	for (std::size_t i = 0; i < UuidSize; i++) {
		if (uuid[i] == '-') {
			continue;  // skip '-'; simple format check
		}
		else if (!IsAlnum(uuid[i]) || !IsAlnum(uuid[i + 1])) {
			throw std::invalid_argument("Invalid UUID format");
		}
		const uint8_t byte = flip ? (HexToUint(uuid[i] + 1) << 4) | HexToUint(uuid[i])
		                          : (HexToUint(uuid[i]) << 4) | HexToUint(uuid[i + 1]);

		arr[flip ? index : (arr.size() - index - 1)] = byte;
		index++;
		i++;  // read 2 characters
	}

	return arr;
}

constexpr esp_bt_uuid_t UuidToStruct(std::string_view uuid, bool flip = false)
{
	std::array arr = UuidToArray(uuid, flip);

	esp_bt_uuid_t uuidData;
	uuidData.len = 16;
	std::copy_n(arr.data(), 16, uuidData.uuid.uuid128);

	return uuidData;
}

}  // namespace Util

constexpr bool operator==(const esp_bt_uuid_t & lhs,
                          const std::array<std::uint8_t, Util::UuidByteCount> & rhs)
{
	if (lhs.len != 16) {
		return false;
	}
	return std::equal(rhs.begin(), rhs.end(), lhs.uuid.uuid128);
}

constexpr bool operator==(const esp_bt_uuid_t & lhs, std::string_view rhs)
{
	return operator==(lhs, Util::UuidToArray(rhs));
}

constexpr bool operator==(std::string_view lhs,
                          const std::array<std::uint8_t, Util::UuidByteCount> & rhs)
{
	return Util::UuidToArray(lhs) == rhs;
}

constexpr std::string ToString(esp_bt_uuid_t uuid)
{
	// "01234567-0123-4567-89ab-0123456789ab"
	constexpr std::size_t MaxSize = 37;
	constexpr char ch[] = "0123456789abcdef";
	const auto & arr = uuid.uuid.uuid128;

	std::array<char, MaxSize> buffer;
	std::size_t arrPos = 0;
	for (std::size_t i = 0; i < uuid.len; i++) {
		// split byte into 2x4 bits and convert to char
		const std::uint8_t val = arr[uuid.len - 1 - i];  // reverse
		buffer[arrPos++] = ch[(val >> 4) & 0x0F];
		buffer[arrPos++] = ch[val & 0x0F];
		if (arrPos == 8 || arrPos == 13 || arrPos == 18 || arrPos == 23) {
			buffer[arrPos++] = '-';
		}
	}

	if (uuid.len <= 4) {
		return std::string(buffer.data(), uuid.len * 2);
	}
	// assume 128b
	buffer.back() = '\0';
	return std::string(buffer.data());
}
