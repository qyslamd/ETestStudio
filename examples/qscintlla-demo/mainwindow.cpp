#include "mainwindow.h"
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qsciapis.h>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QMessageBox>
#include <QFont>
#include <QColor>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
    setupEditor();
    setupConnections();
    setLanguage("cpp");
}

void MainWindow::onLanguageChanged(int index) {
    QString lang = langCombo->itemData(index).toString();
    setLanguage(lang);
}

void MainWindow::onSearch() {
    QString text = searchEdit->text();
    if (text.isEmpty()) return;
    bool found = editor->findFirst(text, false, false, false, true);
    if (!found) {
        QMessageBox::information(this, "提示", "未找到匹配内容");
    }
}

void MainWindow::onReplace() {
    QString searchText = searchEdit->text();
    QString replaceText = replaceEdit->text();
    if (searchText.isEmpty()) return;
    editor->replace(replaceText);
}

void MainWindow::onMarginClicked(int margin, int line, Qt::KeyboardModifiers) {
    if (margin == 1) {
        int markers = editor->markersAtLine(line);
        if (markers & (1 << breakpointMarker)) {
            editor->markerDelete(line, breakpointMarker);
        } else {
            editor->markerAdd(line, breakpointMarker);
        }
    }
}

void MainWindow::setupUi() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    
    langCombo = new QComboBox();
    langCombo->addItem("C++", "cpp");
    langCombo->addItem("Python", "python");
    langCombo->addItem("JavaScript", "js");
    langCombo->addItem("HTML", "html");
    toolbarLayout->addWidget(new QLabel("语言:"));
    toolbarLayout->addWidget(langCombo);

    toolbarLayout->addWidget(new QLabel("搜索:"));
    searchEdit = new QLineEdit();
    toolbarLayout->addWidget(searchEdit);
    searchBtn = new QPushButton("查找");
    toolbarLayout->addWidget(searchBtn);

    toolbarLayout->addWidget(new QLabel("替换为:"));
    replaceEdit = new QLineEdit();
    toolbarLayout->addWidget(replaceEdit);
    replaceBtn = new QPushButton("替换");
    toolbarLayout->addWidget(replaceBtn);

    mainLayout->addLayout(toolbarLayout);

    editor = new QsciScintilla();
    mainLayout->addWidget(editor);

    resize(1000, 700);
    setWindowTitle("QScintilla 2.11.3 Demo (Qt 5.12.12)");
}

void MainWindow::setupEditor() {
    QFont font = QFont("Consolas", 11);
    editor->setFont(font);
    editor->setMarginsFont(font);

    editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);
    editor->setMatchedBraceBackgroundColor(QColor(255, 255, 0));
    editor->setUnmatchedBraceBackgroundColor(QColor(255, 0, 0));

    editor->setAutoIndent(true);
    editor->setIndentationWidth(4);
    editor->setIndentationsUseTabs(false);
    editor->setTabWidth(4);
    editor->setBackspaceUnindents(true);

    editor->setMarginType(0, QsciScintilla::NumberMargin);
    editor->setMarginLineNumbers(0, true);
    editor->setMarginWidth(0, 40);

    editor->setMarginType(1, QsciScintilla::SymbolMargin);
    editor->setMarginWidth(1, 20);
    editor->SendScintilla(2242, 1, true);
    breakpointMarker = editor->markerDefine(QsciScintilla::Circle);
    editor->setMarkerBackgroundColor(QColor(255, 0, 0), breakpointMarker);

    editor->setFolding(QsciScintilla::BoxedTreeFoldStyle, 2);
    editor->setFoldMarginColors(QColor(240, 240, 240), QColor(220, 220, 220));

    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(QColor(245, 245, 245));

    editor->setWhitespaceVisibility(QsciScintilla::WsVisibleAfterIndent);
    editor->setWhitespaceSize(2);

    editor->setEolMode(QsciScintilla::EolUnix);
    editor->setEolVisibility(false);
}

void MainWindow::setupFeatures() {
    QsciAPIs *apis = new QsciAPIs(editor->lexer());
    apis->add("int");
    apis->add("float");
    apis->add("double");
    apis->add("char");
    apis->add("void");
    apis->add("if");
    apis->add("else");
    apis->add("for");
    apis->add("while");
    apis->add("class");
    apis->add("struct");
    apis->add("return");
    apis->add("def");
    apis->add("import");
    apis->add("function");
    apis->add("const");
    apis->add("let");
    apis->add("var");
    apis->prepare();

    editor->setAutoCompletionSource(QsciScintilla::AcsAll);
    editor->setAutoCompletionCaseSensitivity(Qt::CaseInsensitive);
    editor->setAutoCompletionThreshold(2);
    editor->setAutoCompletionReplaceWord(true);
}

void MainWindow::setupConnections() {
    connect(langCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(onLanguageChanged(int)));
    connect(searchEdit, SIGNAL(returnPressed()), this, SLOT(onSearch()));
    connect(searchBtn, SIGNAL(clicked()), this, SLOT(onSearch()));
    connect(replaceBtn, SIGNAL(clicked()), this, SLOT(onReplace()));
    connect(editor, SIGNAL(marginClicked(int, int, Qt::KeyboardModifiers)), this, SLOT(onMarginClicked(int, int, Qt::KeyboardModifiers)));
}

void MainWindow::setLanguage(const QString &lang) {
    QsciLexer *lexer = nullptr;
    QString sampleCode;

    if (lang == "cpp") {
        lexer = new QsciLexerCPP(editor);
        sampleCode = R"RAW(#include <iostream>
#include <vector>

class Demo {
public:
    Demo() : value(42) {}
    
    int calculate(int a, int b) {
        if (a > b) {
            return a + b;
        } else {
            return a * b;
        }
    }

private:
    int value;
};

int main() {
    Demo demo;
    std::cout << "Result: " << demo.calculate(10, 20) << std::endl;
    return 0;
})RAW";
    } else if (lang == "python") {
        lexer = new QsciLexerPython(editor);
        sampleCode = R"RAW(def fibonacci(n):
    a, b = 0, 1
    for _ in range(n):
        yield a
        a, b = b, a + b

class PythonDemo:
    def __init__(self, name):
        self.name = name
    
    def greet(self):
        return f"Hello, {self.name}!"

if __name__ == "__main__":
    demo = PythonDemo("QScintilla")
    print(demo.greet())
    for num in fibonacci(10):
        print(num, end=" "))RAW";
    } else if (lang == "js") {
        lexer = new QsciLexerJavaScript(editor);
        sampleCode = R"RAW(class JsDemo {
    constructor(name) {
        this.name = name;
    }

    calculate(a, b) {
        if (a > b) {
            return a + b;
        } else {
            return a * b;
        }
    }

    greet() {
        return `Hello, ${this.name}!`;
    }
}

const demo = new JsDemo("JavaScript");
console.log(demo.greet());
console.log("Result:", demo.calculate(10, 20));

const numbers = [1, 2, 3, 4, 5];
numbers.forEach(num => console.log(num * 2));)RAW";
    } else if (lang == "html") {
        lexer = new QsciLexerHTML(editor);
        sampleCode = R"RAW(<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <title>QScintilla Demo</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 20px;
        }
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Hello QScintilla</h1>
        <p>This is a HTML syntax highlighting demo.</p>
        <button onclick="alert('Clicked!')">Click Me</button>
    </div>
</body>
</html>)RAW";
    }

    if (lexer) {
        editor->setLexer(lexer);
        editor->setText(sampleCode);
        setupFeatures();
    }
}
