#include "master/master.h"
#include "master/master_impl.h"

namespace Master
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

}  // namespace Master
