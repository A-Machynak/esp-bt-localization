#include "master/http/server.h"
#include "master/http/server_impl.h"

namespace Master
{
HttpServer::HttpServer()
    : _impl(new Impl::HttpServer)
{
}

HttpServer::~HttpServer()
{
	delete _impl;
}

void HttpServer::Init(const WifiConfig & config)
{
	_impl->Init(config);
}

void HttpServer::SetDevicesGetData(std::span<const char> data)
{
	_impl->SetDevicesGetData(data);
}

void HttpServer::SetConfigPostListener(std::function<void(std::span<const char>)> fn)
{
	_impl->SetConfigPostListener(fn);
}

}  // namespace Master
