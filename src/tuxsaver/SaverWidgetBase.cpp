#include "SaverWidgetBase.h"

SaverWidgetBase::SaverWidgetBase(QWidget* parent)
    : QWidget(parent) {
}

void SaverWidgetBase::setIdleThreshold(int sec) {
  Q_UNUSED(sec);
}
