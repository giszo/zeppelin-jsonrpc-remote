# -*- python -*-

vars = Variables()
vars.Add(PathVariable('PREFIX', 'prefix used to install files', '/'))

env = Environment(variables = vars)

env["CPPFLAGS"] = ["-O2", "-Wall", "-Werror", "-Wshadow", "-std=c++11", "-pthread"]
env["CPPPATH"] = [Dir("src")]

env["SHCXXCOMSTR"] = "Compiling $SOURCE"
env["SHLINKCOMSTR"] = "Linking $TARGET"

if "PREFIX" in env :
    env["CPPPATH"] += ["%s/%s" % (env["PREFIX"], "/usr/include")]

plugin = env.SharedLibrary(
    target = "jsonrpc-remote",
    source = ["src/server.cpp", "src/plugin.cpp"]
)

env.Alias("install", env.Install("$PREFIX/usr/lib/zeppelin/plugins", plugin))
