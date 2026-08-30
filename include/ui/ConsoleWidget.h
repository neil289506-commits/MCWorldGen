#pragma once
#include <QPlainTextEdit>

// 即時 console 顯示元件：接收字串行並附加到畫面，自動捲動到最新一行
class ConsoleWidget : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit ConsoleWidget(QWidget *parent = nullptr);

public slots:
    void appendLine(const QString &line);
};
