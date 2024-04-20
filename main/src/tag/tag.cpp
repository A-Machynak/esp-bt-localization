#include "tag/tag.h"
#include "tag/tag_impl.h"

namespace Tag
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

}  // namespace Tag
