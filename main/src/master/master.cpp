#include "master/master.h"
#include "master/master_impl.h"

namespace Master
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

}  // namespace Master
