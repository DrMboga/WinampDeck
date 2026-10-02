// go-librespot's /events and /status payloads, as EngineClient::Listener calls.

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

#include "adapters/librespot_events.hpp"

namespace winampdeck {
namespace {

using namespace std::chrono_literals;

class RecordingListener final : public EngineClient::Listener {
public:
    std::vector<std::string> calls;
    SpotifyTrack track;

    void onSpotifyActiveChanged(bool active) override { calls.push_back(active ? "active" : "inactive"); }
    void onSpotifyPlayingChanged(bool playing) override { calls.push_back(playing ? "playing" : "not playing"); }
    void onSpotifyTrackChanged(const SpotifyTrack& changed) override {
        track = changed;
        calls.push_back("track " + changed.title);
    }
    void onSpotifyPositionChanged(std::chrono::milliseconds position) override {
        calls.push_back("position " + std::to_string(position.count()));
    }
    void onSpotifyShuffleChanged(bool enabled) override { calls.push_back(enabled ? "shuffle on" : "shuffle off"); }
    void onSpotifyRepeatChanged(bool enabled) override { calls.push_back(enabled ? "repeat on" : "repeat off"); }
};

using Calls = std::vector<std::string>;

// A metadata event as go-librespot sends it (ApiTrack fields).
constexpr const char* kMetadata = R"({"type":"metadata","data":{
    "uri":"spotify:track:0DiWol3AO6WpXZgp0goxAV","name":"One More Time",
    "artist_names":["Daft Punk"],"artist_uris":["spotify:artist:4tZwfgrHOc3mvqYlEYSvVi"],
    "album_name":"Discovery","album_uri":"spotify:album:2noRn2Aes5aoNVsU6iWThc",
    "album_cover_url":"https://i.scdn.co/image/ab67616d0000b2731e81bff9807a9e629fce5ade",
    "position":1234,"duration":320357,"release_date":"2001-03-12","track_number":1,
    "disc_number":1,"format":"OGG_VORBIS_320","codec":"vorbis","bitrate":320,
    "sample_rate":44100,"bit_depth":null}})";

TEST(LibrespotEvents, ActiveAndInactive) {
    RecordingListener listener;
    EXPECT_TRUE(dispatchLibrespotEvent(R"({"type":"active","data":null})", listener));
    EXPECT_TRUE(dispatchLibrespotEvent(R"({"type":"inactive","data":null})", listener));
    EXPECT_EQ(listener.calls, (Calls{"active", "inactive"}));
}

TEST(LibrespotEvents, MetadataIsTheTrackAndItsPosition) {
    RecordingListener listener;
    EXPECT_TRUE(dispatchLibrespotEvent(kMetadata, listener));
    EXPECT_EQ(listener.calls, (Calls{"track One More Time", "position 1234"}));
    EXPECT_EQ(listener.track, (SpotifyTrack{"Daft Punk", "One More Time", "Discovery", 320357ms,
                                            "https://i.scdn.co/image/ab67616d0000b2731e81bff9807a9e629fce5ade"}));
}

TEST(LibrespotEvents, SeveralArtistsAreJoined) {
    RecordingListener listener;
    dispatchLibrespotEvent(
        R"({"type":"metadata","data":{"name":"Get Lucky","artist_names":["Daft Punk","Pharrell Williams"]}})",
        listener);
    EXPECT_EQ(listener.track.artist, "Daft Punk, Pharrell Williams");
}

TEST(LibrespotEvents, NullCoverIsNoCover) {
    RecordingListener listener;
    dispatchLibrespotEvent(R"({"type":"metadata","data":{"name":"Episode","album_cover_url":null}})", listener);
    EXPECT_EQ(listener.track.coverUrl, "");
}

