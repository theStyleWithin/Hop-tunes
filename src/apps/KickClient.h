// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "net/AppNet.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace hop {

// The Kick app's data side: a list of live streams to browse, categories, search, one channel, and two lists the person keeps on
// this computer only (channels followed, channels blocked). Every request goes through AppNet (the gateway) and happens because the
// person opened the app, pressed refresh, "show more" or a card; nothing polls in the background and nothing plays by itself.
//
// The addresses are Kick's own website endpoints (the same ones kick.com uses), not an official API: they can change, so every
// failure ends as a plain sentence in `error`, never as a silent blank. See THIRD_PARTY_NOTICES.md (kickNX, twiNX, Switchfin).
class KickClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList streams READ streams NOTIFY streamsChanged)           // what the grid shows (blocked and 18+ already left out)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)  // {id, name, slug}
    Q_PROPERTY(QVariantList playlists READ playlists NOTIFY listsChanged)          // {id, name, kind, count, items}: channels, clips or past streams
    Q_PROPERTY(QVariantList following READ following NOTIFY listsChanged)         // {slug, name, avatar, live, viewers, checked}
    Q_PROPERTY(QStringList blocked READ blocked NOTIFY listsChanged)
    Q_PROPERTY(QVariantList pastStreams READ pastStreams NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY libraryChanged)
    Q_PROPERTY(QString libraryState READ libraryState NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList pastStreams READ pastStreams NOTIFY libraryChanged)
    Q_PROPERTY(QVariantList clips READ clips NOTIFY libraryChanged)
    Q_PROPERTY(QString libraryState READ libraryState NOTIFY libraryChanged)
    Q_PROPERTY(QVariantMap channel READ channel NOTIFY channelChanged)            // the channel being watched / looked at
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY streamsChanged)
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY filtersChanged)        // "viewers" | "featured"
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY filtersChanged)   // "" (all) or a code like "en"; "en" whenever english only is on
    Q_PROPERTY(bool englishOnly READ englishOnly WRITE setEnglishOnly NOTIFY filtersChanged)   // kept in the profile: only English streams, whatever else is chosen
    Q_PROPERTY(int categoryId READ categoryId WRITE setCategoryId NOTIFY filtersChanged) // 0 = all
    Q_PROPERTY(bool showMature READ showMature WRITE setShowMature NOTIFY filtersChanged)
    Q_PROPERTY(QString query READ query NOTIFY filtersChanged)                    // a search is showing when not empty
    Q_PROPERTY(QStringList contacted READ contacted NOTIFY contactedChanged)      // the sites reached since the app was opened
    Q_PROPERTY(bool connected READ isOpen NOTIFY openChanged)
    // What the chat beside the stream is: "off" (the start), "pretend" (Hop-tunes' made-up chat, nobody in it is real) or "kick" (Kick's own
    // live chat). Kick's chat is only for someone who chose to sign in to Kick; without that the choice is no chat or the pretend one.
    Q_PROPERTY(QString chatMode READ chatMode WRITE setChatMode NOTIFY chatModeChanged)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY signedInChanged)
    Q_PROPERTY(QString accountName READ accountName NOTIFY signedInChanged)
    Q_PROPERTY(QString signInState READ signInState NOTIFY signInChanged)           // "" | "waiting" | "error"
    Q_PROPERTY(QString signInCode READ signInCode NOTIFY signInChanged)             // the 6 digits to confirm on kick.com
    Q_PROPERTY(QString signInUrl READ signInUrl NOTIFY signInChanged)               // the kick.com page to open (in the browser) to approve
    Q_PROPERTY(QString signInMessage READ signInMessage NOTIFY signInChanged)
    Q_PROPERTY(QString importMessage READ importMessage NOTIFY signInChanged)
    Q_PROPERTY(bool importing READ importing NOTIFY signInChanged)
