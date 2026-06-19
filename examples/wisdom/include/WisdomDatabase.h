#pragma once

#include <QString>
#include <QVector>
#include <QSqlDatabase>

struct PoemRecord {
    int id;
    QString sentence;
    QString source;
    QString commentary;
    QString tag;
    QString dynasty;
};

class WisdomDatabase {
 public:
    static WisdomDatabase& instance();

    PoemRecord currentPoem() const;
    PoemRecord nextPoem();
    int totalCount() const { return ids_.size(); }

 private:
    WisdomDatabase();
    ~WisdomDatabase();
    WisdomDatabase(const WisdomDatabase&) = delete;
    WisdomDatabase& operator=(const WisdomDatabase&) = delete;

    void loadFromResource();
    PoemRecord queryById(int id);

    QSqlDatabase db_;
    QVector<int> ids_;
    int cursor_ = 0;
};
