#ifndef ETEST_APP_EDITORPANELCONTROLLER_H_
#define ETEST_APP_EDITORPANELCONTROLLER_H_

#include <QObject>

namespace etest::app {

class EditorManager;

class EditorPanelController : public QObject {
  Q_OBJECT
 public:
  explicit EditorPanelController(EditorManager* editor_mgr,
                                 QObject* parent = nullptr);

  // 文件操作
  void saveCurrent();
  void saveCurrentAs();
  void saveAll();
  void closeCurrent();
  void closeAll();

  // 编辑操作
  void undo();
  void redo();
  void cut();
  void copy();
  void paste();

  // 搜索/跳转
  void find();
  void replace();
  void goToLine();

 private:
  EditorManager* editor_mgr_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITORPANELCONTROLLER_H_
