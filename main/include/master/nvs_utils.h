#pragma once

#include <cstdint>
#include <optional>
#include <queue>
#include <span>
#include <string>

namespace Master::Nvs
{

/// @brief NVS cache for frequently queried values.
class Cache
{
public:
	struct CachedValues
	{
		std::optional<std::int8_t> RefPathLoss{std::nullopt};
		std::optional<float> EnvFactor{std::nullopt};
	};

	/// @brief Singleton instance
	/// @return instance
	static Cache & Instance();

	/// @brief Get cached values
	/// @param key key
	/// @return cached values
	const CachedValues & GetValues(std::span<const std::uint8_t, 6> key);

	/// @brief MacName getter; not cached. For convenience.
	/// @param key key
	/// @return mac name
	std::optional<std::string> GetMacName(std::span<const std::uint8_t, 6> key);

	/// @brief NVS Setters which cache some values
	/// @{
	void SetRefPathLoss(std::span<const std::uint8_t, 6> key, std::int8_t pl);
	void SetEnvFactor(std::span<const std::uint8_t, 6> key, float envFactor);
	void SetMacName(std::span<const std::uint8_t, 6> key, std::span<const char> name);
	/// @}

private:
	Cache();
	Cache(const Cache &) = delete;

	static constexpr std::size_t SizeLimit = 32;

	struct KeyValue
	{
		std::array<std::uint8_t, 6> Key;
		CachedValues Value{};
	};

	/// @brief We *could* use a map...however, this structure will probably be filled quite fast
	/// once we start querying for devices with random BDA. Therefore, we'll have to start emptying
	/// it, so it doesn't keep filling up forever. But which value should we throw away? LRU? We'd
	/// have to save another index with that and find the lowest index by going through the whole
	/// map.
	/// ... Whatever. TL;DR Just don't change this.
	std::vector<KeyValue> _vec;

	/// @brief We never delete anything; therefore we'll just hold an index to the first value that
	/// shall be overwritten.
	std::size_t _head;
};

/// @brief Setters/Getters for NVS data
/// @{
void SetRefPathLoss(std::span<const std::uint8_t, 6> mac, std::int8_t value);
void SetEnvFactor(std::span<const std::uint8_t, 6> mac, float value);
void SetMacName(std::span<const std::uint8_t, 6> mac, std::span<const char> name);

std::optional<std::int8_t> GetRefPathLoss(std::span<const std::uint8_t, 6> mac);
std::optional<float> GetEnvFactor(std::span<const std::uint8_t, 6> mac);
std::optional<std::string> GetMacName(std::span<const std::uint8_t, 6> mac);
/// @}

}  // namespace Master::Nvs
