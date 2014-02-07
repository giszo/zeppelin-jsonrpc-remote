#include "server.h"

#include <zeppelin/logger.h>
#include <zeppelin/plugin/pluginmanager.h>
#include <zeppelin/library/storage.h>
#include <zeppelin/player/queue.h>

#include <jsoncpp/json/reader.h>
#include <jsoncpp/json/writer.h>

#include <boost/lexical_cast.hpp>

#define REGISTER_RPC_METHOD(name, function) \
    m_rpcMethods[name] = std::bind(&Server::function, this, std::placeholders::_1, std::placeholders::_2)

struct FilenameComparator
{
    bool operator()(const std::shared_ptr<zeppelin::library::File>& f1, const std::shared_ptr<zeppelin::library::File>& f2)
    {
	return f1->m_name < f2->m_name;
    }
};

struct TrackIndexComparator
{
    bool operator()(const std::shared_ptr<zeppelin::library::File>& f1, const std::shared_ptr<zeppelin::library::File>& f2)
    {
	return f1->m_trackIndex < f2->m_trackIndex;
    }
};

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
    REGISTER_RPC_METHOD("library_get_album_ids_by_artist", libraryGetAlbumIdsByArtist);
    REGISTER_RPC_METHOD("library_get_albums", libraryGetAlbums);

    // library - files
    REGISTER_RPC_METHOD("library_get_files", libraryGetFiles);
    REGISTER_RPC_METHOD("library_get_file_ids_of_album", libraryGetFileIdsOfAlbum);

    // library - directories
    REGISTER_RPC_METHOD("library_get_directories", libraryGetDirectories);
    REGISTER_RPC_METHOD("library_list_directory", libraryListDirectory);

    // library - metadata
    REGISTER_RPC_METHOD("library_update_metadata", libraryUpdateMetadata);

    // libray - playlists
    REGISTER_RPC_METHOD("library_create_playlist", libraryCreatePlaylist);
    REGISTER_RPC_METHOD("library_delete_playlist", libraryDeletePlaylist);
    REGISTER_RPC_METHOD("library_add_playlist_item", libraryAddPlaylistItem);
    REGISTER_RPC_METHOD("library_delete_playlist_item", libraryDeletePlaylistItem);
    REGISTER_RPC_METHOD("library_get_playlists", libraryGetPlaylists);

    // player queue
    REGISTER_RPC_METHOD("player_queue_file", playerQueueFile);
    REGISTER_RPC_METHOD("player_queue_directory", playerQueueDirectory);
    REGISTER_RPC_METHOD("player_queue_album", playerQueueAlbum);
    REGISTER_RPC_METHOD("player_queue_playlist", playerQueuePlaylist);
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
    response["sum_of_song_lengths"] = boost::lexical_cast<std::string>(stat.m_sumOfSongLengths);
    response["sum_of_file_sizes"] = boost::lexical_cast<std::string>(stat.m_sumOfFileSizes);
}

// =====================================================================================================================
void Server::libraryGetArtists(const Json::Value& request, Json::Value& response)
{
    std::vector<int> ids;

    requireType(request, "id", Json::arrayValue);

    for (Json::Value::ArrayIndex i = 0; i < request["id"].size(); ++i)
    {
	const Json::Value& v = request["id"][i];

	if (!v.isInt())
	    throw InvalidMethodCall();

	ids.push_back(v.asInt());
    }

    auto artists = m_library->getStorage().getArtists(ids);

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
    std::vector<int> ids;

    requireType(request, "id", Json::arrayValue);

    for (Json::Value::ArrayIndex i = 0; i < request["id"].size(); ++i)
    {
	const Json::Value& v = request["id"][i];

	if (!v.isInt())
	    throw InvalidMethodCall();

	ids.push_back(v.asInt());
    }

    auto albums = m_library->getStorage().getAlbums(ids);

    response = Json::Value(Json::arrayValue);
    response.resize(albums.size());

    for (Json::Value::ArrayIndex i = 0; i < albums.size(); ++i)
    {
	const auto& a = albums[i];

	Json::Value album(Json::objectValue);
	album["id"] = a->m_id;
	album["name"] = a->m_name;
	album["artist_id"] = a->m_artistId;
	album["songs"] = a->m_songs;

	response[i].swap(album);
    }
}

