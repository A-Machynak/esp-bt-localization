#pragma once

#include "core/wrapper/gap_ble_wrapper.h"

#include <stdexcept>

namespace Gap::Ble
{
constexpr std::uint16_t ConvertAdvertisingInterval(float seconds)
{
	constexpr float MinF = 0.02;
	constexpr float MaxF = 10.24;
	constexpr std::uint16_t MinU = 0x0020;
	constexpr std::uint16_t MaxU = 0x4000;

	if (seconds < MinF || MaxF < seconds) {
		throw std::invalid_argument("Out of bounds of the advertising interval <0.02, 10.24>s");
	}
	constexpr float inverse = 1.0 / 0.625;
	const std::uint16_t n = static_cast<std::uint16_t>(seconds * 1000.0 * inverse);

	if (n < MinU || MaxU < n) {
		throw std::invalid_argument(
		    "Out of bounds of the advertising interval <0.02, 10.24>s after conversion");
	}
	return n;
}

constexpr std::uint16_t ConvertScanInterval(float seconds)
{
	constexpr float MinF = 0.0025;
	constexpr float MaxF = 10.24;
	constexpr std::uint16_t MinU = 0x0004;
	constexpr std::uint16_t MaxU = 0x4000;

	if (seconds < MinF || MaxF < seconds) {
		throw std::invalid_argument("Out of bounds of the advertising interval <0.0025, 10.24>s");
	}
	constexpr float inverse = 1.0 / 0.625;
	const std::uint16_t n = static_cast<std::uint16_t>(seconds * 1000.0 * inverse);

	if (n < MinU || MaxU < n) {
		throw std::invalid_argument(
		    "Out of bounds of the advertising interval <0.0025, 10.24>s after conversion");
	}
	return n;
}

constexpr std::uint16_t ConvertScanWindow(float seconds)
{
	constexpr float MinF = 0.0025;
	constexpr float MaxF = 10.24;
	constexpr std::uint16_t MinU = 0x0004;
	constexpr std::uint16_t MaxU = 0x4000;

	if (seconds < MinF || MaxF < seconds) {
		throw std::invalid_argument("Out of bounds of the advertising interval <0.0025, 10.24>s");
	}
	constexpr float inverse = 1.0 / 0.625;
	const std::uint16_t n = static_cast<std::uint16_t>(seconds * 1000.0 * inverse);

	if (n < MinU || MaxU < n) {
		throw std::invalid_argument(
		    "Out of bounds of the advertising interval <0.0025, 10.24>s after conversion");
	}
	return n;
}
} // namespace Gap::Ble
