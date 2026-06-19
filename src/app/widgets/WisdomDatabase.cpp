#include "WisdomDatabase.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

// ═════════════════════════════════════════════════════════════════════════════
//  WisdomDatabase implementation
// ═════════════════════════════════════════════════════════════════════════════

class WisdomDatabase::Impl {
 public:
  ~Impl() { closeDb(); }

  bool openDb(const QString& path) {
    closeDb();
    sqliteDb_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                          QStringLiteral("wisdom_conn"));
    sqliteDb_.setDatabaseName(path);
    if (!sqliteDb_.open()) {
      qWarning("WisdomDatabase: failed to open SQLite: %s",
               qPrintable(sqliteDb_.lastError().text()));
      return false;
    }
    return true;
  }

  QVector<PoemData> loadAll() {
    QVector<PoemData> result;
    QSqlQuery q(sqliteDb_);
    q.exec(QStringLiteral(
        "SELECT sentence, source, commentary, tag, dynasty FROM poems"));
    while (q.next()) {
      result.append({q.value(0).toString(), q.value(1).toString(),
                     q.value(2).toString(), q.value(3).toString(),
                     q.value(4).toString()});
    }
    return result;
  }

 private:
  void closeDb() {
    if (sqliteDb_.isOpen()) {
      sqliteDb_.close();
    }
    if (QSqlDatabase::contains(QStringLiteral("wisdom_conn"))) {
      QSqlDatabase::removeDatabase(QStringLiteral("wisdom_conn"));
    }
  }

  QSqlDatabase sqliteDb_;
};

WisdomDatabase::~WisdomDatabase() { delete impl_; }

WisdomDatabase& WisdomDatabase::instance() {
  static WisdomDatabase db;
  return db;
}

QVector<PoemData> WisdomDatabase::initDatabase(const QString& writablePath) {
  QString dbDir = writablePath + QStringLiteral("/wisdom");
  QDir().mkpath(dbDir);
  QString dbPath = dbDir + QStringLiteral("/poems.db");

  if (!QFile::exists(dbPath)) {
    if (!QFile::copy(QStringLiteral(":/resources/data/poems.db"), dbPath)) {
      qWarning("WisdomDatabase: failed to copy embedded poems.db");
      return {};
    }
    QFile::setPermissions(dbPath, QFile::ReadOwner | QFile::WriteOwner);
  }

  impl_ = new Impl();
  if (impl_->openDb(dbPath)) {
    QVector<PoemData> poems = impl_->loadAll();
    if (!poems.isEmpty()) {
      return poems;
    }
  }

  delete impl_;
  impl_ = nullptr;
  return {};
}
