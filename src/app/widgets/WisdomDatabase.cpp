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
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                QStringLiteral("wisdom_conn"));
    db.setDatabaseName(path);
    if (!db.open()) {
      qWarning("WisdomDatabase: failed to open SQLite: %s",
               qPrintable(db.lastError().text()));
      return false;
    }
    return true;
  }

  QVector<PoemData> loadAll() {
    QVector<PoemData> result;
    QSqlDatabase db = QSqlDatabase::database(QStringLiteral("wisdom_conn"));
    if (db.isOpen()) {
      QSqlQuery q(db);
      q.exec(QStringLiteral(
          "SELECT sentence, source, commentary, tag, dynasty FROM poems"));
      while (q.next()) {
        result.append({q.value(0).toString(), q.value(1).toString(),
                       q.value(2).toString(), q.value(3).toString(),
                       q.value(4).toString()});
      }
    }
    return result;
  }

 private:
  void closeDb() {
    if (QSqlDatabase::contains(QStringLiteral("wisdom_conn"))) {
      // Scope the db object so it's destroyed before removeDatabase
      { QSqlDatabase db = QSqlDatabase::database(QStringLiteral("wisdom_conn"));
        if (db.isOpen()) db.close();
      }
      QSqlDatabase::removeDatabase(QStringLiteral("wisdom_conn"));
    }
  }
};

WisdomDatabase::~WisdomDatabase() { delete impl_; }

WisdomDatabase& WisdomDatabase::instance() {
  static WisdomDatabase db;
  return db;
}

QVector<PoemData> WisdomDatabase::initDatabase(const QString& writablePath) {
  // Already initialized — return cached data
  if (impl_) return poems_;

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
    poems_ = impl_->loadAll();
    if (!poems_.isEmpty()) {
      return poems_;
    }
  }

  delete impl_;
  impl_ = nullptr;
  return {};
}
