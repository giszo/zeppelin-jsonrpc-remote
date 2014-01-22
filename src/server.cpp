#include "server.h"

#include <zeppelin/logger.h>
#include <zeppelin/plugin/pluginmanager.h>
#include <zeppelin/library/storage.h>
#include <zeppelin/player/queue.h>

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>

#define REGISTER_RPC_METHOD(name, function) \
    m_rpcMethods[name] = std::bind(&Server::function, this, std::placeholders::_1, std::placeholders::_2)

// =====================================================================================================================
Server::Server(const std::shared_ptr<zeppelin::library::MusicLibrary>& library,
	       const std::shared_ptr<zeppelin::player::Controller>& ctrl)
    : m_library(library),
      m_ctrl(ctrl)
{
    // library
    REGISTER_RPC_METHOD("library_scan", libraryScan);
    REGISTER_RPC_METHOD("library_get_statistics", libraryGetStatistics);

    // library - artists
    REGISTER_RPC_METHOD("library_get_artists", libraryGetArtists);

    // library - albums
    REGISTER_RPC_METHOD("library_get_albums", libraryGetAlbums);
    REGISTER_RPC_METHOD("library_get_albums_by_artist", libraryGetAlbumsByArtist);

    // library - files
    REGISTER_RPC_METHOD("library_get_files_of_artist", libraryGetFilesOfArtist);
    REGISTER_RPC_METHOD("library_get_files_of_album", libraryGetFilesOfAlbum);

    // library - directories
    REGISTER_RPC_METHOD("library_list_directory", libraryListDirectory);

    // library - metadata
    REGISTER_RPC_METHOD("library_get_metadata", libraryGetMetadata);
    REGISTER_RPC_METHOD("library_update_metadata", libraryUpdateMetadata);

    // player queue
    REGISTER_RPC_METHOD("player_queue_file", playerQueueFile);
    REGISTER_RPC_METHOD("player_queue_directory", playerQueueDirectory);
    REGISTER_RPC_METHOD("player_queue_album", playerQueueAlbum);
    REGISTER_RPC_METHOD("player_queue_get", playerQueueGet);
    REGISTER_RPC_METHOD("player_queue_remove", playerQueueRemove);
    REGISTER_RPC_METHOD("player_queue_remove_all", playerQueueRemoveAll);

    // player status
    REGISTER_RPC_METHOD("player_status", playerStatus);

    // player control
    REGISTER_RPC_METHOD("player_play", playerPlay);
    REGISTER_RPC_METHOD("player_pause", playerPause);
    REGISTER_RPC_METHOD("player_stop", playerStop);
    REGISTER_RPC_METHOD("player_seek", playerSeek);
    REGISTER_RPC_METHOD("player_prev", playerPrev);
    REGISTER_RPC_METHOD("player_next", playerNext);
    REGISTER_RPC_METHOD("player_goto", playerGoto);

    // player volume
    REGISTER_RPC_METHOD("player_get_volume", playerGetVolume);
    REGISTER_RPC_METHOD("player_set_volume", playerSetVolume);
    REGISTER_RPC_METHOD("player_inc_volume", playerIncVolume);
    REGISTER_RPC_METHOD("player_dec_volume", playerDecVolume);
}

// =====================================================================================================================
void Server::start(const Json::Value& config, zeppelin::plugin::PluginManager& pm)
{
    if (!config.isMember("path") || !config["path"].isString())
    {
	LOG("jsonrpc-remote: path not configured properly");
	return;
    }

    try
    {
	httpserver::HttpServer& httpServer = static_cast<httpserver::HttpServer&>(pm.getInterface("http-server"));

	if (httpServer.version() != HTTP_SERVER_VERSION)
	{
	    LOG("jsonrpc-remote: invalid http-server plugin version!");
	    return;
	}

	httpServer.registerHandler(config["path"].asString(),
	    std::bind(&Server::processRequest, this, std::placeholders::_1));
    }
    catch (const zeppelin::plugin::PluginInterfaceNotFoundException&)
    {
	LOG("jsonrpc-remote: http-server interface not found");
    }
}

// =====================================================================================================================
void Server::stop()
{
}

// =====================================================================================================================
static inline std::unique_ptr<httpserver::HttpResponse> createJsonErrorReply(const httpserver::HttpRequest& httpReq,
									     const Json::Value& request,
									     const std::string& reason)
{
    Json::Value response(Json::objectValue);
    response["jsonrpc"] = "2.0";
    response["error"] = reason;

    if (request.isMember("id"))
	response["id"] = request["id"];
    else
	response["id"] = Json::Value(Json::nullValue);

    return httpReq.createBufferedResponse(200,
					  Json::FastWriter().write(response));
}

