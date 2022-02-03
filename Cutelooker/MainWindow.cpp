#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QGraphicsWidget>
#include <QFile>
#include <QJsonDocument>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonValue>
#include <QSettings>
#include <QFileInfo>

#include <cmath>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include "resource.h"
#endif //Q_OS_WIN

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_windowTitle = windowTitle();
    m_overlay = new OverlayFactoryFilter(this);
    connect(m_overlay, SIGNAL(cursorChanged(QPoint)), this, SLOT(overlayCursorChangedSlot(QPoint)));
    m_informationDialog = new InformationDialog(this);
    m_logDialog = new LogDialog(this);
    connect(m_logDialog, SIGNAL(logSelectionChanged(uint64_t)), this, SLOT(logSelectionChangedSlot(uint64_t)));

    QSettings settings;
    restoreGeometry(settings.value("MainWindowGeometry").toByteArray());
    restoreState(settings.value("MainWindowState").toByteArray());
    m_informationDialog->restoreGeometry(settings.value("InformationDialog").toByteArray());
    m_logDialog->restoreGeometry(settings.value("LogDialog").toByteArray());
    ui->actionPlot_pagefile->setChecked(getPlotPagefileSetting());

    // Windows hack for setting the icon in the taskbar.
#ifdef Q_OS_WIN
    HICON hIcon = LoadIconW(GetModuleHandleW(0), MAKEINTRESOURCEW(IDI_ICON1));
    SendMessageW((HWND)winId(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    DestroyIcon(hIcon);
#endif //Q_OS_WIN
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    settings.setValue("MainWindowState", saveState());
    m_informationDialog->hide();
    settings.setValue("InformationDialog", m_informationDialog->saveGeometry());
    m_logDialog->hide();
    settings.setValue("LogDialog", m_logDialog->saveGeometry());
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if(event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if(event->mimeData()->hasUrls())
    {
        QString jsonFile = QDir::toNativeSeparators(event->mimeData()->urls()[0].toLocalFile());
        QFile f(jsonFile);
        if(!f.open(QFile::ReadOnly))
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open JSON."));
            return;
        }
        QJsonParseError parseError;
        auto json = QJsonDocument::fromJson(f.readAll(), &parseError);
        if(json.isNull())
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to parse JSON:\n%s").arg(parseError.errorString()));
            return;
        }
        QSettings settings;
        settings.setValue("BrowseDirectory", QFileInfo(jsonFile).absoluteDir().absolutePath());
        if(json.isArray()) // Data
            loadJsonChart(jsonFile);
        else if(json.isObject()) // Log
            loadJsonLog(jsonFile);
        event->acceptProposedAction();
    }
}

class MemoryAxisTicker : public QCPAxisTickerFixed
{
protected:
    QString getTickLabel(double tick, const QLocale& locale, QChar formatChar, int precision) override
    {
        Q_UNUSED(locale);
        Q_UNUSED(formatChar);
        Q_UNUSED(precision);

        if(tick < 0)
            return QString();

        return humanReadableSize(tick);
    }
};

class TimeAxisTicker : public QCPAxisTicker
{
public:
    TimeAxisTicker(const std::vector<uint64_t>& times) : m_times(times) { }

protected:
    QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) override
    {
        //return QCPAxisTicker::getTickLabel(tick, locale, formatChar, precision);
        Q_UNUSED(locale);
        Q_UNUSED(formatChar);
        Q_UNUSED(precision);

        if(tick < 0 || tick >= m_times.size())
            return QString();

        auto ms = (m_times[tick] - m_times[0]);
        QTime t = QTime(0, 0).addMSecs(ms);
        return t.toString("hh:mm:ss");
    }

private:
    std::vector<uint64_t> m_times;
};

template <typename Cont, typename Pred>
Cont filter(const Cont &container, Pred predicate)
{
    Cont result;
    std::copy_if(container.begin(), container.end(), std::back_inserter(result), predicate);
    return result;
}

