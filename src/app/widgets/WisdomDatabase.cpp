#include "WisdomDatabase.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

  bool exec(const QString& sql) {
    QSqlQuery q(sqliteDb_);
    return q.exec(sql);
  }

  bool tableExists(const QString& name) {
    QSqlQuery q(sqliteDb_);
    q.exec(QStringLiteral("SELECT count(*) FROM sqlite_master WHERE "
                          "type='table' AND name='%1'")
               .arg(name));
    return q.next() && q.value(0).toInt() > 0;
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

  void insertBatch(const QVector<PoemData>& poems) {
    sqliteDb_.transaction();
    QSqlQuery q(sqliteDb_);
    q.prepare(QStringLiteral("INSERT INTO poems "
                             "(sentence, source, commentary, tag, dynasty) "
                             "VALUES (?, ?, ?, ?, ?)"));
    for (const auto& p : poems) {
      q.addBindValue(p.sentence);
      q.addBindValue(p.source);
      q.addBindValue(p.commentary);
      q.addBindValue(p.tag);
      q.addBindValue(p.dynasty);
      q.exec();
    }
    sqliteDb_.commit();
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

  if (QFileInfo::exists(dbPath)) {
    impl_ = new Impl();
    if (impl_->openDb(dbPath)) {
      QVector<PoemData> poems = impl_->loadAll();
      if (!poems.isEmpty()) {
        return poems;
      }
    }
    delete impl_;
    impl_ = nullptr;
  }

  // Load builtin data from JSON resource
  QVector<PoemData> builtin = loadBuiltinFromJson();

  // Try to create SQLite DB and populate it
  impl_ = new Impl();
  if (impl_->openDb(dbPath)) {
    if (!impl_->tableExists(QStringLiteral("poems"))) {
      impl_->exec(QStringLiteral(
          "CREATE TABLE poems ("
          "id INTEGER PRIMARY KEY AUTOINCREMENT,"
          "sentence TEXT NOT NULL,"
          "source TEXT NOT NULL,"
          "commentary TEXT,"
          "tag TEXT,"
          "dynasty TEXT"
          ")"));
      impl_->insertBatch(builtin);
    }
    QVector<PoemData> poems = impl_->loadAll();
    if (!poems.isEmpty()) {
      return poems;
    }
  }

  // SQLite failed — pure memory mode
  delete impl_;
  impl_ = nullptr;
  return builtin;
}

QVector<PoemData> WisdomDatabase::loadBuiltinFromJson() {
  QVector<PoemData> result;

  QFile f(QStringLiteral(":/resources/data/poems.json"));
  if (!f.open(QIODevice::ReadOnly)) {
    qWarning("WisdomDatabase: cannot open poems.json resource");
    return result;
  }

  QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  f.close();

  if (!doc.isArray()) {
    qWarning("WisdomDatabase: poems.json is not a JSON array");
    return result;
  }

  const QJsonArray arr = doc.array();
  result.reserve(arr.size());

  for (const QJsonValue& v : arr) {
    QJsonObject obj = v.toObject();
    result.append({obj.value(QStringLiteral("sentence")).toString(),
                   obj.value(QStringLiteral("source")).toString(),
                   obj.value(QStringLiteral("commentary")).toString(),
                   obj.value(QStringLiteral("tag")).toString(),
                   obj.value(QStringLiteral("dynasty")).toString()});
  }

  return result;
}