TEST(LibrespotEvents, PlayingStates) {
    RecordingListener listener;
    for (const char* type : {"playing", "paused", "playing", "not_playing", "playing", "stopped"}) {
        EXPECT_TRUE(dispatchLibrespotEvent(std::string(R"({"type":")") + type +
                                               R"(","data":{"uri":"spotify:track:x","play_origin":"go-librespot"}})",
                                           listener));
    }
    EXPECT_EQ(listener.calls,
              (Calls{"playing", "not playing", "playing", "not playing", "playing", "not playing"}));
}

TEST(LibrespotEvents, SeekIsThePosition) {
    RecordingListener listener;
    dispatchLibrespotEvent(R"({"type":"seek","data":{"uri":"spotify:track:x","position":90000,"duration":320357}})",
                           listener);
    EXPECT_EQ(listener.calls, (Calls{"position 90000"}));
}

TEST(LibrespotEvents, ShuffleAndRepeatContext) {
    RecordingListener listener;
    dispatchLibrespotEvent(R"({"type":"shuffle_context","data":{"value":true}})", listener);
    dispatchLibrespotEvent(R"({"type":"repeat_context","data":{"value":true}})", listener);
    dispatchLibrespotEvent(R"({"type":"repeat_context","data":{"value":false}})", listener);
    EXPECT_EQ(listener.calls, (Calls{"shuffle on", "repeat on", "repeat off"}));
}

TEST(LibrespotEvents, OtherTypesChangeNothing) {
    RecordingListener listener;
    EXPECT_TRUE(dispatchLibrespotEvent(R"({"type":"will_play","data":{"uri":"spotify:track:x"}})", listener));
    EXPECT_TRUE(dispatchLibrespotEvent(R"({"type":"volume","data":{"value":40,"max":100}})", listener));
    EXPECT_TRUE(dispatchLibrespotEvent(R"({"type":"repeat_track","data":{"value":true}})", listener));
    EXPECT_TRUE(listener.calls.empty());
}

TEST(LibrespotEvents, MalformedMessagesAreRejected) {
    RecordingListener listener;
    EXPECT_FALSE(dispatchLibrespotEvent("not json", listener));
    EXPECT_FALSE(dispatchLibrespotEvent(R"({"data":{}})", listener));
    EXPECT_FALSE(dispatchLibrespotEvent(R"({"type":"metadata","data":null})", listener));
    EXPECT_TRUE(listener.calls.empty());
}

TEST(LibrespotStatus, PlayingSessionReplaysAsEvents) {
    RecordingListener listener;
    EXPECT_TRUE(dispatchLibrespotStatus(R"({
        "username":"someone","device_id":"abc","device_type":"SPEAKER","device_name":"WinampDeck",
        "play_origin":"spotify","stopped":false,"paused":false,"buffering":false,
        "volume":50,"volume_steps":100,"repeat_context":true,"repeat_track":false,"shuffle_context":false,
        "track":{"name":"One More Time","artist_names":["Daft Punk"],"position":61000,"duration":320357}})",
                                        listener));
    EXPECT_EQ(listener.calls, (Calls{"active", "track One More Time", "position 61000", "shuffle off",
                                     "repeat on", "playing"}));
}

TEST(LibrespotStatus, PausedSession) {
    RecordingListener listener;
    dispatchLibrespotStatus(R"({"stopped":false,"paused":true,"track":null})", listener);
    EXPECT_EQ(listener.calls, (Calls{"active", "shuffle off", "repeat off", "not playing"}));
}

TEST(LibrespotStatus, StoppedSessionIsInactive) {
    RecordingListener listener;
    dispatchLibrespotStatus(R"({"stopped":true,"paused":false,"track":null})", listener);
    EXPECT_EQ(listener.calls, (Calls{"inactive", "shuffle off", "repeat off", "not playing"}));
}

TEST(LibrespotStatus, NoSession) {
    RecordingListener listener;
    dispatchLibrespotNoSession(listener);
    EXPECT_EQ(listener.calls, (Calls{"inactive", "not playing"}));
}

}  // namespace
}  // namespace winampdeck