void MainWindow::loadJsonChart(const QString& jsonFile)
{
    auto plotPagefile = getPlotPagefileSetting();
    // deserialize json
    std::map<UniqueProcess, std::vector<ProcessData>> processData;
    {
        QFile f(jsonFile);
        if(!f.open(QFile::ReadOnly))
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to open JSON."));
            return;
        }
        QJsonParseError parseError;
        auto json = QJsonDocument::fromJson(f.readAll(), &parseError);
        if(json.isNull())
        {
            QMessageBox::warning(this, tr("Error"), tr("Failed to parse JSON:\n%s").arg(parseError.errorString()));
            return;
        }
        if(!json.isArray())
        {
            QMessageBox::warning(this, tr("Error"), tr("Unexpected data format"));
            return;
        }
        auto processes = json.array();
        int processCount = processes.size();

        for(int i = 0; i < processCount; i++)
        {
            QJsonValue process = processes[i];
            UniqueProcess uniqueProcess;
            uniqueProcess.pid = process["pid"].toVariant().toLongLong();
            uniqueProcess.ppid = process["ppid"].toVariant().toLongLong();
            uniqueProcess.name = process["name"].toString();
            auto& pdata = processData[uniqueProcess];
            auto datas = process["data"].toArray();
            auto dataCount = datas.size();
            pdata.resize(dataCount);
            for(int j = 0; j < dataCount; j++)
            {
                QJsonValue data = datas[j];
                pdata[j].time = data["time"].toVariant().toLongLong();
                pdata[j].memoryUsage = data["memory"]["workingSetSize"].toVariant().toLongLong();
                pdata[j].pagefileUsage = data["memory"]["pagefileUsage"].toVariant().toLongLong();
                pdata[j].cpuUsage = data["cpuUsage"].toDouble();
            }
        }
    }

    // get sorted processes
    std::vector<SortedProcess> sortedProcesses;
    for (const auto& process : processData)
    {
        SortedProcess s;
        s.uniqueProcess = process.first;
        for (const ProcessData& data : process.second)
        {
            s.startTime = qMin(data.time, s.startTime);
            s.endTime = qMax(data.time, s.endTime);
            s.maxMemoryUsage = qMax(plotPagefile ? data.pagefileUsage : data.memoryUsage, s.maxMemoryUsage);
        }
        sortedProcesses.push_back(s);
    }
    std::sort(sortedProcesses.begin(), sortedProcesses.end());
    m_sortedProcesses = sortedProcesses;

    // populate timeline
    uint64_t startTime = -1;
    uint64_t endTime = 0;
    std::map<uint64_t, std::map<UniqueProcess, ProcessData>> timeline;
    for (const SortedProcess& process : sortedProcesses)
    {
        const UniqueProcess& uniqueProcess = process.uniqueProcess;
        for (const ProcessData& data : processData.at(uniqueProcess))
        {
            startTime = qMin(startTime, data.time);
            endTime = qMax(endTime, data.time);
            timeline[data.time].emplace(uniqueProcess, data);
        }
    }

    // zero where timeline is not present
    for (auto& event : timeline)
    {
        for (const auto& process : sortedProcesses)
        {
            auto itr = event.second.find(process.uniqueProcess);
            if (itr == event.second.end())
            {
                ProcessData zeroData;
                zeroData.time = event.first;
                event.second.emplace(process.uniqueProcess, zeroData);
            }
        }
    }
    m_timeline = timeline;

    // generate chart
    {
        // TODO: use matplotlib tab20
        QVector<QColor> colors =
        {
            QColor("#F3B415"),
            QColor("#F27036"),
            QColor("#663F59"),
            QColor("#6A6E94"),
            QColor("#4E88B4"),
            QColor("#00A7C6"),
            QColor("#18D8D8"),
            QColor("#A9D794"),
            QColor("#46AF78"),
            QColor("#A93F55"),
            QColor("#8C5E58"),
            QColor("#2176FF"),
            QColor("#33A1FD"),
            QColor("#7A918D"),
            QColor("#BAFF29"),
        };
        // tab20
        /*
        import matplotlib.pyplot as plt
        cmap = plt.get_cmap('tab20')
        for i in range(0, 20):
            print ('QColor' + str(cmap(i / 20.0, bytes=True)[:3]) + ',')
        */
        colors =
        {
            QColor(31, 119, 180),
            QColor(174, 199, 232),
            QColor(255, 127, 14),
            QColor(255, 187, 120),
            QColor(44, 160, 44),
            QColor(152, 223, 138),
            QColor(214, 39, 40),
            QColor(255, 152, 150),
            QColor(148, 103, 189),
            QColor(197, 176, 213),
            QColor(140, 86, 75),
            QColor(196, 156, 148),
            QColor(227, 119, 194),
            QColor(247, 182, 210),
            QColor(127, 127, 127),
            QColor(199, 199, 199),
            QColor(188, 189, 34),
            QColor(219, 219, 141),
            QColor(23, 190, 207),
            QColor(158, 218, 229),
        };

        if(m_customPlot)
        {
            m_overlay->hideOverlay();
            m_customPlot->removeEventFilter(m_overlay);
            m_informationDialog->hide();
            m_informationDialog->setInformationText(QString());
            delete m_customPlot;
            m_customPlot = nullptr;
        }

        auto customPlot = m_customPlot = new QCustomPlot(this);

        std::map<UniqueProcess, QCPBars*> processBars;
        for(size_t i = 0; i < sortedProcesses.size(); i++)
        {
            const SortedProcess& process = sortedProcesses[i];
            const QColor& color = colors[i % colors.size()];

            auto bars = new QCPBars(customPlot->xAxis, customPlot->yAxis);
            bars->setProperty("PID", process.uniqueProcess.pid);
            bars->setProperty("PPID", process.uniqueProcess.ppid);
            bars->setName(QString("%1 (PID: %2, Parent: %3)").arg(process.uniqueProcess.name).arg(process.uniqueProcess.pid).arg(process.uniqueProcess.ppid));
            bars->setBrush(QBrush(color));
            bars->setPen(QPen(color));
            bars->setWidthType(QCPBars::wtPlotCoords);
            bars->setWidth(1);
            bars->setAntialiased(false);
            bars->setSelectable(QCP::SelectionType::stSingleData);

            void(QCPBars::* mySelectionChanged)(const QCPDataSelection&) = &QCPBars::selectionChanged;
            connect(bars, mySelectionChanged, [this, bars](const QCPDataSelection& ds)
            {
                if (ds.dataRangeCount() == 0)
                {
                    m_selectedGraph = nullptr;
                }
                else
                {
                    m_selectedGraph = bars;
                }
                // TODO: find a better way, this is super delayed
                overlayCursorChangedSlot(m_lastPos);
            });

            processBars[process.uniqueProcess] = bars;
            if(i > 0)
                bars->moveAbove(processBars[sortedProcesses[i - 1].uniqueProcess]);
        }

        uint64_t maxSum = 0;

        QVector<double> ticks;
        std::vector<uint64_t> times;
        uint64_t xx = 0;
        std::map<UniqueProcess, QVector<double>> processBarData;
        for (const auto& event : timeline)
        {
            ticks.push_back(xx++);
            times.push_back(event.first);
            uint64_t eventSum = 0;
            for (const auto& process : sortedProcesses)
            {
                const auto& p = event.second.at(process.uniqueProcess);
                auto memoryUsage = plotPagefile ? p.pagefileUsage : p.memoryUsage;
                eventSum += memoryUsage;
                processBarData[process.uniqueProcess].push_back(memoryUsage);
            }
            maxSum = qMax(maxSum, eventSum);
        }
        m_times = times;

        // prepare x axis
        customPlot->xAxis->setRange(0, xx);
        customPlot->xAxis->setLabel("Time");
        QSharedPointer<TimeAxisTicker> timeTicker(new TimeAxisTicker(times));
        customPlot->xAxis->setTicker(timeTicker);

        // prepare y axis
        customPlot->yAxis->setRange(0, maxSum);
        customPlot->yAxis->setLabel(plotPagefile ? "Pagefile usage" : "Memory usage");
        QSharedPointer<MemoryAxisTicker> memoryTicker(new MemoryAxisTicker);
        memoryTicker->setScaleStrategy(MemoryAxisTicker::ssMultiples);
        memoryTicker->setTickStep(1024ull * 1024 * 10); // 10 mb
        auto tickStep = memoryTicker->getTickStep(QCPRange(0, maxSum));
        customPlot->yAxis->setRange(0, std::ceil(maxSum / tickStep) * tickStep);
        customPlot->yAxis->setTicker(memoryTicker);

        // add data
        for(const auto& process : sortedProcesses)
        {
            processBars[process.uniqueProcess]->setData(ticks, processBarData[process.uniqueProcess], true);
        }

        // setup legend
        customPlot->legend->setVisible(false); // TODO: make menu to toggle the legend
        customPlot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop|Qt::AlignLeft);
        customPlot->legend->setBrush(QColor(255, 255, 255, 100));
        customPlot->legend->setBorderPen(Qt::NoPen);
        QFont legendFont = font();
        legendFont.setPointSize(10);
        customPlot->legend->setFont(legendFont);
        
        // selection
        customPlot->setInteraction(QCP::Interaction::iSelectPlottables);
        customPlot->installEventFilter(m_overlay);
        setCentralWidget(customPlot);
    }

    m_hasOpenedInformation = false;
    m_logDialog->clear();
    m_logDialog->hide();
    ui->action_Log->setEnabled(false);
    ui->actionLoad_Log_JSON->setEnabled(true);
    ui->actionInformation->setEnabled(true);
    setWindowTitle(tr("%1 - %2").arg(m_windowTitle).arg(QFileInfo(jsonFile).fileName()));
}

