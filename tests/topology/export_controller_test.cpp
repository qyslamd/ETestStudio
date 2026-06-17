#include <gtest/gtest.h>

#include "topology/TopologyExportController.h"

using namespace etest::topology;

TEST(TopologyExportControllerTest, CompleteFilePathKeepsExistingSuffix) {
  EXPECT_EQ(TopologyExportController::completeFilePath(
                QStringLiteral("out.svg"), QStringLiteral("PDF 文档 (*.pdf)")),
            QStringLiteral("out.svg"));
}

TEST(TopologyExportControllerTest, CompleteFilePathUsesSelectedSvgFilter) {
  EXPECT_EQ(TopologyExportController::completeFilePath(
                QStringLiteral("out"), QStringLiteral("SVG 矢量图 (*.svg)")),
            QStringLiteral("out.svg"));
}

TEST(TopologyExportControllerTest, CompleteFilePathUsesSelectedPdfFilter) {
  EXPECT_EQ(TopologyExportController::completeFilePath(
                QStringLiteral("out"), QStringLiteral("PDF 文档 (*.pdf)")),
            QStringLiteral("out.pdf"));
}

TEST(TopologyExportControllerTest, CompleteFilePathDefaultsToPng) {
  EXPECT_EQ(TopologyExportController::completeFilePath(
                QStringLiteral("out"), QStringLiteral("PNG 图片 (*.png)")),
            QStringLiteral("out.png"));
}