public:
    static constexpr const char *kApp = "kick";
    static constexpr int kPageSize = 36;
    // The developer's own channel: when it is live it takes the first card of the default feed (no search, category or language chosen).
    // It is one lookup when the list is loaded, labelled "developer" on its card, and it follows the viewer's own choices: a viewer who blocked
    // the channel, or who keeps 18+ streams hidden, does not get it.
    static constexpr const char *kDeveloperSlug = "gamehopper";

    explicit KickClient(AppNet *net, QObject *parent = nullptr);

    QVariantList streams() const { return m_streams; }
    QVariantList categories() const { return m_categories; }
    QVariantList following() const;
    QVariantList playlists() const;
    QStringList blocked() const { return m_blocked; }
    QVariantMap channel() const { return m_channel; }
    QVariantList pastStreams() const { return m_past; }     // finished broadcasts of the open channel (when asked for)
    QVariantList clips() const { return m_clips; }
    QString libraryState() const { return m_libraryState; } // "" | "loading" | a plain sentence when it could not be loaded
    bool loading() const { return m_pending > 0; }
    QString error() const { return m_error; }
    bool hasMore() const { return !m_next.isEmpty(); }
    QString sort() const { return m_sort; }
    void setSort(const QString &s);
    QString language() const { return m_englishOnly ? QStringLiteral("en") : m_language; }
    bool englishOnly() const { return m_englishOnly; }
    void setEnglishOnly(bool on);
    void setLanguage(const QString &l);
    int categoryId() const { return m_categoryId; }
    void setCategoryId(int id);
    bool showMature() const { return m_showMature; }
    void setShowMature(bool on);
    QString query() const { return m_query; }
    QStringList contacted() const { return m_net->contacted(kApp); }
    bool isOpen() const { return m_open; }
    QString chatMode() const { return m_chatMode; }
    void setChatMode(const QString &mode);
    bool signedIn() const { return m_signedIn; }      // optional: nothing needs it to watch. Signing in only lets you import your follow list.
    QString accountName() const { return m_accountName; }
    QString signInState() const { return m_signInState; }
    QString signInCode() const { return m_signInCode; }
    QString signInUrl() const { return m_signInUrl; }
    QString signInMessage() const { return m_signInMessage; }
    QString importMessage() const { return m_importMessage; }
    bool importing() const { return m_importing; }
    void setSignInPollMs(int ms) { m_signInPollMs = ms; }   // tests

    // the app opens: the gateway opens and the first list is asked for. close() drops everything still on the way.
    Q_INVOKABLE void open();
    Q_INVOKABLE void close();
    Q_INVOKABLE void refresh();                    // the list again from the start (a search stays a search)
    Q_INVOKABLE void loadMore();                   // the next page, only when asked
    Q_INVOKABLE void search(const QString &text);  // "" goes back to the live list
    Q_INVOKABLE void openChannel(const QString &slugOrLink);   // fills `channel` (title, viewers, stream address, chat room)
    Q_INVOKABLE void closeChannel();
    Q_INVOKABLE void loadPastStreams();            // the open channel's finished broadcasts (only when asked)
    Q_INVOKABLE void loadClips();                  // and its clips
    Q_INVOKABLE bool streamAllowed(const QString &url) const;   // the Qt Multimedia player (Android) asks before it opens a stream
    // Optional sign-in, the way Kick's TV-link page works: Hop-tunes shows a 6-digit code and an address on kick.com; you approve it there
    // (signed in to Kick in your own browser) and Hop-tunes is given a session token, kept only in this profile. Nothing needs it to
    // watch; it only lets you import your follow list into the local follow list. Unofficial: Kick can change it.
    Q_INVOKABLE void startSignIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();                    // forgets the token (the local follow list stays)
    Q_INVOKABLE void importFollows();              // reads who you follow on Kick once, and adds them to the local follow list
    Q_INVOKABLE void refreshFollowing();           // one lookup per followed channel, only when asked

    // the two lists kept on this computer only
    Q_INVOKABLE bool follow(const QString &slug);
    Q_INVOKABLE void unfollow(const QString &slug);
    Q_INVOKABLE bool isFollowing(const QString &slug) const { return m_following.contains(normalize(slug)); }
    Q_INVOKABLE bool block(const QString &slug);
    Q_INVOKABLE void unblock(const QString &slug);
    // playlists, kept on this computer only (in the profile): "channels" holds channels, "clips" and "streams" hold clips / past streams
    static constexpr int kMaxPlaylists = 50, kMaxItems = 500, kMaxNameLength = 60;
    Q_INVOKABLE QString createPlaylist(const QString &name, const QString &kind);          // the new id, or "" (empty name, unknown kind, too many)
    Q_INVOKABLE bool renamePlaylist(const QString &id, const QString &name);
    Q_INVOKABLE bool deletePlaylist(const QString &id);
    Q_INVOKABLE bool addToPlaylist(const QString &id, const QVariantMap &item);          // a channel: {slug}; a clip / past stream: {id, kind, title, channel, url, thumb, seconds}
    Q_INVOKABLE bool removeFromPlaylist(const QString &id, const QString &key);          // key = the slug, or the clip / stream id
    Q_INVOKABLE bool isInPlaylist(const QString &id, const QString &key) const;
    Q_INVOKABLE QVariantList playlistsHolding(const QString &key) const;                 // ids of the playlists that have it (for "add to playlist" menus)
    Q_INVOKABLE void refreshPlaylist(const QString &id);                                  // live dots for a channel playlist: one lookup each, only when asked
    Q_INVOKABLE bool isBlocked(const QString &slug) const { return m_blocked.contains(normalize(slug)); }
    Q_INVOKABLE void clearBlocked();
    QJsonObject listsJson() const;                 // saved with the profile
    QJsonArray playlistsJson() const;
    void setListsJson(const QJsonObject &o);       // loaded with the profile

    // ---- the replies, as plain functions (unit-tested with fixtures) ----
    static QString normalize(const QString &slugOrLink);   // "https://kick.com/Name?x" -> "name"; "" when it is not a channel name
    static bool parseLivestreams(const QByteArray &json, QVariantList *out, QString *nextCursor);
    static bool parseCategories(const QByteArray &json, QVariantList *out);
    static bool parseSearch(const QByteArray &json, QVariantList *channels);
    static bool parseChannel(const QByteArray &json, QVariantMap *out);
    static bool parsePastStreams(const QByteArray &json, QVariantList *out);   // finished ones only (the live one is the channel page)
    static bool parseClips(const QByteArray &json, QVariantList *out);
    Q_INVOKABLE static QString viewersText(int n);             // 12400 -> "12.4k"