// =====================================================================================================================
std::unique_ptr<httpserver::HttpResponse> Server::processRequest(const httpserver::HttpRequest& request)
{
    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(request.getData(), root))
	return createJsonErrorReply(request, root, "invalid request");

    if (!root.isMember("method") || !root.isMember("id"))
	return createJsonErrorReply(request, root, "method/id not found");

    Json::Value params;

    if (root.isMember("params"))
	params = root["params"];

    Json::Value result;
    std::string method = root["method"].asString();

    auto it = m_rpcMethods.find(method);

    if (it == m_rpcMethods.end())
	return createJsonErrorReply(request, root, "invalid method");

    try
    {
	it->second(params, result);
    }
    catch (...)
    {
	return createJsonErrorReply(request, root, "invalid method call");
    }

    Json::Value response(Json::objectValue);
    response["jsonrpc"] = "2.0";
    response["id"] = root["id"];
    response["result"] = result;

    std::unique_ptr<httpserver::HttpResponse> resp = request.createBufferedResponse(200,
										    Json::FastWriter().write(response));
    resp->addHeader("Content-Type", "application/json;charset=utf-8");
    return resp;
}

// =====================================================================================================================
void Server::libraryScan(const Json::Value& request, Json::Value& response)
{
    m_library->scan();
}

// =====================================================================================================================
void Server::libraryGetStatistics(const Json::Value& request, Json::Value& response)
{
    auto stat = m_library->getStorage().getStatistics();

    response = Json::Value(Json::objectValue);
    response["num_of_artists"] = stat.m_numOfArtists;
    response["num_of_albums"] = stat.m_numOfAlbums;
    response["num_of_files"] = stat.m_numOfFiles;
    response["sum_of_song_length"] = stat.m_sumOfSongLength;
}

// =====================================================================================================================
void Server::libraryGetArtists(const Json::Value& request, Json::Value& response)
{
    auto artists = m_library->getStorage().getArtists();

    response = Json::Value(Json::arrayValue);
    response.resize(artists.size());

    for (Json::Value::ArrayIndex i = 0; i < artists.size(); ++i)
    {
	const auto& a = artists[i];

	Json::Value artist(Json::objectValue);
	artist["id"] = a->m_id;
	artist["name"] = a->m_name;
	artist["albums"] = a->m_albums;

	response[i].swap(artist);
    }
}

// =====================================================================================================================
void Server::libraryGetAlbums(const Json::Value& request, Json::Value& response)
{
    auto albums = m_library->getStorage().getAlbums();

    response = Json::Value(Json::arrayValue);
    response.resize(albums.size());

    for (Json::Value::ArrayIndex i = 0; i < albums.size(); ++i)
    {
	const auto& a = albums[i];

	Json::Value album(Json::objectValue);
	album["id"] = a->m_id;
	album["name"] = a->m_name;
	album["artist"] = a->m_artist;
	album["songs"] = a->m_songs;
	album["length"] = a->m_length;

	response[i].swap(album);
    }
}

// =====================================================================================================================
void Server::libraryGetAlbumsByArtist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "artist_id", Json::intValue);

    auto albums = m_library->getStorage().getAlbumsByArtist(request["artist_id"].asInt());

    response = Json::Value(Json::arrayValue);
    response.resize(albums.size());

    for (Json::Value::ArrayIndex i = 0; i < albums.size(); ++i)
    {
	const auto& a = albums[i];

	Json::Value album(Json::objectValue);
	album["id"] = a->m_id;
	album["name"] = a->m_name;
	album["songs"] = a->m_songs;
	album["length"] = a->m_length;

	response[i].swap(album);
    }
}

// =====================================================================================================================
static inline void serializeFile(Json::Value& file, const zeppelin::library::File& f)
{
    file["id"] = f.m_id;
    file["path"] = f.m_path;
    file["name"] = f.m_name;
    file["length"] = f.m_length;
    file["title"] = f.m_title;
    file["year"] = f.m_year;
    file["track_index"] = f.m_trackIndex;
    file["codec"] = f.m_codec;
    file["artist_id"] = f.m_artistId;
    file["album_id"] = f.m_albumId;
    file["sampling_rate"] = f.m_samplingRate;
}

// =====================================================================================================================
void Server::libraryGetFilesOfArtist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "artist_id", Json::intValue);

    auto files = m_library->getStorage().getFilesOfArtist(request["artist_id"].asInt());

    response = Json::Value(Json::arrayValue);
    response.resize(files.size());

    for (Json::Value::ArrayIndex i = 0; i < files.size(); ++i)
    {
	Json::Value file(Json::objectValue);
	serializeFile(file, *files[i]);

	response[i].swap(file);
    }
}

