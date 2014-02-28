# This file is part of the Zeppelin music player project.
# Copyright (c) 2013-2014 Zoltan Kovacs, Lajos Santa
# See http://zeppelin-player.com for more details.

vars = Variables()
vars.Add(PathVariable('PREFIX', 'prefix used to install files', '/usr'))
vars.Add(PathVariable('JSONCPP', 'path of jsoncpp library', None))

env = Environment(variables = vars)

env["CPPFLAGS"] = ["-O2", "-Wall", "-Werror", "-Wshadow", "-std=c++11", "-pthread"]
env["CPPPATH"] = [Dir("src")]
env["LIBPATH"] = []

env["SHCXXCOMSTR"] = "Compiling $SOURCE"
env["SHLINKCOMSTR"] = "Linking $TARGET"

if "PREFIX" in env :
    env["CPPPATH"] += ["%s/include" % env["PREFIX"]]

if "JSONCPP" in env :
    env["CPPPATH"] += ["%s/include" % env["JSONCPP"]]
    env["LIBPATH"] += ["%s/lib" % env["JSONCPP"]]

plugin = env.SharedLibrary(
    target = "jsonrpc-remote",
    source = ["src/server.cpp", "src/plugin.cpp"]
)

env.Alias("install", env.Install("$PREFIX/lib/zeppelin/plugins", plugin))
