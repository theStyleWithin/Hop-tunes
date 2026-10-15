// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

#include <sqlite3.h>

namespace hop::sql {

// Small RAII wrapper around a prepared SQLite statement.
class Stmt {
public:
    Stmt(sqlite3 *db, const char *sql) { sqlite3_prepare_v2(db, sql, -1, &m_s, nullptr); }
    ~Stmt() { sqlite3_finalize(m_s); }
    Stmt(const Stmt &) = delete;
    Stmt &operator=(const Stmt &) = delete;

    bool ok() const { return m_s != nullptr; }
    void bind(int i, const QString &v)
    {
        const QByteArray u = v.toUtf8();
        sqlite3_bind_text(m_s, i, u.constData(), int(u.size()), SQLITE_TRANSIENT);
    }
    void bind(int i, qint64 v) { sqlite3_bind_int64(m_s, i, v); }
    void bind(int i, int v) { sqlite3_bind_int(m_s, i, v); }
    int step() { return sqlite3_step(m_s); }
    bool run() { return sqlite3_step(m_s) == SQLITE_DONE; }
    void reset() { sqlite3_reset(m_s); sqlite3_clear_bindings(m_s); }

    qint64 i64(int c) const { return sqlite3_column_int64(m_s, c); }
    int i32(int c) const { return sqlite3_column_int(m_s, c); }
    QString str(int c) const
    {
        const auto *t = reinterpret_cast<const char *>(sqlite3_column_text(m_s, c));
        return t ? QString::fromUtf8(t, sqlite3_column_bytes(m_s, c)) : QString();
    }

private:
    sqlite3_stmt *m_s = nullptr;
};

} // namespace hop::sql
