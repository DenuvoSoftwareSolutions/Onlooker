#pragma once

#include <QPlainTextEdit>

class LogViewTextEdit : public QPlainTextEdit
{
public:
    explicit LogViewTextEdit(QWidget* parent = nullptr);
    void scrollToLine(int lineIdx);

protected:
    void keyPressEvent(QKeyEvent* event) override;
};
