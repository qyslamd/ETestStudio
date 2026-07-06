#pragma once

#include <QLabel>
#include <QToolButton>
#include <QWidget>

class QDockWidget;

namespace etest::ui {

/// Custom title bar for QDockWidget, replaces the native title bar
/// so button and icon sizes are fully controllable via layout.
/// Used by TopologyEditorWidget and ProtocolEditorWidget.
class DockTitleBar : public QWidget {
  Q_OBJECT
 public:
  DockTitleBar(const QString& title,
               QDockWidget* dockWidget,
               QWidget* parent = nullptr);

  void setTitle(const QString& title);

 private:
  void updateIcons();

  QDockWidget* dock_widget_;
  QLabel* title_label_;
  QToolButton* float_btn_ = nullptr;
  QToolButton* close_btn_ = nullptr;
};

}  // namespace etest::ui
