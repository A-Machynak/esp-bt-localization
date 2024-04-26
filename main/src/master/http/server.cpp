#include "master/http/server.h"
#include "master/http/server_impl.h"

namespace Master
{
HttpServer::HttpServer(const WifiConfig & cfg)
    : _impl(new Impl::HttpServer(cfg))
{
}

HttpServer::~HttpServer()
{
	delete _impl;
}

void HttpServer::Init()
{
	_impl->Init();
}

void HttpServer::SetDevicesGetData(std::span<const char> data)
{
	_impl->SetDevicesGetData(data);
}

void HttpServer::SetConfigPostListener(std::function<void(std::span<const char>)> fn)
{
	_impl->SetConfigPostListener(fn);
}

void HttpServer::SwitchMode(WifiOpMode mode)
{
	_impl->SwitchMode(mode);
}

}  // namespace Master
