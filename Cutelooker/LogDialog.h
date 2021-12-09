#pragma once

#include <QDialog>
#include <vector>

namespace Ui {
class LogDialog;
}

class LogDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LogDialog(QWidget* parent = nullptr);
    ~LogDialog();
    bool loadLogJson(const QString& jsonFile);
    void clear();

signals:
    void logSelectionChanged(uint64_t time);

public slots:
    void selectTime(uint64_t time);

private slots:
    void textCursorChangedSlot();

private:
    Ui::LogDialog *ui;
    std::vector<uint64_t> m_lineTimes;
    QString m_windowTitle;
};