// =====================================================================================================================
void Server::libraryGetAlbumIdsByArtist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "artist_id", Json::intValue);

    auto albumIds = m_library->getStorage().getAlbumIdsByArtist(request["artist_id"].asInt());

    response = Json::Value(Json::arrayValue);
    response.resize(albumIds.size());

    for (Json::Value::ArrayIndex i = 0; i < albumIds.size(); ++i)
	response[i] = albumIds[i];
}

// =====================================================================================================================
void Server::libraryGetFiles(const Json::Value& request, Json::Value& response)
{
    std::vector<int> ids;

    requireType(request, "id", Json::arrayValue);

    for (Json::Value::ArrayIndex i = 0; i < request["id"].size(); ++i)
    {
	const Json::Value& v = request["id"][i];

	if (!v.isInt())
	    throw InvalidMethodCall();

	ids.push_back(v.asInt());
    }

    auto files = m_library->getStorage().getFiles(ids);

    response = Json::Value(Json::arrayValue);
    response.resize(files.size());

    for (Json::Value::ArrayIndex i = 0; i < files.size(); ++i)
    {
	const auto& f = files[i];

	Json::Value file(Json::objectValue);
	file["id"] = f->m_id;
	file["path"] = f->m_path;
	file["name"] = f->m_name;
	file["directory_id"] = f->m_directoryId;
	file["artist_id"] = f->m_artistId;
	file["album_id"] = f->m_albumId;
	file["length"] = f->m_length;
	file["title"] = f->m_title;
	file["year"] = f->m_year;
	file["track_index"] = f->m_trackIndex;
	file["codec"] = f->m_codec;
	file["sample_rate"] = f->m_sampleRate;
	file["sample_size"] = f->m_sampleSize;

	response[i].swap(file);
    }
}

// =====================================================================================================================
void Server::libraryGetFileIdsOfAlbum(const Json::Value& request, Json::Value& response)
{
    requireType(request, "album_id", Json::intValue);

    auto fileIds = m_library->getStorage().getFileIdsOfAlbum(request["album_id"].asInt());

    response = Json::Value(Json::arrayValue);
    response.resize(fileIds.size());

    for (Json::Value::ArrayIndex i = 0; i < fileIds.size(); ++i)
	response[i] = fileIds[i];
}

// =====================================================================================================================
void Server::libraryGetDirectories(const Json::Value& request, Json::Value& response)
{
    std::vector<int> ids;

    requireType(request, "id", Json::arrayValue);

    for (Json::Value::ArrayIndex i = 0; i < request["id"].size(); ++i)
    {
	const Json::Value& v = request["id"][i];

	if (!v.isInt())
	    throw InvalidMethodCall();

	ids.push_back(v.asInt());
    }

    auto directories = m_library->getStorage().getDirectories(ids);

    response = Json::Value(Json::arrayValue);
    response.resize(directories.size());

    for (Json::Value::ArrayIndex i = 0; i < directories.size(); ++i)
    {
	const auto& d = directories[i];

	Json::Value dir(Json::objectValue);
	dir["id"] = d->m_id;
	dir["name"] = d->m_name;
	dir["parent_id"] = d->m_parentId;

	response[i].swap(dir);
    }
}

