#include "LogViewTextEdit.h"
#include <QKeyEvent>
#include <QTextBlock>
#include <QDebug>

LogViewTextEdit::LogViewTextEdit(QWidget* parent) :
    QPlainTextEdit(parent)
{
    setWordWrapMode(QTextOption::NoWrap);
    setUndoRedoEnabled(false);
    setContextMenuPolicy(Qt::NoContextMenu);
}

void LogViewTextEdit::scrollToLine(int lineIdx)
{
    // https://forum.qt.io/post/364710
    int visibleLines = 0;
    {
        int startPos = cursorForPosition(QPoint(0, 0)).position();
        QPoint bottomRight(viewport()->width() - 1, viewport()->height() - 1);
        int endPos = cursorForPosition(bottomRight).position();
        auto cursor = textCursor();
        cursor.setPosition(startPos);
        auto startBlock = cursor.blockNumber();
        cursor.setPosition(endPos);
        auto endBlock = cursor.blockNumber();
        visibleLines = endBlock - startBlock;
    }
    // scrollbar to the end
    moveCursor(QTextCursor::End);
    // scrollbar so that lineIdx is in the middle
    setTextCursor(QTextCursor(document()->findBlockByNumber(qMax(0, lineIdx - visibleLines / 2))));
    // changes cursor but not the scrollbar
    setTextCursor(QTextCursor(document()->findBlockByNumber(lineIdx)));
}

void LogViewTextEdit::keyPressEvent(QKeyEvent* event)
{
    auto text = event->text();
    auto isCopy = event->modifiers() == Qt::ControlModifier && event->key() == Qt::Key_C;
    if(text.isEmpty() || isCopy) // interaction keys
        QPlainTextEdit::keyPressEvent(event);
}
