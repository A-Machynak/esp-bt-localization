#include "scanner/scanner.h"
#include "scanner/scanner_impl.h"

namespace Scanner
{

App::App()
    : _impl(new Impl::App)
{
}

App::~App()
{
	delete _impl;
}

void App::Init(const AppConfig & cfg)
{
	_impl->Init(cfg);
}

}  // namespace Scanner
