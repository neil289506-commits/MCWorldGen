#include "ui/ConsoleWidget.h"
#include <QScrollBar>
#include <QFont>

ConsoleWidget::ConsoleWidget(QWidget *parent) : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setMaximumBlockCount(5000); // 避免長時間執行導致記憶體無限增長
    QFont mono("Consolas");
    mono.setStyleHint(QFont::Monospace);
    setFont(mono);
}

void ConsoleWidget::appendLine(const QString &line)
{
    appendPlainText(line);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}
