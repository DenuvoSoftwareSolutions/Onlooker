#pragma once

#include <QMainWindow>
#include "qcustomplot.h"
#include "OverlayFactoryFilter.h"
#include "OnlookerData.h"
#include "InformationDialog.h"
#include "LogDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent* event);
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);

private:
    void loadJsonChart(const QString& jsonFile);
    void loadJsonLog(const QString& jsonFile);
    bool getPlotPagefileSetting() const;

private slots:
    void overlayCursorChangedSlot(QPoint pos);
    void logSelectionChangedSlot(uint64_t time);

    void on_actionLoad_JSON_triggered();
    void on_actionLoad_Log_JSON_triggered();
    void on_actionInformation_triggered();
    void on_action_Log_triggered();
    void on_actionPlot_pagefile_toggled(bool arg1);

private:
    Ui::MainWindow* ui = nullptr;
    OverlayFactoryFilter* m_overlay = nullptr;
    QCustomPlot* m_customPlot = nullptr;
    InformationDialog* m_informationDialog = nullptr;
    LogDialog* m_logDialog = nullptr;
    bool m_allowLogSelectionEvent = true;
    bool m_hasOpenedInformation = false;
    QString m_windowTitle;

    std::vector<SortedProcess> m_sortedProcesses;
    std::map<uint64_t, std::map<UniqueProcess, ProcessData>> m_timeline;
    std::vector<uint64_t> m_times;
};