void MainWindow::loadJsonLog(const QString& jsonFile)
{
    if(m_logDialog->loadLogJson(jsonFile))
    {
        ui->action_Log->setEnabled(true);
        m_logDialog->show();
    }
}

bool MainWindow::getPlotPagefileSetting() const
{
    bool plotPagefile = false;
    QSettings settings;
    plotPagefile = settings.value("PlotPagefile", plotPagefile).toBool();
    return plotPagefile;
}

void MainWindow::overlayCursorChangedSlot(QPoint pos)
{
    m_lastPos = pos;
    if(m_customPlot)
    {
        DWORD selectedPid = 0, selectedPpid = 0;
        if (m_selectedGraph)
        {
            selectedPid = m_selectedGraph->property("PID").toUInt();
            selectedPpid = m_selectedGraph->property("PPID").toUInt();
        }
        
        auto coord = m_customPlot->xAxis->pixelToCoord(pos.x());
        coord = std::round(coord);
        if(coord >= 0 && coord < m_times.size())
        {
            uint64_t time = m_times[coord];
            auto itr = m_timeline.find(time);
            if(itr != m_timeline.end())
            {
                QString info;
                auto ms = time - m_times[0];
                size_t memoryUsageSum = 0;
                size_t pagefileUsageSum = 0;
                QTime t = QTime(0, 0).addMSecs(ms);
                info += QString("%1 (%2 ms epoch)\n")
                        .arg(t.toString("hh:mm:ss"))
                        .arg(time);
                for(const auto& process : m_sortedProcesses)
                {
                    const UniqueProcess& up = process.uniqueProcess;
                    const ProcessData& data = itr->second[up];
                    if(!data.memoryUsage && !data.pagefileUsage && !data.cpuUsage)
                        continue; // skip non-running processes
                    memoryUsageSum += data.memoryUsage;
                    pagefileUsageSum += data.pagefileUsage;
                    if(info.size())
                        info += "\n";
                    auto selected = up.pid == selectedPid && up.ppid == selectedPpid;
                    if (selected)
                        info += "<b>";
                    info += QString("%1 (PID: %2, Parent: %3):\n  Memory usage: %4\n  Pagefile usage: %5 (%6)\n  CPU: %7\n")
                            .arg(up.name)
                            .arg(up.pid)
                            .arg(up.ppid)
                            .arg(humanReadableSize(data.memoryUsage))
                            .arg(humanReadableSize(data.pagefileUsage))
                            .arg(humanReadableSize((data.pagefileUsage >= data.memoryUsage) * (data.pagefileUsage - data.memoryUsage)))
                            .arg(QString::number(data.cpuUsage, 'f', 3));
                    if (selected)
                        info += "</b>";
                }
                info+= QString("\nTotal memory usage: %1\nTotal pagefile usage: %2 (%3)")
                        .arg(humanReadableSize(memoryUsageSum))
                        .arg(humanReadableSize(pagefileUsageSum))
                        .arg(humanReadableSize((pagefileUsageSum >= memoryUsageSum) * (pagefileUsageSum - memoryUsageSum)));
                m_informationDialog->setInformationText(info);
                if(!m_hasOpenedInformation)
                {
                    m_informationDialog->show();
                    m_hasOpenedInformation = true;
                }
                if(m_allowLogSelectionEvent)
                {
                    m_allowLogSelectionEvent = false;
                    m_logDialog->selectTime(time);
                    m_allowLogSelectionEvent = true;
                }
            }
            else
            {
                m_informationDialog->setInformationText(QString());
            }
        }
        else
        {
            m_informationDialog->setInformationText(QString());
        }
    }
}

