#include "LogDialog.h"
#include "ui_LogDialog.h"
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QTextBlock>
#include <QDebug>
#include <QFileInfo>

#include <algorithm>

LogDialog::LogDialog(QWidget* parent) :
    QDialog(parent),
    ui(new Ui::LogDialog)
{
    ui->setupUi(this);
    m_windowTitle = windowTitle();
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setAttribute(Qt::WA_ShowWithoutActivating);

    connect(ui->logTextEdit, SIGNAL(cursorPositionChanged()), this, SLOT(textCursorChangedSlot()));
}

LogDialog::~LogDialog()
{
    delete ui;
}

bool LogDialog::loadLogJson(const QString& jsonFile)
{
    QFile f(jsonFile);
    if(!f.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open JSON."));
        return false;
    }
    QJsonParseError parseError;
    auto json = QJsonDocument::fromJson(f.readAll(), &parseError);
    if(json.isNull())
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to parse JSON:\n%s").arg(parseError.errorString()));
        return false;
    }
    if(!json.isObject())
    {
        QMessageBox::warning(this, tr("Error"), tr("Unexpected data format"));
        return false;
    }
    // TODO: detect time drift
    std::map<uint64_t, QStringList> log;
    QJsonObject obj = json.object();
    for(const QString& key : obj.keys())
    {
        QJsonValue value = obj.value(key);
        QJsonArray array = value.toArray();
        QStringList l;
        l.reserve(array.size());
        for(int i = 0; i < array.size(); i++)
            l.append(array[i].toString());
        uint64_t time = key.toLongLong();
        log[time] = l;
    }
    m_lineTimes.clear();
    QString logLines;
    for(const auto& event : log)
    {
        for(const QString& line : event.second)
        {
            m_lineTimes.push_back(event.first);
            if(!logLines.isEmpty())
                logLines.append('\n');
            logLines.append(line);
        }
    }
    ui->logTextEdit->setPlainText(logLines);
    setWindowTitle(tr("%1 - %2").arg(m_windowTitle).arg(QFileInfo(jsonFile).fileName()));
    return true;
}

void LogDialog::clear()
{
    m_lineTimes.clear();
    ui->logTextEdit->clear();
}

void LogDialog::selectTime(uint64_t time)
{
    auto itr = std::lower_bound(m_lineTimes.begin(), m_lineTimes.end(), time);
    if(itr == m_lineTimes.end())
    {
        ui->logTextEdit->scrollToLine(0);
        return;
    }
    auto oldItr = itr;
    itr = std::lower_bound(m_lineTimes.begin(), m_lineTimes.end(), *(itr - 1));
    if(itr == m_lineTimes.end())
        itr = oldItr;
    size_t lineIdx = itr - m_lineTimes.begin();
    ui->logTextEdit->scrollToLine(int(lineIdx));
}

void LogDialog::textCursorChangedSlot()
{
    QTextCursor cursor = ui->logTextEdit->textCursor();

    // don't update highlighting when selecting
    if(cursor.selectionStart() != cursor.selectionEnd())
        return;

    size_t lineIdx = cursor.blockNumber();
    if(lineIdx >= m_lineTimes.size())
        return;
    uint64_t lineTime = m_lineTimes[lineIdx];

    size_t timeStartIdx = lineIdx;
    while(true)
    {
        if(timeStartIdx == 0)
            break;
        auto prevIdx = timeStartIdx - 1;
        if(m_lineTimes[prevIdx] != lineTime)
            break;
        timeStartIdx = prevIdx;
    }

    size_t timeEndIdx = lineIdx;
    while(true)
    {
        if(timeEndIdx + 1 >= m_lineTimes.size())
            break;
        auto nextIdx = timeEndIdx + 1;
        if(m_lineTimes[nextIdx] != lineTime)
            break;
        timeEndIdx = nextIdx;
    }
    if(timeEndIdx <= m_lineTimes.size())
        timeEndIdx++;

    auto lineToCursor = [this](int line)
    {
        QTextCursor cursor = ui->logTextEdit->textCursor();
        QTextBlock block = ui->logTextEdit->document()->findBlockByLineNumber(line);
        cursor.setPosition(block.position());
        return cursor;
    };

    QColor cursorColor = QColor(80, 80, 255, 128);
    QColor neighborColor = cursorColor.light();

    QList<QTextEdit::ExtraSelection> extraSelections;
    for(size_t i = timeStartIdx; i < timeEndIdx; i++)
    {
        QColor lineColor = i == lineIdx ? cursorColor : neighborColor;
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(lineColor);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = lineToCursor(int(i));
        selection.cursor.clearSelection();
        extraSelections.append(selection);
    }

    ui->logTextEdit->setExtraSelections(extraSelections);

    emit logSelectionChanged(lineTime);
}
