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

void HttpServer::SetRawData(std::span<const char> data)
{
	_impl->SetRawData(data);
}

}  // namespace Master
