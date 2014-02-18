#include "server.h"

// =====================================================================================================================
extern "C"
std::shared_ptr<zeppelin::plugin::Plugin> plugin_create(const std::shared_ptr<zeppelin::library::MusicLibrary>& library,
							const std::shared_ptr<zeppelin::player::Controller>& ctrl)
{
    return std::make_shared<Server>(library, ctrl);
}
