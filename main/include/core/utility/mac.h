#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

/// @brief MAC wrapper
struct Mac
{
	/// @brief MAC byte count
	static constexpr std::size_t Size = 6;

	/// @brief Constructor
	/// @param address MAC
	Mac(std::span<const std::uint8_t, Size> address);

	/// @brief Equality
	/// @param other other
	/// @return equal
	bool operator==(const Mac & other) const;


	/// @brief Raw MAC
	std::array<std::uint8_t, Size> Addr;

	/// Convenience address comparator (for map)
	struct Cmp
	{
		bool operator()(const Mac & lhs, const Mac & rhs) const;
	};
};

std::string ToString(const Mac & addr);
std::string ToString(std::span<const std::uint8_t, Mac::Size> addr);
