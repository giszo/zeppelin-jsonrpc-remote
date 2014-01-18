#include "server.h"

// =====================================================================================================================
extern "C"
zeppelin::plugin::Plugin* plugin_create(const std::shared_ptr<zeppelin::library::MusicLibrary>& library,
					const std::shared_ptr<zeppelin::player::Controller>& ctrl)
{
    return new Server(library, ctrl);
}