// =====================================================================================================================
void Server::libraryGetFilesOfAlbum(const Json::Value& request, Json::Value& response)
{
    requireType(request, "album_id", Json::intValue);

    auto files = m_library->getStorage().getFilesOfAlbum(request["album_id"].asInt());

    response = Json::Value(Json::arrayValue);
    response.resize(files.size());

    for (Json::Value::ArrayIndex i = 0; i < files.size(); ++i)
    {
	Json::Value file(Json::objectValue);
	serializeFile(file, *files[i]);

	response[i].swap(file);
    }
}

// =====================================================================================================================
void Server::libraryListDirectory(const Json::Value& request, Json::Value& response)
{
    requireType(request, "directory_id", Json::intValue);

    int directoryId = request["directory_id"].asInt();

    auto directories = m_library->getStorage().listSubdirectories(directoryId);
    auto files = m_library->getStorage().getFilesOfDirectory(directoryId);

    response = Json::Value(Json::arrayValue);
    response.resize(directories.size() + files.size());

    Json::Value::ArrayIndex i = 0;

    // subdirectories
    for (const auto& d : directories)
    {
	Json::Value dir(Json::objectValue);
	dir["type"] = "dir";
	dir["id"] = d->m_id;
	dir["name"] = d->m_name;

	response[i++].swap(dir);
    }

    // files
    for (const auto& f : files)
    {
	Json::Value file(Json::objectValue);
	file["type"] = "file";
	serializeFile(file, *f);

	response[i++].swap(file);
    }
}

// =====================================================================================================================
void Server::libraryGetMetadata(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    auto file = m_library->getStorage().getFile(request["id"].asInt());

    response = Json::Value(Json::objectValue);
    response["artist"] = file->m_artist;
    response["album"] = file->m_album;
    response["title"] = file->m_title;
    response["year"] = file->m_year;
    response["track_index"] = file->m_trackIndex;
    response["id"] = file->m_id;
    response["name"] = file->m_name;
}

// =====================================================================================================================
void Server::libraryUpdateMetadata(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    zeppelin::library::File file(request["id"].asInt());

    file.m_artist = request["artist"].asString();
    file.m_album = request["album"].asString();
    file.m_title = request["title"].asString();
    file.m_year = request["year"].asInt();
    file.m_trackIndex = request["track_index"].asInt();

    m_library->getStorage().updateFileMetadata(file);
}

// =====================================================================================================================
void Server::playerQueueFile(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    auto file = m_library->getStorage().getFile(request["id"].asInt());

    m_ctrl->queue(file);
}

// =====================================================================================================================
void Server::playerQueueDirectory(const Json::Value& request, Json::Value& response)
{
    requireType(request, "directory_id", Json::intValue);

    int directoryId = request["directory_id"].asInt();

    auto directory = m_library->getStorage().getDirectory(directoryId);
    auto files = m_library->getStorage().getFilesOfDirectory(directoryId);

    m_ctrl->queue(directory, files);
}

// =====================================================================================================================
void Server::playerQueueAlbum(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    int albumId = request["id"].asInt();

    auto album = m_library->getStorage().getAlbum(albumId);
    auto files = m_library->getStorage().getFilesOfAlbum(albumId);

    m_ctrl->queue(album, files);
}

// =====================================================================================================================
static inline void serializeQueueItem(Json::Value& parent, const std::shared_ptr<zeppelin::player::QueueItem>& item)
{
    Json::Value qi(Json::objectValue);

    switch (item->type())
    {
	case zeppelin::player::QueueItem::PLAYLIST :
	    break;

	case zeppelin::player::QueueItem::DIRECTORY :
	{
	    const zeppelin::player::Directory& di = static_cast<const zeppelin::player::Directory&>(*item);
	    const zeppelin::library::Directory& directory = di.directory();

	    qi["type"] = "dir";
	    qi["id"] = directory.m_id;
	    qi["name"] = directory.m_name;
	    qi["files"] = Json::Value(Json::arrayValue);

	    for (const auto& i : di.items())
		serializeQueueItem(qi["files"], i);

	    break;
	}

	case zeppelin::player::QueueItem::ALBUM :
	{
	    const zeppelin::player::Album& ai = static_cast<const zeppelin::player::Album&>(*item);
	    const zeppelin::library::Album& album = ai.album();

	    qi["type"] = "album";
	    qi["id"] = album.m_id;
	    qi["name"] = album.m_name;
	    qi["files"] = Json::Value(Json::arrayValue);

	    for (const auto& i : ai.items())
		serializeQueueItem(qi["files"], i);

	    break;
	}

	case zeppelin::player::QueueItem::FILE :
	{
	    auto file = item->file();

	    qi["type"] = "file";
	    qi["id"] = file->m_id;
	    qi["path"] = file->m_path;
	    qi["name"] = file->m_name;
	    qi["title"] = file->m_title;
	    qi["length"] = file->m_length;
	    qi["codec"] = file->m_codec;
	    qi["sampling_rate"] = file->m_samplingRate;

	    break;
	}
    }

    parent.append(qi);
}

