// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "library/EpubBook.h"
#include "net/AppNet.h"

#include <QJsonObject>
#include <QObject>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include <memory>

namespace hop {

// The Internet Archive app: find and play music, watch videos and read books that are on archive.org, through the gateway (net/AppNet), only while
// the app is open and only after the person allowed it. No account, nothing to register. One curator is shown first, the way Kick shows its
// developer first: normand64, "the media preserver", through his public favourites collection (his uploads are not listed by any public
// interface). What the person keeps (favourites) stays in their profile on this device. A book is read from a temporary copy that is deleted when
// the book is closed; nothing else is saved. Not made or endorsed by the Internet Archive; items carry their own rights and are only shown.
class ArchiveClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isOpen READ isOpen NOTIFY openChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)
    Q_PROPERTY(QString query READ query NOTIFY changed)
    Q_PROPERTY(QString kind READ kind WRITE setKind NOTIFY changed)               // "all" | "audio" | "movies" | "texts"
    Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)                 // the curator's matches first (featured: true), then the rest
    Q_PROPERTY(int featuredCount READ featuredCount NOTIFY itemsChanged)
    Q_PROPERTY(bool canLoadMore READ canLoadMore NOTIFY itemsChanged)
    Q_PROPERTY(QVariantMap detail READ detail NOTIFY detailChanged)               // the opened item: {} when none
    Q_PROPERTY(bool detailLoading READ detailLoading NOTIFY detailChanged)
    Q_PROPERTY(QVariantList favourites READ favourites NOTIFY favouritesChanged)
    Q_PROPERTY(bool bookLoading READ bookLoading NOTIFY bookChanged)
    Q_PROPERTY(bool bookReady READ bookReady NOTIFY bookChanged)
    Q_PROPERTY(QString bookError READ bookError NOTIFY bookChanged)
    Q_PROPERTY(QString bookTitle READ bookTitle NOTIFY bookChanged)
    Q_PROPERTY(int bookChapters READ bookChapters NOTIFY bookChanged)
    Q_PROPERTY(QString curatorName READ curatorName CONSTANT)
    Q_PROPERTY(QString curatorTagline READ curatorTagline CONSTANT)
    Q_PROPERTY(QString curatorUrl READ curatorUrl CONSTANT)
public:
    static constexpr const char *kApp = "archive";
    static constexpr int kPage = 40, kCuratorRows = 60, kMaxFavourites = 500, kMaxBookBytes = 60 * 1024 * 1024, kMaxTextBytes = 12 * 1024 * 1024;
    static QString curatorCollection() { return QStringLiteral("fav-normand64"); }

    explicit ArchiveClient(AppNet *net, QObject *parent = nullptr);

    // what the answers are made of (public so the tests can drive them without a screen)
    static QVariantList parseSearch(const QByteArray &json, int *total = nullptr);                  // [{id, title, creator, kind, year, downloads, thumb}]
    static QVariantMap parseDetail(const QByteArray &json, const QString &id);                       // {} when it is not a usable item
    static bool validId(const QString &id);
    static QString downloadUrl(const QString &id, const QString &file);
    static QString searchUrl(const QString &text, const QString &kind, int page, bool curatorOnly);

    bool isOpen() const { return m_open; }
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    QString query() const { return m_query; }
    QString kind() const { return m_kind; }
    void setKind(const QString &k);
    QVariantList items() const { return m_items; }
    int featuredCount() const { return m_featured; }
    bool canLoadMore() const { return m_open && !m_loading && m_loaded < m_total; }
    QVariantMap detail() const { return m_detail; }
    bool detailLoading() const { return m_detailLoading; }
    QVariantList favourites() const { return m_favs; }
    bool bookLoading() const { return m_bookLoading; }
    bool bookReady() const { return m_bookReady; }
    QString bookError() const { return m_bookError; }
    QString bookTitle() const { return m_bookTitle; }
    int bookChapters() const { return m_bookReady ? (m_epubOpen ? m_epub.chapterCount() : m_pages.size()) : 0; }
    QString curatorName() const { return QStringLiteral("normand64"); }
    QString curatorTagline() const { return QStringLiteral("the media preserver"); }
    QString curatorUrl() const { return QStringLiteral("https://archive.org/details/@normand64"); }

    Q_INVOKABLE void open();                          // connects (the permission was given) and shows what is popular, the curator first
    Q_INVOKABLE void close();
    Q_INVOKABLE void search(const QString &text);     // "" = popular
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void openItem(const QString &id);
    Q_INVOKABLE void closeItem();
    Q_INVOKABLE bool streamAllowed(const QString &url) const;
    Q_INVOKABLE void openBook();                      // the opened item's EPUB, or else its plain text
    Q_INVOKABLE void closeBook();
    Q_INVOKABLE QString chapterTitle(int i);
    Q_INVOKABLE QString chapterHtml(int i);
    Q_INVOKABLE void toggleFavourite(const QVariantMap &item);   // {id, title, creator, kind}
    Q_INVOKABLE bool isFavourite(const QString &id) const;

    QJsonObject listsJson() const;
    void setListsJson(const QJsonObject &o);

signals:
    void openChanged();
    void changed();
    void itemsChanged();
    void detailChanged();
    void favouritesChanged();
    void bookChanged();
    void listsEdited();

private:
    void runSearch(bool append);
    void finishSearch();
    void setMessage(const QString &m);
    static QString clean(const QString &s, int max = 300);
    void setBookError(const QString &e);
    void bookFromText(const QString &text);

    AppNet *m_net;
    bool m_open = false, m_loading = false, m_detailLoading = false;
    quint64 m_gen = 0, m_detailGen = 0, m_bookGen = 0;
    QString m_message, m_query, m_kind = QStringLiteral("all");
    QVariantList m_items, m_favs;
    QVariantList m_curatorBatch, m_generalBatch;
    int m_pending = 0, m_featured = 0, m_page = 1, m_total = 0, m_loaded = 0;
    QVariantMap m_detail;
    // the book being read
    bool m_bookLoading = false, m_bookReady = false, m_epubOpen = false;
    QString m_bookError, m_bookTitle;
    std::unique_ptr<QTemporaryDir> m_bookDir;
    EpubBook m_epub;
    QStringList m_pages;
};

} // namespace hop
