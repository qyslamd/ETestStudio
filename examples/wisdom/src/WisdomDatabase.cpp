#include "WisdomDatabase.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <algorithm>

WisdomDatabase& WisdomDatabase::instance() {
    static WisdomDatabase inst;
    return inst;
}

WisdomDatabase::WisdomDatabase() {
    loadFromResource();
}

WisdomDatabase::~WisdomDatabase() {
    if (db_.isOpen()) db_.close();
}

void WisdomDatabase::loadFromResource() {
    QString writablePath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(writablePath);
    QString dbPath = writablePath + "/wisdom_poems.db";

    if (!QFile::exists(dbPath)) {
        if (!QFile::copy(":/poems.db", dbPath)) {
            return;
        }
        QFile::setPermissions(dbPath,
                              QFile::ReadOwner | QFile::WriteOwner);
    }

    db_ = QSqlDatabase::addDatabase("QSQLITE", "wisdom_demo");
    db_.setDatabaseName(dbPath);
    if (!db_.open()) return;

    QSqlQuery q(db_);
    q.exec("SELECT id FROM poems");
    while (q.next()) {
        ids_.append(q.value(0).toInt());
    }

    std::shuffle(ids_.begin(), ids_.end(),
                 *QRandomGenerator::global());
}

PoemRecord WisdomDatabase::queryById(int id) {
    QSqlQuery q(db_);
    q.prepare("SELECT id, sentence, source, commentary, tag, dynasty "
              "FROM poems WHERE id = ?");
    q.addBindValue(id);
    q.exec();
    if (q.next()) {
        return {q.value(0).toInt(),   q.value(1).toString(),
                q.value(2).toString(), q.value(3).toString(),
                q.value(4).toString(), q.value(5).toString()};
    }
    return {};
}

PoemRecord WisdomDatabase::currentPoem() const {
    if (ids_.isEmpty()) return {};
    return const_cast<WisdomDatabase*>(this)->queryById(
        ids_[cursor_ % ids_.size()]);
}

PoemRecord WisdomDatabase::nextPoem() {
    if (ids_.isEmpty()) return {};
    cursor_++;
    if (cursor_ >= ids_.size()) {
        cursor_ = 0;
        std::shuffle(ids_.begin(), ids_.end(),
                     *QRandomGenerator::global());
    }
    return queryById(ids_[cursor_]);
}