// =====================================================================================================================
void Server::playerQueueGet(const Json::Value& request, Json::Value& response)
{
    auto queue = m_ctrl->getQueue();

    response = Json::Value(Json::arrayValue);

    for (const auto& item : queue->items())
	serializeQueueItem(response, item);
}

// =====================================================================================================================
void Server::playerQueueRemove(const Json::Value& request, Json::Value& response)
{
    requireType(request, "index", Json::arrayValue);

    Json::Value index = request["index"];

    std::vector<int> i;

    for (Json::Value::ArrayIndex j = 0; j < index.size(); ++j)
    {
	const Json::Value& item = index[j];

	// make sure index contains only integers
	if (!item.isInt())
	    throw InvalidMethodCall();

	i.push_back(item.asInt());
    }

    m_ctrl->remove(i);
}

// =====================================================================================================================
void Server::playerQueueRemoveAll(const Json::Value& request, Json::Value& response)
{
    m_ctrl->removeAll();
}

// =====================================================================================================================
void Server::playerStatus(const Json::Value& request, Json::Value& response)
{
    zeppelin::player::Controller::Status s = m_ctrl->getStatus();

    response = Json::Value(Json::objectValue);

    response["current"] = s.m_file ? Json::Value(s.m_file->m_id) : Json::Value(Json::nullValue);
    response["state"] = static_cast<int>(s.m_state);
    response["position"] = s.m_position;
    response["volume"] = s.m_volume;
    response["index"] = Json::Value(Json::arrayValue);
    response["index"].resize(s.m_index.size());
    for (Json::Value::ArrayIndex i = 0; i < s.m_index.size(); ++i)
	response["index"][i] = s.m_index[i];
}

// =====================================================================================================================
void Server::playerPlay(const Json::Value& request, Json::Value& response)
{
    m_ctrl->play();
}

// =====================================================================================================================
void Server::playerPause(const Json::Value& request, Json::Value& response)
{
    m_ctrl->pause();
}

// =====================================================================================================================
void Server::playerStop(const Json::Value& request, Json::Value& response)
{
    m_ctrl->stop();
}

// =====================================================================================================================
void Server::playerSeek(const Json::Value& request, Json::Value& response)
{
    requireType(request, "seconds", Json::intValue);

    m_ctrl->seek(request["seconds"].asInt());
}

// =====================================================================================================================
void Server::playerPrev(const Json::Value& request, Json::Value& response)
{
    m_ctrl->prev();
}

// =====================================================================================================================
void Server::playerNext(const Json::Value& request, Json::Value& response)
{
    m_ctrl->next();
}

// =====================================================================================================================
void Server::playerGoto(const Json::Value& request, Json::Value& response)
{
    requireType(request, "index", Json::arrayValue);

    Json::Value index = request["index"];

    std::vector<int> i;

    for (Json::UInt j = 0; j < index.size(); ++j)
    {
	const Json::Value& item = index[j];

	// make sure index contains only integers
	if (!item.isInt())
	    throw InvalidMethodCall();

	i.push_back(item.asInt());
    }

    m_ctrl->goTo(i);
}

// =====================================================================================================================
void Server::playerGetVolume(const Json::Value& request, Json::Value& response)
{
    response = m_ctrl->getVolume();
}

// =====================================================================================================================
void Server::playerSetVolume(const Json::Value& request, Json::Value& response)
{
    requireType(request, "level", Json::intValue);

    m_ctrl->setVolume(request["level"].asInt());
}

// =====================================================================================================================
void Server::playerIncVolume(const Json::Value& request, Json::Value& response)
{
    m_ctrl->incVolume();
}

// =====================================================================================================================
void Server::playerDecVolume(const Json::Value& request, Json::Value& response)
{
    m_ctrl->decVolume();
}

// =====================================================================================================================
void Server::requireType(const Json::Value& request, const std::string& key, Json::ValueType type)
{
    if (!request.isMember(key) || request[key].type() != type)
	throw InvalidMethodCall();
}
