#pragma once

#include <QDockWidget>
#include <QFrame>
#include <QLabel>
#include <QToolButton>

namespace etest::ui {

/// Custom title bar for QDockWidget, replaces the native title bar
/// so button and icon sizes are fully controllable via layout.
/// Used by TopologyEditorWidget and ProtocolEditorWidget.
class DockTitleBar final : public QFrame {
  Q_OBJECT
 private:
  DockTitleBar(const QString& title,
               QDockWidget* dockWidget,
               QWidget* parent = nullptr);

  void setTitle(const QString& title);

  friend class EtDockWidget;
  void initUi(const QString& title);
  void initSignals();
  void updateIcons();

  void onDockWidgetFeaturesChanged(QDockWidget::DockWidgetFeatures features);

 private:
  QDockWidget* dock_widget_;
  QLabel* title_label_;
  QToolButton* float_btn_ = nullptr;
  QToolButton* close_btn_ = nullptr;
};

/// QDockWidget 子类：让 QSS 的 border/border-radius 对停靠面板生效，
/// 并内聚复用 DockTitleBar 作为标题栏（浮动/关闭按钮逻辑在内）。
class EtDockWidget final : public QDockWidget {
  Q_OBJECT
 public:
  explicit EtDockWidget(const QString& title, QWidget* parent = nullptr);
  ~EtDockWidget() override;

  void setTitle(const QString& title);

 private:
  DockTitleBar* title_bar_ = nullptr;
};

}  // namespace etest::ui
