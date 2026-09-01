#include "AboutDialog.h"

#include <QLabel>
#include <QLayout>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>


#include "version.h"

namespace etest::app {

AboutDialog::AboutDialog(QWidget* parent) : OverlayDialog(parent) {
  initUi();
}

void AboutDialog::initUi() {
  auto* content = new QWidget;
  content->setObjectName(QStringLiteral("aboutContent"));
  content->setFixedSize(440, 540);

  auto* main_layout = new QVBoxLayout(content);
  main_layout->setContentsMargins(0, 0, 0, 0);
  main_layout->setSpacing(0);

  // ── Header ──
  auto* header = new QFrame(content);
  header->setObjectName(QStringLiteral("aboutHeader"));
  header->setFixedHeight(160);

  auto* header_layout = new QVBoxLayout(header);
  header_layout->setAlignment(Qt::AlignCenter);
  header_layout->setSpacing(8);

  logo_label_ = new QLabel(header);
  logo_label_->setObjectName(QStringLiteral("aboutLogo"));
  QPixmap logo_pix(QStringLiteral(":/resources/icons/app_icon.svg"));
  logo_label_->setPixmap(
      logo_pix.scaled(72, 72, Qt::KeepAspectRatio, Qt::SmoothTransformation));
  logo_label_->setAlignment(Qt::AlignCenter);
  logo_label_->setFixedSize(64, 64);

  name_label_ = new QLabel(QStringLiteral("ETestStudio"), header);
  name_label_->setObjectName(QStringLiteral("aboutName"));
  name_label_->setAlignment(Qt::AlignCenter);

  version_label_ = new QLabel(header);
  version_label_->setObjectName(QStringLiteral("aboutVersion"));
  version_label_->setText(QStringLiteral("Version %1").arg(PROJECT_VERSION));
  version_label_->setAlignment(Qt::AlignCenter);

  header_layout->addStretch();
  header_layout->addWidget(logo_label_);
  header_layout->addWidget(name_label_);
  header_layout->addWidget(version_label_);
  header_layout->addStretch();

  main_layout->addWidget(header);

  // ── Body ──
  auto* body = new QWidget(content);
  body->setObjectName(QStringLiteral("aboutBody"));

  auto* body_layout = new QVBoxLayout(body);
  body_layout->setContentsMargins(32, 20, 32, 20);
  body_layout->setSpacing(12);

  // Description
  desc_label_ =
      new QLabel(QStringLiteral("通过硬件抽象层、接口控制文档及脚本执行引擎，\n"
                                "实现测试逻辑与物理硬件的高度解耦。"),
                 body);
  desc_label_->setObjectName(QStringLiteral("aboutDesc"));
  desc_label_->setWordWrap(true);
  desc_label_->setAlignment(Qt::AlignCenter);
  body_layout->addWidget(desc_label_);

  // Tech section header
  auto* tech_header = new QLabel(QStringLiteral("技术栈"), body);
  tech_header->setObjectName(QStringLiteral("aboutTechHeader"));
  body_layout->addWidget(tech_header);

  // Tech chips — wrapped in a flow-like grid (QVBoxLayout of QHBoxLayout rows)
  tech_chips_container_ = new QWidget(body);
  tech_chips_container_->setObjectName(QStringLiteral("aboutTechChips"));
  auto* chips_grid = new QVBoxLayout(tech_chips_container_);
  chips_grid->setContentsMargins(0, 0, 0, 0);
  chips_grid->setSpacing(8);

  const char* tech_list[] = {
      "C++17",      "Lua 5.4.4", "spdlog",     "QScintilla", "QXlsx", "libharu",
      "Googletest", "SARibbon",  "QWindowKit", "libpng",     "zlib",
  };
  constexpr int kChipsPerRow = 3;

  // First row: Qt (runtime version via qVersion()) + C++17 + Lua
  {
    auto* row = new QHBoxLayout;
    row->setSpacing(8);

    auto* qt_chip = new QLabel(QString("Qt %1").arg(QLatin1String(qVersion())));
    qt_chip->setObjectName(QStringLiteral("aboutTechChip"));
    qt_chip->setAlignment(Qt::AlignCenter);
    row->addWidget(qt_chip);

    for (int j = 0; j < 2; ++j) {
      auto* chip = new QLabel(QString::fromLatin1(tech_list[j]));
      chip->setObjectName(QStringLiteral("aboutTechChip"));
      chip->setAlignment(Qt::AlignCenter);
      row->addWidget(chip);
    }
    row->addStretch();
    chips_grid->addLayout(row);
  }

  // Remaining rows (9 items × 3 per row)
  int chip_count = sizeof(tech_list) / sizeof(tech_list[0]);
  for (int i = 2; i < chip_count; i += kChipsPerRow) {
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    for (int j = 0; j < kChipsPerRow && i + j < chip_count; ++j) {
      auto* chip = new QLabel(QString::fromLatin1(tech_list[i + j]));
      chip->setObjectName(QStringLiteral("aboutTechChip"));
      chip->setAlignment(Qt::AlignCenter);
      row->addWidget(chip);
    }
    row->addStretch();
    chips_grid->addLayout(row);
  }

  body_layout->addWidget(tech_chips_container_);

  // Separator
  auto* sep = new QFrame(body);
  sep->setObjectName(QStringLiteral("aboutSep"));
  sep->setFrameShape(QFrame::HLine);
  sep->setFixedHeight(1);
  body_layout->addWidget(sep);

  // Build info
  build_info_label_ = new QLabel(
      QStringLiteral("MSVC 2019 x64  |  %1").arg(PROJECT_BUILD_DATE), body);
  build_info_label_->setObjectName(QStringLiteral("aboutBuildInfo"));
  build_info_label_->setAlignment(Qt::AlignCenter);
  body_layout->addWidget(build_info_label_);

  // OK button
  ok_button_ = new QToolButton(body);
  ok_button_->setText(QStringLiteral("确  定"));
  ok_button_->setObjectName(QStringLiteral("aboutOkButton"));
  ok_button_->setCursor(Qt::PointingHandCursor);
  ok_button_->setFixedWidth(160);
  ok_button_->setFixedHeight(36);
  connect(ok_button_, &QToolButton::clicked, this, &QDialog::accept);
  auto* btn_row = new QHBoxLayout;
  btn_row->setAlignment(Qt::AlignCenter);
  btn_row->addWidget(ok_button_);
  body_layout->addLayout(btn_row);

  // Copyright + License
  copyright_label_ = new QLabel(QStringLiteral("© 2026 ETest Demo"), body);
  copyright_label_->setObjectName(QStringLiteral("aboutCopyright"));
  copyright_label_->setAlignment(Qt::AlignCenter);
  body_layout->addWidget(copyright_label_);

  license_label_ = new QLabel(QStringLiteral("保留所有权利 · 内部使用"), body);
  license_label_->setObjectName(QStringLiteral("aboutLicense"));
  license_label_->setAlignment(Qt::AlignCenter);
  body_layout->addWidget(license_label_);
  body_layout->addStretch();

  main_layout->addWidget(body);

  setWidget(content);
}

}  // namespace etest::app
