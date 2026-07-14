#ifndef ETEST_APP_EDITORPANELCONTROLLER_H_
#define ETEST_APP_EDITORPANELCONTROLLER_H_

#include <QMetaObject>
#include <QObject>

namespace etest::app {

class AppStatusBarController;
class EditorManager;

class EditorPanelController : public QObject {
  Q_OBJECT
 public:
  explicit EditorPanelController(EditorManager* editor_mgr,
                                 AppStatusBarController* status_bar_ctrl,
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

  // 连接当前编辑器的状态变化信号
  void connectCurrentEditor();

 signals:
  void modificationChanged(bool modified);
  void cursorPositionChanged(int line, int col);

 private:
  void disconnectCurrentEditor();
  void updateEditorStatus();

  EditorManager* editor_mgr_;
  AppStatusBarController* status_bar_ctrl_;

  QMetaObject::Connection sel_connection_;
  QMetaObject::Connection state_connection_;
};

}  // namespace etest::app

#endif  // ETEST_APP_EDITORPANELCONTROLLER_H_