void MainWindow::logSelectionChangedSlot(uint64_t time)
{
    if(!m_allowLogSelectionEvent)
        return;
    auto itr = std::lower_bound(m_times.begin(), m_times.end(), time);
    if(itr == m_times.end())
    {
        m_overlay->hideOverlay();
        return;
    }
    size_t idx = itr - m_times.begin();
    auto scrollX = m_customPlot->xAxis->coordToPixel(idx);
    m_allowLogSelectionEvent = false;
    m_overlay->moveOverlay(m_customPlot, scrollX);
    m_allowLogSelectionEvent = true;
}

void MainWindow::on_actionLoad_JSON_triggered()
{
    QSettings settings;
    QString directory = settings.value("BrowseDirectory").toString();
    auto jsonFile = QFileDialog::getOpenFileName(this, tr("Data JSON"), directory, tr("Data JSON (*.json)"));
    if(jsonFile.isEmpty())
        return;
    settings.setValue("BrowseDirectory", QFileInfo(jsonFile).absoluteDir().absolutePath());
    jsonFile = QDir::toNativeSeparators(jsonFile);
    loadJsonChart(jsonFile);
}

void MainWindow::on_actionLoad_Log_JSON_triggered()
{
    QSettings settings;
    QString directory = settings.value("BrowseDirectory").toString();
    auto jsonFile = QFileDialog::getOpenFileName(this, tr("Log JSON"), directory, tr("Log JSON (*.json)"));
    if(jsonFile.isEmpty())
        return;
    settings.setValue("BrowseDirectory", QFileInfo(jsonFile).absoluteDir().absolutePath());
    jsonFile = QDir::toNativeSeparators(jsonFile);
    loadJsonLog(jsonFile);
}

void MainWindow::on_actionInformation_triggered()
{
    m_informationDialog->show();
}

void MainWindow::on_action_Log_triggered()
{
    m_logDialog->show();
}

void MainWindow::on_actionPlot_pagefile_toggled(bool checked)
{
    QSettings settings;
    settings.setValue("PlotPagefile", checked);
    if(!m_timeline.empty())
        QMessageBox::information(this, tr("Information"), tr("Reload the data to change the plot."));
}
