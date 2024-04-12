#pragma once

#include "master/http/server_cfg.h"

#include <functional>
#include <span>

namespace Master::Impl
{
class HttpServer;
}  // namespace Master::Impl

namespace Master
{

/// @brief Basic HTTP server for Master device with 3 endpoints (IndexUri, DevicesUri, ConfigUri).
class HttpServer
{
public:
	/// @brief Constructor, has to be initialized with Init (in case of static initialization)
	HttpServer();
	~HttpServer();

	/// @brief Initialize with configuration
	/// @param config config
	void Init(const WifiConfig & config);

	/// @brief Set the data returned from DevicesUri endpoint (GET)
	/// @param data raw data
	void SetDevicesGetData(std::span<const char> data);

	/// @brief Listener for ConfigUri POST request
	/// @param fn function
	void SetConfigPostListener(std::function<void(std::span<const char>)> fn);

private:
	Impl::HttpServer * _impl;
};

}  // namespace Master
