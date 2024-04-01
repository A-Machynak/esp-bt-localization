#include "scanner/scanner.h"
#include "scanner/scanner_impl.h"

namespace Scanner
{

App::App(const AppConfig & cfg)
    : _impl(new Impl::App(cfg))
{
}

App::~App()
{
	delete _impl;
}

void App::Init()
{
	_impl->Init();
}

}  // namespace Scanner
