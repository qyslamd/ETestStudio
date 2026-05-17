#pragma once

#include <QString>
#include <QWidget>

namespace etest::app {

class IEditor {
 public:
  virtual ~IEditor() = default;

  virtual QString displayName() const = 0;
  virtual bool isModified() const = 0;
  virtual bool save() = 0;
  virtual bool saveAs(const QString& path) = 0;
  virtual QString filePath() const = 0;
  virtual QString editorId() const = 0;
  virtual QWidget* widget() = 0;
  virtual QString editorType() const = 0;
  virtual QObject* signalObject() = 0;
};

}  // namespace etest::app