// =====================================================================================================================
void Server::libraryListDirectory(const Json::Value& request, Json::Value& response)
{
    requireType(request, "directory_id", Json::intValue);

    int directoryId = request["directory_id"].asInt();

    auto directories = m_library->getStorage().listSubdirectories(directoryId);
    auto fileIds = m_library->getStorage().getFileIdsOfDirectory(directoryId);

    Json::Value dirs(Json::arrayValue);
    dirs.resize(directories.size());

    // subdirectories
    for (Json::Value::ArrayIndex i = 0; i < directories.size(); ++i)
    {
	Json::Value dir(Json::objectValue);
	dir["type"] = "dir";
	dir["id"] = directories[i]->m_id;
	dir["name"] = directories[i]->m_name;

	dirs[i].swap(dir);
    }

    Json::Value files(Json::arrayValue);
    files.resize(fileIds.size());

    // files
    for (Json::Value::ArrayIndex i = 0; i < fileIds.size(); ++i)
	files[i] = fileIds[i];

    response = Json::Value(Json::objectValue);
    response["dirs"] = dirs;
    response["files"] = files;
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
void Server::libraryCreatePlaylist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "name", Json::stringValue);

    response = Json::Value(Json::intValue);
    response = m_library->getStorage().createPlaylist(request["name"].asString());
}

// =====================================================================================================================
void Server::libraryDeletePlaylist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    m_library->getStorage().deletePlaylist(request["id"].asInt());
}

// =====================================================================================================================
void Server::libraryAddPlaylistItem(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);
    requireType(request, "type", Json::stringValue);
    requireType(request, "item_id", Json::intValue);

    response = Json::Value(Json::intValue);
    response = m_library->getStorage().addPlaylistItem(request["id"].asInt(),
						       request["type"].asString(),
						       request["item_id"].asInt());
}

// =====================================================================================================================
void Server::libraryDeletePlaylistItem(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    m_library->getStorage().deletePlaylistItem(request["id"].asInt());
}

// =====================================================================================================================
void Server::libraryGetPlaylists(const Json::Value& request, Json::Value& response)
{
    std::vector<int> ids;

    requireType(request, "id", Json::arrayValue);

    for (Json::Value::ArrayIndex i = 0; i < request["id"].size(); ++i)
    {
	const Json::Value& v = request["id"][i];

	if (!v.isInt())
	    throw InvalidMethodCall();

	ids.push_back(v.asInt());
    }

    auto playlists = m_library->getStorage().getPlaylists(ids);

    response = Json::Value(Json::arrayValue);
    response.resize(playlists.size());

    for (Json::Value::ArrayIndex i = 0; i < playlists.size(); ++i)
    {
	const auto& p = playlists[i];

	Json::Value playlist(Json::objectValue);
	playlist["id"] = p->m_id;
	playlist["name"] = p->m_name;

	playlist["items"] = Json::Value(Json::arrayValue);
	playlist["items"].resize(p->m_items.size());

	for (Json::Value::ArrayIndex j = 0; j < p->m_items.size(); ++j)
	{
	    const auto& pi = p->m_items[j];

	    Json::Value item(Json::objectValue);
	    item["id"] = pi.m_id;
	    item["type"] = pi.m_type;
	    item["item_id"] = pi.m_itemId;

	    playlist["items"][j].swap(item);
	}

	response[i].swap(playlist);
    }
}

// =====================================================================================================================
void Server::playerQueueFile(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    auto files = m_library->getStorage().getFiles({request["id"].asInt()});

    if (files.empty())
	throw InvalidMethodCall();

    m_ctrl->queue(std::make_shared<zeppelin::player::File>(files[0]));
}

// =====================================================================================================================
void Server::playerQueueDirectory(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    int directoryId = request["id"].asInt();

    auto directories = m_library->getStorage().getDirectories({directoryId});

    if (directories.empty())
	throw InvalidMethodCall();

    auto fileIds = m_library->getStorage().getFileIdsOfDirectory(directoryId);
    auto files = m_library->getStorage().getFiles(fileIds);
    std::sort(files.begin(), files.end(), FilenameComparator());

    m_ctrl->queue(std::make_shared<zeppelin::player::Directory>(
	directories[0],
	fileIds.empty() ?
	    std::vector<std::shared_ptr<zeppelin::library::File>>() :
	    files));
}

