
#include "master/nvs_utils.h"

#include <esp_log.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <nvs_handle.hpp>

#include <algorithm>
#include <array>
#include <map>

namespace
{
/// @brief Logger tag
static const char * TAG = "NVS";

/// @brief NVS namespaces
/// @{
static const char * RefPathLossNamespace = "BtLocPL";
static const char * EnvFactorNamespace = "BtLocEF";
static const char * MacNameNamespace = "BtLocMN";
/// @}

/// @brief Doesn't allow "ridiculous" values for reference path loss, env factor, etc.
constexpr bool ForceClampValues = true;

}  // namespace

namespace Master::Nvs
{

Cache::Cache()
{
	_vec.reserve(Cache::SizeLimit);
	_head = 0;
}

Cache & Cache::Instance()
{
	static Cache cache;
	return cache;
}

const Cache::CachedValues & Cache::GetValues(const std::array<std::uint8_t, 6> & key)
{
	// Start from end
	auto it = std::find_if(_vec.rbegin(), _vec.rend(),
	                       [&](const Cache::KeyValue & kv) { return key == kv.Key; });

	if (it == _vec.rend()) {  // Not found
		auto ef = GetEnvFactor(key);
		auto pl = GetRefPathLoss(key);

		if (_vec.size() >= SizeLimit) {
			_vec[_head] = {key, Cache::CachedValues{ef, pl}};
			const KeyValue & item = _vec[_head];

			_head = (_head >= _vec.size() - 1) ? 0 : (_head + 1);
			return item.Value;
		}

		const KeyValue & item = _vec.emplace_back(key, Cache::CachedValues{ef, pl});
		return item.Value;
	}
	return it->Value;
}

std::optional<std::string> Cache::GetMacName(const std::array<std::uint8_t, 6> & key)
{
	return ::Master::Nvs::GetMacName(key);
}

void Cache::SetRefPathLoss(const std::array<std::uint8_t, 6> & key, std::int8_t pl)
{
	::Master::Nvs::SetRefPathLoss(key, pl);
}

void Cache::SetEnvFactor(const std::array<std::uint8_t, 6> & key, float envFactor)
{
	::Master::Nvs::SetEnvFactor(key, envFactor);
}

void Cache::SetMacName(const std::array<std::uint8_t, 6> & key, std::span<const char> name)
{
	::Master::Nvs::SetMacName(key, name);
}

void SetRefPathLoss(std::span<const std::uint8_t, 6> mac, std::int8_t value)
{
	if constexpr (ForceClampValues) {
		value = std::clamp(value, std::int8_t(0), std::int8_t(100));
	}

	esp_err_t err;
	if (auto p = nvs::open_nvs_handle(RefPathLossNamespace, NVS_READWRITE, &err)) {
		const std::string key(mac.begin(), mac.end());

		p.get()->set_item(key.c_str(), value);
		p.get()->commit();

		ESP_LOGI(TAG, "RefPathLoss updated: %s -> %d", key.c_str(), value);
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Set RefPathLoss): %d", err);
	}
}

void SetEnvFactor(std::span<const std::uint8_t, 6> mac, float value)
{
	if constexpr (ForceClampValues) {
		value = std::clamp(value, 0.1f, 10.0f);
	}

	esp_err_t err;
	if (auto p = nvs::open_nvs_handle(EnvFactorNamespace, NVS_READWRITE, &err)) {
		const std::string key(mac.begin(), mac.end());

		// hmm... no native support...
		p.get()->set_blob(key.c_str(), &value, sizeof(float));
		p.get()->commit();

		ESP_LOGI(TAG, "EnvFactor updated: %s -> %.2f", key.c_str(), value);
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Set EnvFactor): %d", err);
	}
}

void SetMacName(std::span<const std::uint8_t, 6> mac, std::span<const char> name)
{
	esp_err_t err;
	if (auto p = nvs::open_nvs_handle(MacNameNamespace, NVS_READWRITE, &err)) {
		const std::string key(mac.begin(), mac.end());
		const std::string value(name.begin(), name.end());

		p.get()->set_string(key.c_str(), value.c_str());
		p.get()->commit();

		ESP_LOGI(TAG, "MacName updated: %s -> \"%s\"", key.c_str(), value.c_str());
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Set MacName): %d", err);
	}
}

std::optional<std::int8_t> GetRefPathLoss(std::span<const std::uint8_t, 6> mac)
{
	esp_err_t err;

	std::int8_t out;
	if (auto p = nvs::open_nvs_handle(RefPathLossNamespace, NVS_READWRITE, &err)) {
		if (p.get()->get_item(std::string(mac.begin(), mac.end()).c_str(), out) != ESP_OK) {
			return std::optional<std::int8_t>(std::nullopt);
		}
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Get RefPathLoss): %d", err);
	}
	return std::optional<std::int8_t>(out);
}

std::optional<float> GetEnvFactor(std::span<const std::uint8_t, 6> mac)
{
	esp_err_t err;

	float out;
	if (auto p = nvs::open_nvs_handle(EnvFactorNamespace, NVS_READWRITE, &err)) {
		if (p.get()->get_blob(std::string(mac.begin(), mac.end()).c_str(), &out, 4) != ESP_OK) {
			return std::optional<float>(std::nullopt);
		}
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Get EnvFactor): %d", err);
	}
	return std::optional<float>(out);
}

std::optional<std::string> GetMacName(std::span<const std::uint8_t, 6> mac)
{
	esp_err_t err;

	std::array<char, 16> out;
	if (auto p = nvs::open_nvs_handle(MacNameNamespace, NVS_READWRITE, &err)) {
		const std::string key(mac.begin(), mac.end());
		std::size_t size = 0;
		if ((p.get()->get_item_size(nvs::ItemType::SZ, key.c_str(), size) != ESP_OK)
		    || (p.get()->get_string(key.c_str(), out.data(), size) != ESP_OK)) {
			return std::optional<std::string>(std::nullopt);
		}
	}
	else {
		ESP_LOGW(TAG, "Nvs open failed (Get MacName): %d", err);
	}
	return std::optional<std::string>({out.begin(), out.end()});
}

}  // namespace Master::Nvs
