#pragma once

#include "master/http/server_cfg.h"

#include <span>

namespace Master::Impl
{
class HttpServer;
}  // namespace Master::Impl

namespace Master
{

/// @brief Basic HTTP server for Master device with 2 endpoints (GetIndexUri, GetDevicesUri).
class HttpServer
{
public:
	/// @brief Constructor, has to be initialized with Init (in case of static initialization)
	HttpServer();
	~HttpServer();

	/// @brief Initialize with configuration
	/// @param config config
	void Init(const WifiConfig & config);

	/// @brief Set the data returned from API endpoint
	/// @param data raw data
	void SetRawData(std::span<const char> data);

private:
	Impl::HttpServer * _impl;
};

}  // namespace Master