// =====================================================================================================================
void Server::playerQueueAlbum(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    int albumId = request["id"].asInt();

    auto albums = m_library->getStorage().getAlbums({albumId});

    if (albums.empty())
	throw InvalidMethodCall();

    auto fileIds = m_library->getStorage().getFileIdsOfAlbum(albumId);
    auto files = m_library->getStorage().getFiles(fileIds);
    std::sort(files.begin(), files.end(), TrackIndexComparator());

    m_ctrl->queue(std::make_shared<zeppelin::player::Album>(
	albums[0],
	fileIds.empty() ?
	    std::vector<std::shared_ptr<zeppelin::library::File>>() :
	    files));
}

// =====================================================================================================================
void Server::playerQueuePlaylist(const Json::Value& request, Json::Value& response)
{
    requireType(request, "id", Json::intValue);

    auto playlists = m_library->getStorage().getPlaylists({request["id"].asInt()});

    if (playlists.empty())
	throw InvalidMethodCall();

    std::shared_ptr<zeppelin::player::Playlist> p = std::make_shared<zeppelin::player::Playlist>(playlists[0]->m_id);

    for (const auto& item : playlists[0]->m_items)
    {
	if (item.m_type == "file")
	{
	    auto files = m_library->getStorage().getFiles({item.m_itemId});

	    if (!files.empty())
		p->add(std::make_shared<zeppelin::player::File>(files[0]));
	}
	else if (item.m_type == "directory")
	{
	    auto directories = m_library->getStorage().getDirectories({item.m_itemId});

	    if (!directories.empty())
	    {
		auto fileIds = m_library->getStorage().getFileIdsOfDirectory(item.m_itemId);
		auto files = m_library->getStorage().getFiles(fileIds);
		std::sort(files.begin(), files.end(), FilenameComparator());

		p->add(std::make_shared<zeppelin::player::Directory>(
		    directories[0],
		    fileIds.empty() ?
		        std::vector<std::shared_ptr<zeppelin::library::File>>() :
		        files));
	    }
	}
	else if (item.m_type == "album")
	{
	    auto albums = m_library->getStorage().getAlbums({item.m_itemId});

	    if (!albums.empty())
	    {
		auto fileIds = m_library->getStorage().getFileIdsOfAlbum(item.m_itemId);
		auto files = m_library->getStorage().getFiles(fileIds);
		std::sort(files.begin(), files.end(), TrackIndexComparator());

		p->add(std::make_shared<zeppelin::player::Album>(
		    albums[0],
		    fileIds.empty() ?
		        std::vector<std::shared_ptr<zeppelin::library::File>>() :
		        files));
	    }
	}
	else
	    LOG("jsonrpc-remote: invalid playlist item: " << item.m_type);
    }

    m_ctrl->queue(p);
}

// =====================================================================================================================
static inline void serializeQueueItem(Json::Value& parent, const std::shared_ptr<zeppelin::player::QueueItem>& item)
{
    Json::Value qi(Json::objectValue);

    switch (item->type())
    {
	case zeppelin::player::QueueItem::PLAYLIST :
	{
	    const zeppelin::player::Playlist& pl = static_cast<const zeppelin::player::Playlist&>(*item);

	    qi["type"] = "playlist";
	    qi["id"] = pl.getId();
	    qi["items"] = Json::Value(Json::arrayValue);

	    for (const auto& i : pl.items())
		serializeQueueItem(qi["items"], i);

	    break;
	}

	case zeppelin::player::QueueItem::DIRECTORY :
	{
	    const zeppelin::player::Directory& di = static_cast<const zeppelin::player::Directory&>(*item);
	    const zeppelin::library::Directory& directory = di.directory();

	    qi["type"] = "directory";
	    qi["id"] = directory.m_id;
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
void Server::requireType(const Json::Value& request, const std::string& key, Json::ValueType type)
{
    if (!request.isMember(key) || request[key].type() != type)
	throw InvalidMethodCall();
}