signals:
    void streamsChanged();
    void categoriesChanged();
    void listsChanged();      // the lists or what is known about them changed (also: live dots)
    void listsEdited();       // the person followed, unfollowed, blocked or unblocked someone: worth saving
    void channelChanged();
    void libraryChanged();
    void loadingChanged();
    void errorChanged();
    void filtersChanged();
    void contactedChanged();
    void openChanged();
    void chatModeChanged();
    void signedInChanged();
    void signInChanged();

private:
    void requestList(bool more);
    void requestCategories();
    void requestDeveloper();
    bool pinApplies() const { return m_query.isEmpty() && m_categoryId == 0 && (m_language.isEmpty() || m_englishOnly); }
    void applyFilters();
    void setError(const QString &e);
    void begin() { ++m_pending; emit loadingChanged(); }
    void end() { if (m_pending > 0) --m_pending; emit loadingChanged(); }
    bool ready();

    AppNet *m_net;
    bool m_open = false;
    int m_pending = 0;
    quint64 m_listGen = 0, m_channelGen = 0;
    QString m_error;
    QVariantList m_raw;          // everything fetched for the current list, before blocked / 18+ are left out
    QVariantList m_streams, m_categories, m_searchChannels;
    QString m_next, m_sort = QStringLiteral("viewers"), m_language, m_query;
    int m_categoryId = 0, m_emptyPages = 0;
    bool m_englishOnly = false;
    bool m_showMature = false;
    QVariantMap m_channel;
    QVariantList m_past, m_clips;
    QString m_libraryState;
    quint64 m_libraryGen = 0;
    QVariantMap m_dev;           // the developer's stream when it is live (else empty)
    QStringList m_following, m_blocked;
    struct Playlist { QString id, name, kind; QVariantList items; };
    QList<Playlist> m_playlists;
    bool channelInPlaylists(const QString &slug) const;
    void refreshChannels(const QStringList &todo);
    QString m_chatMode = QStringLiteral("off");
    bool m_signedIn = false;
    QString m_token, m_accountName, m_signInState, m_signInCode, m_signInUrl, m_signInUuid, m_signInMessage, m_importMessage;
    bool m_importing = false;
    int m_signInPollMs = 3000, m_signInPolls = 0, m_signInFailures = 0;
    quint64 m_signInGen = 0;
    void pollSignIn(quint64 gen);
    void finishSignIn(const QString &token);
    void importPage(quint64 gen, QString cursor, int page, QStringList slugs);
    void setSignInState(const QString &state, const QString &message);
    QHash<QString, QVariantMap> m_status;   // slug -> {live, viewers, name, avatar, checked}
    int m_followChecks = 0;
};

} // namespace hop
