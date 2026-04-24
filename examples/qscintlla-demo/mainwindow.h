#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <Qsci/qsciscintilla.h>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void onLanguageChanged(int index);
    void onSearch();
    void onReplace();
    void onMarginClicked(int margin, int line, Qt::KeyboardModifiers);

private:
    void setupUi();
    void setupEditor();
    void setupFeatures();
    void setupConnections();
    void setLanguage(const QString &lang);

    QsciScintilla *editor;
    QComboBox *langCombo;
    QLineEdit *searchEdit;
    QLineEdit *replaceEdit;
    QPushButton *searchBtn;
    QPushButton *replaceBtn;
    int breakpointMarker;
};

#endif // MAINWINDOW_H
