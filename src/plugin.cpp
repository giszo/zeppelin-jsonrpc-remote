/**
 * This file is part of the Zeppelin music player project.
 * Copyright (c) 2013-2014 Zoltan Kovacs, Lajos Santa
 * See http://zeppelin-player.com for more details.
 */

#include "server.h"

// =====================================================================================================================
extern "C"
std::shared_ptr<zeppelin::plugin::Plugin> plugin_create(const std::shared_ptr<zeppelin::library::MusicLibrary>& library,
							const std::shared_ptr<zeppelin::player::Controller>& ctrl)
{
    return std::make_shared<Server>(library, ctrl);
}
