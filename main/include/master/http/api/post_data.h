#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <variant>

namespace Master::HttpApi
{

/// @brief Type of POST data
/// - System message (Restart)
/// - Set reference RSSI
/// - Set environment factor
/// - Map name to a device
namespace Type
{

/// @brief Mapping between type representation <-> Type value
enum class ValueType : std::uint8_t
{
	SystemMsg = 0,
	RefPathLoss = 1,
	EnvFactor = 2,
	MacName = 3,
	ForceAdvertise = 4
};

struct SystemMsg
{
	SystemMsg(std::span<const std::uint8_t> data);

	enum class Operation
	{
		Restart = 0,
		ResetScanners = 1,
		SwitchToAp = 2,
		SwitchToSta = 3
	};

	/// @brief Data getters
	/// @return data
	/// @{
	Operation Value() const;
	/// @}

	/// @brief Validity check; expects data without first (type) byte
	/// @param data data
	/// @return is valid
	static bool IsValid(std::span<const std::uint8_t> data);

	constexpr static std::size_t Size = 1;
	Operation Data;
};

/// @brief Reference RSSI for a scanner
struct RefPathLoss
{
	RefPathLoss(std::span<const std::uint8_t> data);

	/// @brief Data getters
	/// @return data
	/// @{
	std::span<const std::uint8_t, 6> Mac() const;
	std::int8_t Value() const;
	/// @}

	/// @brief Validity check; expects data without first type byte
	/// @param data data
	/// @return is valid
	static bool IsValid(std::span<const std::uint8_t> data);

	constexpr static std::size_t Size = 7;  // 6B MAC + 1B int8
	std::span<const std::uint8_t, Size> Data;
};

/// @brief Environment factor for a scanner
struct EnvFactor
{
	EnvFactor(std::span<const std::uint8_t> data);

	/// @brief Data getters
	/// @return data
	/// @{
	std::span<const std::uint8_t, 6> Mac() const;
	float Value() const;
	/// @}

	/// @brief Validity check; expects data without first type byte
	/// @param data data
	/// @return is valid
	static bool IsValid(std::span<const std::uint8_t> data);

	constexpr static std::size_t Size = 10;  // 6B MAC + 4B float
	std::span<const std::uint8_t, Size> Data;
};

/// @brief Map a name to a Scanner
struct MacName
{
	MacName(std::span<const std::uint8_t> data);
	MacName(const MacName &) = default;
	MacName & operator=(const MacName &) = default;

	/// @brief Data getters
	/// @return data
	/// @{
	std::span<const std::uint8_t, 6> Mac() const;
	std::span<const char> Value() const;
	/// @}

	/// @brief Validity check; expects data without first type byte
	/// @param data data
	/// @return is valid
	static bool IsValid(std::span<const std::uint8_t> data);

	std::span<const std::uint8_t> Data;
	std::size_t ValueLength;

	static constexpr std::size_t MinValueLength = 1;
	static constexpr std::size_t MaxValueLength = 16;
	static constexpr std::size_t MinSize = 7;   // 6B MAC + 1B str
	static constexpr std::size_t MaxSize = 22;  // 6B MAC + 16B str
};

/// @brief Force a scanner to advertise
struct ForceAdvertise
{
	ForceAdvertise(std::span<const std::uint8_t> data);

	/// @brief Data getters
	/// @return data
	/// @{
	std::span<const std::uint8_t, 6> Mac() const;
	/// @}

	/// @brief Validity check; expects data without first type byte
	/// @param data data
	/// @return is valid
	static bool IsValid(std::span<const std::uint8_t> data);

	constexpr static std::size_t Size = 6;  // 6B MAC
	std::span<const std::uint8_t, Size> Data;
};
}  // namespace Type

/// @brief POST data underlying types
using PostDataEntry = std::variant<std::monostate,
                                   Type::SystemMsg,
                                   Type::RefPathLoss,
                                   Type::EnvFactor,
                                   Type::MacName,
                                   Type::ForceAdvertise>;

/// @brief View for accessing devices API POST data:
/// [Type][Data][Type]...
struct DevicesPostDataView
{
	DevicesPostDataView(std::span<const std::uint8_t> data);

	/// @brief Returns the currently pointed to data entry
	/// and increases the pointer by one entry
	/// @return data entry; monostate if invalid/end of data
	PostDataEntry Next();

	std::span<const std::uint8_t> Data;  ///< Underlying data
	std::size_t Head{0};                 ///< Pointer to the start of the current data entry
};

}  // namespace Master::HttpApi
