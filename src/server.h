#ifndef JSONRPCREMOTE_SERVER_H_INCLUDED
#define JSONRPCREMOTE_SERVER_H_INCLUDED

#include <zeppelin/plugins/http-server/httpserver.h>

#include <zeppelin/plugin/plugin.h>
#include <zeppelin/library/musiclibrary.h>
#include <zeppelin/player/controller.h>

#include <jsoncpp/json/value.h>

#include <unordered_map>
#include <stdexcept>

class InvalidMethodCall : public std::runtime_error
{
    public:
	InvalidMethodCall()
	    : runtime_error("invalid method call")
	{}
};

class Server : public zeppelin::plugin::Plugin
{
    public:
	Server(const std::shared_ptr<zeppelin::library::MusicLibrary>& library,
	       const std::shared_ptr<zeppelin::player::Controller>& ctrl);

	std::string getName() const override
	{ return "jsonrpc-remote"; }

	void start(const Json::Value& config, zeppelin::plugin::PluginManager& pm) override;
	void stop() override;

    private:
	std::unique_ptr<httpserver::HttpResponse> processRequest(const httpserver::HttpRequest& request);

	void libraryScan(const Json::Value& request, Json::Value& response);
	void libraryGetStatistics(const Json::Value& request, Json::Value& response);

	// library - artists
	void libraryGetArtists(const Json::Value& request, Json::Value& response);

	// library - albums
	void libraryGetAlbums(const Json::Value& request, Json::Value& response);
	void libraryGetAlbumsByArtist(const Json::Value& request, Json::Value& response);

	// library - files
	void libraryGetFilesOfArtist(const Json::Value& request, Json::Value& response);
	void libraryGetFilesOfAlbum(const Json::Value& request, Json::Value& response);

	// library - directories
	void libraryListDirectory(const Json::Value& request, Json::Value& response);

	// library - metadata
	void libraryGetMetadata(const Json::Value& request, Json::Value& response);
	void libraryUpdateMetadata(const Json::Value& request, Json::Value& response);

	// player - queue
	void playerQueueFile(const Json::Value& request, Json::Value& response);
	void playerQueueDirectory(const Json::Value& request, Json::Value& response);
	void playerQueueAlbum(const Json::Value& request, Json::Value& response);
	void playerQueueGet(const Json::Value& request, Json::Value& response);
	void playerQueueRemove(const Json::Value& request, Json::Value& response);
	void playerQueueRemoveAll(const Json::Value& request, Json::Value& response);

	void playerStatus(const Json::Value& request, Json::Value& response);

	void playerPlay(const Json::Value& request, Json::Value& response);
	void playerPause(const Json::Value& request, Json::Value& response);
	void playerStop(const Json::Value& request, Json::Value& response);
	void playerSeek(const Json::Value& request, Json::Value& response);
	void playerPrev(const Json::Value& request, Json::Value& response);
	void playerNext(const Json::Value& request, Json::Value& response);
	void playerGoto(const Json::Value& request, Json::Value& response);

	void playerGetVolume(const Json::Value& request, Json::Value& response);
	void playerSetVolume(const Json::Value& request, Json::Value& response);

	void requireType(const Json::Value& request, const std::string& key, Json::ValueType type);

    private:
	std::shared_ptr<zeppelin::library::MusicLibrary> m_library;
	std::shared_ptr<zeppelin::player::Controller> m_ctrl;

	typedef std::function<void(const Json::Value&, Json::Value&)> RpcMethod;

	std::unordered_map<std::string, RpcMethod> m_rpcMethods;
};

#endif
