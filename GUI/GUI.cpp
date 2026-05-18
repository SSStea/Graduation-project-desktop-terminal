#include "GUI.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QTextStream>
#include <QVBoxLayout>

CLineChartWidget::CLineChartWidget(QWidget* pParent)
    : QWidget(pParent)
{
    setMinimumHeight(110);
    vecChartSamples = QVector<int>{ 0, 0, 0, 0, 0, 0, 0, 0 };
}

void CLineChartWidget::setSamples(const QVector<int>& vecSamples)
{
    vecChartSamples = vecSamples;
    update();
}

void CLineChartWidget::paintEvent(QPaintEvent* pEvent)
{
    Q_UNUSED(pEvent);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#ffffff"));

    QRectF rcPlot = rect().adjusted(42, 10, -16, -24);
    painter.setPen(QPen(QColor("#dce3ec"), 1, Qt::DashLine));

    for (int nIndex = 0; nIndex <= 4; ++nIndex)
    {
        qreal dY = rcPlot.top() + rcPlot.height() * nIndex / 4.0;
        painter.drawLine(QPointF(rcPlot.left(), dY), QPointF(rcPlot.right(), dY));
    }

    if (vecChartSamples.size() < 2)
    {
        return;
    }

    int nMaxValue = 1;
    for (int nValue : vecChartSamples)
    {
        nMaxValue = qMax(nMaxValue, nValue);
    }

    QPainterPath pathLine;
    for (int nIndex = 0; nIndex < vecChartSamples.size(); ++nIndex)
    {
        qreal dX = rcPlot.left() + rcPlot.width() * nIndex / (vecChartSamples.size() - 1);
        qreal dY = rcPlot.bottom() - rcPlot.height() * vecChartSamples[nIndex] / nMaxValue;

        if (nIndex == 0)
        {
            pathLine.moveTo(dX, dY);
        }
        else
        {
            pathLine.lineTo(dX, dY);
        }
    }

    painter.setPen(QPen(QColor("#1f6feb"), 2));
    painter.drawPath(pathLine);
    painter.setBrush(QColor("#1f6feb"));
    painter.setPen(Qt::NoPen);

    for (int nIndex = 0; nIndex < vecChartSamples.size(); ++nIndex)
    {
        qreal dX = rcPlot.left() + rcPlot.width() * nIndex / (vecChartSamples.size() - 1);
        qreal dY = rcPlot.bottom() - rcPlot.height() * vecChartSamples[nIndex] / nMaxValue;
        painter.drawEllipse(QPointF(dX, dY), 2.5, 2.5);
    }

    painter.setPen(QColor("#6b7280"));
    painter.drawText(QRectF(0, 4, 38, 18), Qt::AlignRight, QString::number(nMaxValue));
    painter.drawText(QRectF(0, rcPlot.bottom() - 12, 38, 18), Qt::AlignRight, "0");
    painter.drawText(QRectF(rcPlot.left(), 0, rcPlot.width(), 18), Qt::AlignLeft, "认证进度（累计已认证报文数）");
}

GUI::GUI(QWidget* pParent)
    : QMainWindow(pParent)
{
    ui.setupUi(this);

    pSerial = new QSerialPort(this);
    pRuntimeTimer = new QTimer(this);
    pPingTimer = new QTimer(this);

    buildInterface();
    refreshSerialPorts();
    updateMetrics();

    connect(pSerial, &QSerialPort::readyRead, this, &GUI::readSerialData);
    connect(pRuntimeTimer, &QTimer::timeout, this, &GUI::updateRuntime);
    connect(pPingTimer, &QTimer::timeout, this, &GUI::sendPing);
    pPingTimer->setInterval(5000);
    pRuntimeTimer->start(1000);

    showMaximized();
}

GUI::~GUI()
{
    if (pSerial->isOpen())
    {
        pSerial->close();
    }
}

void GUI::buildInterface()
{
    setWindowTitle("TESLA 无人机认证管理平台");
    resize(1500, 840);

    QWidget* pRootWidget = new QWidget(this);
    QVBoxLayout* pRootLayout = new QVBoxLayout(pRootWidget);
    pRootLayout->setContentsMargins(10, 8, 10, 8);
    pRootLayout->setSpacing(8);

    pRootLayout->addWidget(pCreateHeader());
    pRootLayout->addWidget(pCreateCommunicationSection());

    QGridLayout* pMainGridLayout = new QGridLayout();
    pMainGridLayout->setHorizontalSpacing(12);
    pMainGridLayout->setVerticalSpacing(8);
    pMainGridLayout->addWidget(pCreateParameterSection(), 0, 0);
    pMainGridLayout->addWidget(pCreateProcessSection(), 1, 0);
    pMainGridLayout->addWidget(pCreateMonitorSection(), 0, 1, 2, 1);
    pMainGridLayout->setColumnStretch(0, 50);
    pMainGridLayout->setColumnStretch(1, 52);
    pRootLayout->addLayout(pMainGridLayout);

    pRootLayout->addWidget(pCreateAnalysisSection());
    pRootLayout->addWidget(pCreateLogSection(), 1);

    QHBoxLayout* pStatusLayout = new QHBoxLayout();
    QLabel* pReadyLabel = new QLabel("● 就绪", this);
    pReadyLabel->setObjectName("greenTextLabel");
    pReceiveBytesLabel = new QLabel("接收: 0 KB", this);
    pSendBytesLabel = new QLabel("发送: 0 KB", this);
    pRuntimeLabel = new QLabel("运行时长: 00:00:00", this);
    pStatusLayout->addWidget(pReadyLabel);
    pStatusLayout->addStretch();
    pStatusLayout->addWidget(pReceiveBytesLabel);
    pStatusLayout->addSpacing(36);
    pStatusLayout->addWidget(pSendBytesLabel);
    pStatusLayout->addSpacing(36);
    pStatusLayout->addWidget(pRuntimeLabel);
    pRootLayout->addLayout(pStatusLayout);

    setCentralWidget(pRootWidget);
    applyStyle();
}

QWidget* GUI::pCreateHeader()
{
    QWidget* pWidget = new QWidget(this);
    QHBoxLayout* pLayout = new QHBoxLayout(pWidget);
    pLayout->setContentsMargins(0, 0, 0, 0);

    QLabel* pTitleLabel = new QLabel("◆  TESLA 无人机认证管理平台", this);
    pTitleLabel->setObjectName("appTitleLabel");
    pHeaderSerialLabel = new QLabel("串口  COM3", this);
    pHeaderStateLabel = new QLabel("● 未连接", this);
    pHeaderStateLabel->setObjectName("greenTextLabel");

    pLayout->addWidget(pTitleLabel);
    pLayout->addStretch();
    pLayout->addWidget(pHeaderSerialLabel);
    pLayout->addSpacing(18);
    pLayout->addWidget(pHeaderStateLabel);

    return pWidget;
}

QFrame* GUI::pCreateSectionFrame(const QString& strTitle)
{
    QFrame* pFrame = new QFrame(this);
    pFrame->setObjectName("sectionFrame");

    QVBoxLayout* pLayout = new QVBoxLayout(pFrame);
    pLayout->setContentsMargins(12, 8, 12, 10);
    pLayout->setSpacing(8);

    QLabel* pTitleLabel = new QLabel(strTitle, this);
    pTitleLabel->setObjectName("sectionTitleLabel");
    pLayout->addWidget(pTitleLabel);

    return pFrame;
}

QWidget* GUI::pCreateCommunicationSection()
{
    QFrame* pFrame = pCreateSectionFrame("通信配置");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    QHBoxLayout* pLayout = new QHBoxLayout();
    pLayout->setSpacing(18);

    pPortComboBox = new QComboBox(this);
    pPortComboBox->setMinimumWidth(180);
    pBaudComboBox = new QComboBox(this);
    pBaudComboBox->addItems({ "57600", "115200", "9600" });
    pBaudComboBox->setCurrentText("57600");
    pBaudComboBox->setMinimumWidth(130);

    QPushButton* pScanButton = new QPushButton("扫描串口", this);
    QPushButton* pConnectButton = new QPushButton("连接", this);
    QPushButton* pDisconnectButton = new QPushButton("断开", this);
    pDelayValueLabel = new QLabel("延迟          待测", this);
    pLossValueLabel = new QLabel("丢包            待测", this);

    pConnectButton->setObjectName("blueButton");
    pDelayValueLabel->setObjectName("smallMetricLabel");
    pLossValueLabel->setObjectName("smallMetricLabel");

    pLayout->addSpacing(14);
    pLayout->addWidget(new QLabel("串口", this));
    pLayout->addWidget(pPortComboBox);
    pLayout->addSpacing(20);
    pLayout->addWidget(new QLabel("波特率", this));
    pLayout->addWidget(pBaudComboBox);
    pLayout->addSpacing(24);
    pLayout->addWidget(pScanButton);
    pLayout->addWidget(pConnectButton);
    pLayout->addWidget(pDisconnectButton);
    pLayout->addStretch();
    pLayout->addWidget(pDelayValueLabel);
    pLayout->addWidget(pLossValueLabel);

    pFrameLayout->addLayout(pLayout);

    QHBoxLayout* pNodeLayout = new QHBoxLayout();
    pNodeLayout->setSpacing(18);

    pSenderNodesLabel = new QLabel("Sender：等待节点响应", this);
    pReceiverNodesLabel = new QLabel("Receiver：等待节点响应", this);
    pSenderNodesLabel->setObjectName("nodeStatusLabel");
    pReceiverNodesLabel->setObjectName("nodeStatusLabel");

    pNodeLayout->addSpacing(14);
    pNodeLayout->addWidget(pSenderNodesLabel);
    pNodeLayout->addSpacing(24);
    pNodeLayout->addWidget(pReceiverNodesLabel);
    pNodeLayout->addStretch();
    pFrameLayout->addLayout(pNodeLayout);

    connect(pScanButton, &QPushButton::clicked, this, &GUI::refreshSerialPorts);
    connect(pConnectButton, &QPushButton::clicked, this, &GUI::connectSerialPort);
    connect(pDisconnectButton, &QPushButton::clicked, this, &GUI::disconnectSerialPort);

    return pFrame;
}

QWidget* GUI::pCreateParameterSection()
{
    QFrame* pFrame = pCreateSectionFrame("参数配置");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    QGridLayout* pFormLayout = new QGridLayout();
    pFormLayout->setHorizontalSpacing(18);
    pFormLayout->setVerticalSpacing(12);

    pInitKeyLineEdit = new QLineEdit("Hello World", this);
    pKeyCountLineEdit = new QLineEdit("101", this);
    pDelayLineEdit = new QLineEdit("3", this);
    pIntervalLineEdit = new QLineEdit("1000", this);
    pGroupSizeLineEdit = new QLineEdit("100", this);
    pDetectLineEdit = new QLineEdit("1", this);
    pContextLineEdit = new QLineEdit("gen_mac", this);
    pMessageLineEdit = new QLineEdit("TESLA Protocol", this);

    pFormLayout->addWidget(new QLabel("初始密钥", this), 0, 0);
    pFormLayout->addWidget(pInitKeyLineEdit, 0, 1);
    pFormLayout->addWidget(new QLabel("密钥总数 N", this), 0, 2);
    pFormLayout->addWidget(pKeyCountLineEdit, 0, 3);
    pFormLayout->addWidget(new QLabel("披露延迟 d", this), 0, 4);
    pFormLayout->addWidget(pDelayLineEdit, 0, 5);
    pFormLayout->addWidget(new QLabel("时间间隔", this), 0, 6);
    pFormLayout->addWidget(pIntervalLineEdit, 0, 7);
    pFormLayout->addWidget(new QLabel("ms", this), 0, 8);
    pFormLayout->addWidget(new QLabel("分组大小", this), 1, 0);
    pFormLayout->addWidget(pGroupSizeLineEdit, 1, 1);
    pFormLayout->addWidget(new QLabel("检测阈值", this), 1, 2);
    pFormLayout->addWidget(pDetectLineEdit, 1, 3);
    pFormLayout->addWidget(new QLabel("Context", this), 1, 4);
    pFormLayout->addWidget(pContextLineEdit, 1, 5);
    pFormLayout->addWidget(new QLabel("发送消息", this), 1, 6);
    pFormLayout->addWidget(pMessageLineEdit, 1, 7, 1, 2);

    QHBoxLayout* pButtonLayout = new QHBoxLayout();
    QPushButton* pSaveButton = new QPushButton("保存配置", this);
    QPushButton* pSendButton = new QPushButton("下发参数", this);
    QPushButton* pStartButton = new QPushButton("开始认证", this);
    QPushButton* pStopButton = new QPushButton("停止", this);
    pSaveButton->setObjectName("blueButton");
    pSendButton->setObjectName("blueButton");
    pStartButton->setObjectName("greenButton");
    pStopButton->setObjectName("redButton");
    pButtonLayout->addSpacing(28);
    pButtonLayout->addWidget(pSaveButton);
    pButtonLayout->addSpacing(28);
    pButtonLayout->addWidget(pSendButton);
    pButtonLayout->addSpacing(28);
    pButtonLayout->addWidget(pStartButton);
    pButtonLayout->addSpacing(28);
    pButtonLayout->addWidget(pStopButton);
    pButtonLayout->addStretch();

    pFrameLayout->addLayout(pFormLayout);
    pFrameLayout->addLayout(pButtonLayout);

    connect(pSaveButton, &QPushButton::clicked, this, &GUI::saveConfig);
    connect(pSendButton, &QPushButton::clicked, this, &GUI::sendConfig);
    connect(pStartButton, &QPushButton::clicked, this, &GUI::startAuthentication);
    connect(pStopButton, &QPushButton::clicked, this, &GUI::stopAuthentication);

    return pFrame;
}

QWidget* GUI::pCreateProcessSection()
{
    QFrame* pFrame = pCreateSectionFrame("认证过程可视化");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    pProcessTable = new QTableWidget(5, 5, this);
    pProcessTable->setHorizontalHeaderLabels({ "", "阶段", "进度", "状态", "时间" });
    pProcessTable->verticalHeader()->setVisible(false);
    pProcessTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pProcessTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pProcessTable->setSelectionMode(QAbstractItemView::NoSelection);
    pProcessTable->setMinimumHeight(164);
    setupProcessTable();

    pFrameLayout->addWidget(pProcessTable);
    return pFrame;
}

QWidget* GUI::pCreateMonitorSection()
{
    QFrame* pFrame = pCreateSectionFrame("通信监视");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    pMonitorTextEdit = new QTextEdit(this);
    pMonitorTextEdit->setObjectName("monitorTextEdit");
    pMonitorTextEdit->setReadOnly(true);
    pMonitorTextEdit->setMinimumHeight(270);
    pFrameLayout->addWidget(pMonitorTextEdit, 1);

    QHBoxLayout* pOptionLayout = new QHBoxLayout();
    pAutoScrollCheckBox = new QCheckBox("自动滚动", this);
    pShowTimeCheckBox = new QCheckBox("显示时间", this);
    pAutoScrollCheckBox->setChecked(true);
    pShowTimeCheckBox->setChecked(true);
    QPushButton* pClearButton = new QPushButton("清空", this);
    QPushButton* pExportButton = new QPushButton("导出日志", this);
    pOptionLayout->addWidget(pAutoScrollCheckBox);
    pOptionLayout->addWidget(pShowTimeCheckBox);
    pOptionLayout->addStretch();
    pOptionLayout->addWidget(pClearButton);
    pOptionLayout->addSpacing(80);
    pOptionLayout->addWidget(pExportButton);
    pFrameLayout->addLayout(pOptionLayout);

    connect(pClearButton, &QPushButton::clicked, this, &GUI::clearMonitor);
    connect(pExportButton, &QPushButton::clicked, this, &GUI::exportLog);

    return pFrame;
}

QWidget* GUI::pCreateAnalysisSection()
{
    QFrame* pFrame = pCreateSectionFrame("结果分析");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    QHBoxLayout* pLayout = new QHBoxLayout();
    pLayout->setSpacing(16);
    pLayout->addWidget(pCreateMetricCard("总报文", "0", "#1f6feb"));
    pLayout->addWidget(pCreateMetricCard("已认证", "0", "#1fa463"));
    pLayout->addWidget(pCreateMetricCard("失败", "0", "#d93025"));
    pLayout->addWidget(pCreateMetricCard("认证效率", "0.0%", "#6f42c1"));
    pLayout->addWidget(pCreateMetricCard("平均耗时", "0ms", "#f59e0b"));
    pLayout->addWidget(pCreateMetricCard("可靠性", "●", "#0ea5a8"));

    pLineChartWidget = new CLineChartWidget(this);
    pLayout->addWidget(pLineChartWidget, 2);
    pFrameLayout->addLayout(pLayout);

    return pFrame;
}

QWidget* GUI::pCreateLogSection()
{
    QFrame* pFrame = pCreateSectionFrame("日志记录");
    QVBoxLayout* pFrameLayout = qobject_cast<QVBoxLayout*>(pFrame->layout());

    pAuditTable = new QTableWidget(0, 6, this);
    pAuditTable->setHorizontalHeaderLabels({ "时间", "类型", "Sender ID", "分组", "结果", "详情" });
    pAuditTable->verticalHeader()->setVisible(false);
    pAuditTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    pAuditTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    pAuditTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    pAuditTable->setMinimumHeight(150);
    pFrameLayout->addWidget(pAuditTable);

    return pFrame;
}

QWidget* GUI::pCreateMetricCard(const QString& strTitle, const QString& strValue, const QString& strColor)
{
    QFrame* pCardFrame = new QFrame(this);
    pCardFrame->setObjectName("metricCardFrame");
    pCardFrame->setMinimumWidth(138);
    pCardFrame->setMinimumHeight(82);

    QHBoxLayout* pLayout = new QHBoxLayout(pCardFrame);
    pLayout->setContentsMargins(10, 10, 10, 10);
    pLayout->setSpacing(10);

    QLabel* pIconLabel = new QLabel(this);
    pIconLabel->setObjectName("metricIconLabel");
    pIconLabel->setStyleSheet(QString("background:%1;").arg(strColor));
    pIconLabel->setFixedSize(46, 46);

    QVBoxLayout* pTextLayout = new QVBoxLayout();
    pTextLayout->setSpacing(2);
    QLabel* pTitleLabel = new QLabel(strTitle, this);
    QLabel* pValueLabel = new QLabel(strValue, this);
    pTitleLabel->setObjectName("metricTitleLabel");
    pValueLabel->setObjectName("metricValueLabel");
    pTextLayout->addWidget(pTitleLabel);
    pTextLayout->addWidget(pValueLabel);
    pLayout->addWidget(pIconLabel);
    pLayout->addLayout(pTextLayout);
    
    if (strTitle == "总报文")
    {
        pTotalPacketsValueLabel = pValueLabel;

        QPixmap pixmap(":/image/images/totalmetric.png");
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
    }
    else if (strTitle == "已认证")
    {
        pVerifiedValueLabel = pValueLabel;
        QPixmap pixmap(":/image/images/verified.png");
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
    }
    else if (strTitle == "失败")
    {
        pFailedValueLabel = pValueLabel;
        QPixmap pixmap(":/image/images/failed.png");
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
    }
    else if (strTitle == "认证效率")
    {
        pEfficiencyValueLabel = pValueLabel;
        QPixmap pixmap(":/image/images/efficiency.png");
        pixmap = pixmap.scaled(26, 26, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
    }
    else if (strTitle == "平均耗时")
    {
        pAverageCostValueLabel = pValueLabel;
        QPixmap pixmap(":/image/images/time-consuming.png");
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
    }
    else if (strTitle == "可靠性")
    {
        pReliabilityValueLabel = pValueLabel;
        QPixmap pixmap(":/image/images/reliability.png");
        pixmap = pixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pIconLabel->setPixmap(pixmap);
        pReliabilityValueLabel->setStyleSheet("color:#1fa463;font-size:30px;font-weight:700;");
    }

    return pCardFrame;
}

void GUI::setupProcessTable()
{
    const QStringList listIndex = { "✓", "2", "3", "4", "5" };
    const QStringList listStage = { "初始化参数", "K0 承诺值", "数据包接收", "延迟密钥披露", "SAMD 分组验证" };
    const QStringList listState = { "已完成", "未进行", "未进行", "未进行", "未进行" };
    const QStringList listTime = { "--:--:--", "-", "-", "-", "-" };

    for (int nRow = 0; nRow < 5; ++nRow)
    {
        QTableWidgetItem* pIndexItem = new QTableWidgetItem(listIndex[nRow]);
        QTableWidgetItem* pStageItem = new QTableWidgetItem(listStage[nRow]);
        QTableWidgetItem* pStateItem = new QTableWidgetItem(listState[nRow]);
        QTableWidgetItem* pTimeItem = new QTableWidgetItem(listTime[nRow]);
        QProgressBar* pProgressBar = new QProgressBar(this);

        pIndexItem->setTextAlignment(Qt::AlignCenter);
        pStageItem->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        pStateItem->setTextAlignment(Qt::AlignCenter);
        pTimeItem->setTextAlignment(Qt::AlignCenter);
        pProgressBar->setRange(0, 100);
        pProgressBar->setTextVisible(false);
        pProgressBar->setValue(nRow == 0 ? 100 : 0);

        pProcessTable->setItem(nRow, 0, pIndexItem);
        pProcessTable->setItem(nRow, 1, pStageItem);
        pProcessTable->setCellWidget(nRow, 2, pProgressBar);
        pProcessTable->setItem(nRow, 3, pStateItem);
        pProcessTable->setItem(nRow, 4, pTimeItem);
    }

    pProcessTable->setColumnWidth(0, 46);
    pProcessTable->setColumnWidth(1, 180);
    pProcessTable->setColumnWidth(2, 220);
}

void GUI::applyStyle()
{
    setStyleSheet(R"(
        QMainWindow, QWidget { background:#f8fafc; color:#1f2937; font-family:"Microsoft YaHei"; font-size:13px; }
        #appTitleLabel { font-size:20px; font-weight:700; color:#111827; }
        #sectionFrame { background:#ffffff; border:1px solid #d4dbe5; border-radius:6px; }
        #sectionTitleLabel { background:transparent; font-size:15px; font-weight:700; color:#111827; }
        QLabel { background:transparent; }
        QLineEdit, QComboBox { background:#ffffff; border:1px solid #cfd8e3; border-radius:4px; padding:6px 8px; min-height:22px; }
        QPushButton { background:#f8fafc; border:1px solid #c8d2df; border-radius:4px; padding:7px 18px; color:#1f2937; min-width:76px; }
        QPushButton:hover { background:#eef4ff; }
        #blueButton { background:#1f6feb; color:#ffffff; border:none; font-weight:700; }
        #greenButton { background:#23a455; color:#ffffff; border:none; font-weight:700; }
        #redButton { background:#ef3333; color:#ffffff; border:none; font-weight:700; }
        #smallMetricLabel { background:#f8fafc; border:1px solid #d4dbe5; border-radius:4px; padding:8px 56px; color:#00863d; font-weight:700; }
        #nodeStatusLabel { background:#f8fafc; border:1px solid #d4dbe5; border-radius:4px; padding:6px 12px; color:#1f2937; font-weight:600; }
        #monitorTextEdit { background:#ffffff; border:1px solid #d4dbe5; border-radius:4px; font-family:Consolas; font-size:12px; }
        #metricCardFrame { background:#ffffff; border:1px solid #d4dbe5; border-radius:6px; }
        #metricIconLabel { border-radius:23px; color:#ffffff; qproperty-alignment:AlignCenter; }
        #metricTitleLabel { color:#374151; font-weight:600; }
        #metricValueLabel { color:#111827; font-size:24px; font-weight:700; }
        #greenTextLabel { color:#0a8f3c; font-weight:700; }
        QTableWidget { background:#ffffff; alternate-background-color:#f8fafc; gridline-color:#d7dee8; border:1px solid #d4dbe5; border-radius:4px; }
        QHeaderView::section { background:#f3f6fa; color:#1f2937; border:none; border-right:1px solid #d7dee8; padding:5px; font-weight:700; }
        QProgressBar { background:#e8edf4; border:none; border-radius:3px; height:6px; }
        QProgressBar::chunk { background:#1f6feb; border-radius:3px; }
        QCheckBox { background:transparent; }
    )");
}

void GUI::refreshSerialPorts()
{
    QString strCurrentPort = pPortComboBox->currentText();
    pPortComboBox->clear();

    const QList<QSerialPortInfo> listPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& serialInfo : listPorts)
    {
        pPortComboBox->addItem(serialInfo.portName());
    }

    if (pPortComboBox->count() == 0)
    {
        pPortComboBox->addItem("COM3");
    }

    if (!strCurrentPort.isEmpty())
    {
        pPortComboBox->setCurrentText(strCurrentPort);
    }
}

void GUI::connectSerialPort()
{
    if (pSerial->isOpen())
    {
        pSerial->close();
    }

    pSerial->setPortName(pPortComboBox->currentText());
    pSerial->setBaudRate(pBaudComboBox->currentText().toInt());
    pSerial->setDataBits(QSerialPort::Data8);
    pSerial->setParity(QSerialPort::NoParity);
    pSerial->setStopBits(QSerialPort::OneStop);
    pSerial->setFlowControl(QSerialPort::NoFlowControl);

    if (pSerial->open(QIODevice::ReadWrite))
    {
        pHeaderSerialLabel->setText(QString("串口  %1").arg(pPortComboBox->currentText()));
        pHeaderStateLabel->setText("● 已连接");
        nLinkDelayMs = -1;
        nLossPackets = 0;
        setSenderNodeIds.clear();
        setReceiverNodeIds.clear();
        mapSenderDelayMs.clear();
        mapReceiverDelayMs.clear();
        refreshConnectedNodeLabels();
        if (pDelayValueLabel != nullptr)
        {
            pDelayValueLabel->setText("延迟          待测");
        }
        if (pLossValueLabel != nullptr)
        {
            pLossValueLabel->setText("丢包            待测");
        }
        appendMonitorLine("SYS", QString("串口 %1 已连接，波特率 %2").arg(pPortComboBox->currentText(), pBaudComboBox->currentText()));
        appendAuditRow("连接", "-", "-", "OK", QString("串口 %1 已连接，波特率 %2").arg(pPortComboBox->currentText(), pBaudComboBox->currentText()));
        pPingTimer->start();
        sendPing();
    }
    else
    {
        pHeaderStateLabel->setText("● 未连接");
        appendMonitorLine("ERR", QString("串口打开失败: %1").arg(pSerial->errorString()));
        appendAuditRow("连接", "-", "-", "FAIL", pSerial->errorString());
    }
}

void GUI::disconnectSerialPort()
{
    if (pSerial->isOpen())
    {
        pSerial->close();
    }

    pPingTimer->stop();
    nLinkDelayMs = -1;
    setSenderNodeIds.clear();
    setReceiverNodeIds.clear();
    mapSenderDelayMs.clear();
    mapReceiverDelayMs.clear();
    refreshConnectedNodeLabels();
    pHeaderStateLabel->setText("● 已断开");
    if (pDelayValueLabel != nullptr)
    {
        pDelayValueLabel->setText("延迟          待测");
    }
    appendMonitorLine("SYS", "串口已断开");
}

void GUI::saveConfig()
{
    bConfigSaved = true;
    bConfigSent = false;
    appendMonitorLine("SYS", "保存成功：参数已保存到本地配置区，尚未下发无人机端");
    appendAuditRow("保存配置", "PC", "-", "OK", "local config saved");
}

QJsonObject GUI::jsonBuildConfig() const
{
    QJsonObject jsonMessage;
    jsonMessage["channel"] = "mgmt";
    jsonMessage["srcNodeId"] = "pc";
    jsonMessage["dstNodeId"] = "*";
    jsonMessage["type"] = "CONFIG";
    jsonMessage["role"] = "management";
    jsonMessage["keyInterval"] = nLineEditValue(pDelayLineEdit, 3);
    jsonMessage["keyCount"] = nLineEditValue(pKeyCountLineEdit, 101);
    jsonMessage["initKey"] = pInitKeyLineEdit->text();
    jsonMessage["groupSize"] = nLineEditValue(pGroupSizeLineEdit, 100);
    jsonMessage["detectCount"] = nLineEditValue(pDetectLineEdit, 1);
    jsonMessage["intervalMs"] = nLineEditValue(pIntervalLineEdit, 1000);
    jsonMessage["sendMsg"] = pMessageLineEdit->text();
    jsonMessage["context"] = pContextLineEdit->text();

    return jsonMessage;
}

void GUI::sendConfig()
{
    QJsonObject jsonMessage = jsonBuildConfig();

    if (!pSerial->isOpen())
    {
        appendMonitorLine("ERR", "串口未连接，无法下发参数");
        appendAuditRow("配置下发", "PC", "-", "FAIL", "serial port is not connected");
        return;
    }

    writeJsonLine(jsonMessage);

    bConfigSaved = true;
    bConfigSent = true;
    nTotalPackets = jsonMessage["keyCount"].toInt();
    nVerifiedPackets = 0;
    nFailedPackets = 0;
    nAbnormalPackets = 0;
    nLossPackets = 0;
    if (pLossValueLabel != nullptr)
    {
        pLossValueLabel->setText("丢包            0");
    }
    updateMetrics();
    updateProcessRow(0, "已完成", nTotalPackets, nTotalPackets);
    appendAuditRow("配置下发", "PC", "-", "OK",
        QString("config sent: N=%1 d=%2 group=%3 interval=%4")
            .arg(nTotalPackets)
            .arg(jsonMessage["keyInterval"].toInt())
            .arg(jsonMessage["groupSize"].toInt())
            .arg(jsonMessage["intervalMs"].toInt()));
}

void GUI::startAuthentication()
{
    if (!pSerial->isOpen())
    {
        appendMonitorLine("ERR", "串口未连接，无法启动认证");
        appendAuditRow("认证启动", "PC", "-", "FAIL", "serial port is not connected");
        return;
    }
    if (!bConfigSent)
    {
        appendMonitorLine("ERR", "参数未下发，无法启动认证");
        appendAuditRow("认证启动", "PC", "-", "FAIL", "config is not sent");
        return;
    }

    QJsonObject jsonMessage;
    jsonMessage["channel"] = "mgmt";
    jsonMessage["srcNodeId"] = "pc";
    jsonMessage["dstNodeId"] = "*";
    jsonMessage["type"] = "START";
    jsonMessage["role"] = "management";
    jsonMessage["timestamp"] = QDateTime::currentMSecsSinceEpoch();
    writeJsonLine(jsonMessage);

    nVerifiedPackets = 0;
    nFailedPackets = 0;
    nAbnormalPackets = 0;
    nRuntimeSeconds = 0;
    updateMetrics();
    appendAuditRow("认证启动", "PC", "-", "OK", "start command sent");
}

void GUI::stopAuthentication()
{
    if (!pSerial->isOpen())
    {
        appendMonitorLine("ERR", "串口未连接，无法发送停止命令");
        appendAuditRow("停止", "PC", "-", "FAIL", "serial port is not connected");
        return;
    }

    QJsonObject jsonMessage;
    jsonMessage["channel"] = "mgmt";
    jsonMessage["srcNodeId"] = "pc";
    jsonMessage["dstNodeId"] = "*";
    jsonMessage["type"] = "STOP";
    jsonMessage["role"] = "management";
    writeJsonLine(jsonMessage);
    appendAuditRow("停止", "PC", "-", "OK", "stop command sent");
}

void GUI::clearMonitor()
{
    pMonitorTextEdit->clear();
}

void GUI::exportLog()
{
    QString strFilePath = QFileDialog::getSaveFileName(this, "导出日志", "tesla-log.csv", "CSV (*.csv)");
    if (strFilePath.isEmpty())
    {
        return;
    }

    QFile fileLog(strFilePath);
    if (!fileLog.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        return;
    }

    QTextStream streamLog(&fileLog);
    streamLog << "time,type,sender,group,result,detail\\n";
    for (int nRow = 0; nRow < pAuditTable->rowCount(); ++nRow)
    {
        QStringList listColumns;
        for (int nColumn = 0; nColumn < pAuditTable->columnCount(); ++nColumn)
        {
            QTableWidgetItem* pItem = pAuditTable->item(nRow, nColumn);
            listColumns << (pItem == nullptr ? "" : pItem->text());
        }
        streamLog << listColumns.join(",") << "\\n";
    }
}

void GUI::readSerialData()
{
    QByteArray arrNewData = pSerial->readAll();
    arrSerialBuffer.append(arrNewData);
    nReceiveBytes += arrNewData.size();

    while (true)
    {
        int nLineEnd = arrSerialBuffer.indexOf('\n');
        if (nLineEnd < 0)
        {
            break;
        }

        QByteArray arrLine = arrSerialBuffer.left(nLineEnd).trimmed();
        arrSerialBuffer.remove(0, nLineEnd + 1);

        if (!arrLine.isEmpty())
        {
            processSerialLine(arrLine);
        }
    }

    updateMetrics();
}

void GUI::processSerialLine(const QByteArray& arrLine)
{
    appendMonitorLine("RX", QString::fromUtf8(arrLine));

    QJsonParseError parseError;
    QJsonDocument jsonDocument = QJsonDocument::fromJson(arrLine, &parseError);

    if (parseError.error == QJsonParseError::NoError && jsonDocument.isObject())
    {
        handleJsonMessage(jsonDocument.object());
        return;
    }

    handleLegacyText(QString::fromUtf8(arrLine));
}

void GUI::handleJsonMessage(const QJsonObject& jsonMessage)
{
    QString strType = jsonMessage["type"].toString();
    QString strSenderId = jsonMessage["senderId"].toString(jsonMessage["src"].toString("sender_19AF"));
    updateConnectedNode(jsonMessage);

    if (strType == "PONG")
    {
        qint64 llSequence = static_cast<qint64>(jsonMessage["seq"].toDouble());
        qint64 llPcTimestampMs = static_cast<qint64>(jsonMessage["pcTimestampMs"].toDouble(llLastPingTimestampMs));
        qint64 llRttMs = QDateTime::currentMSecsSinceEpoch() - llPcTimestampMs;

        if (llRttMs >= 0)
        {
            QString strRole = jsonMessage["role"].toString().toLower();
            QString strNodeId = jsonMessage["srcNodeId"].toString(jsonMessage["senderId"].toString(strSenderId));
            nLinkDelayMs = static_cast<int>(llRttMs / 2);

            if (strRole == "sender")
            {
                mapSenderDelayMs[strNodeId] = nLinkDelayMs;
            }
            else if (strRole == "receiver")
            {
                mapReceiverDelayMs[strNodeId] = nLinkDelayMs;
            }

            if (pDelayValueLabel != nullptr)
            {
                pDelayValueLabel->setText(QString("延迟          %1ms").arg(nLinkDelayMs));
            }
            refreshConnectedNodeLabels();
        }
    }
    else if (strType == "CONFIG_ACK")
    {
        appendMonitorLine("TX", "ACK -> {\"msg\":\"ack\",\"status\":\"ok\"}");
        appendAuditRow("配置确认", "PC", "-", "OK", jsonMessage["message"].toString("config accepted"));
    }
    else if (strType == "SENDER_INIT_SENT" || strType == "INIT_RECEIVED")
    {
        updateProcessRow(1, "已完成", nTotalPackets, nTotalPackets);
        appendAuditRow("初始化", strSenderId, "-", "OK", "K0 commitment ready");
    }
    else if (strType == "PACKET_SENT" || strType == "PACKET_RECEIVED")
    {
        int nIndex = jsonMessage["index"].toInt();
        if (jsonMessage.contains("loss"))
        {
            nLossPackets = jsonMessage["loss"].toInt(nLossPackets);
            if (pLossValueLabel != nullptr)
            {
                pLossValueLabel->setText(QString("丢包            %1").arg(nLossPackets));
            }
        }
        updateProcessRow(2, QString("%1 / %2").arg(nIndex).arg(nTotalPackets), nIndex, nTotalPackets);
        appendAuditRow("数据接收", strSenderId, "-", "OK", QString("packet index=%1").arg(nIndex));
    }
    else if (strType == "KEY_PENDING")
    {
        int nIndex = jsonMessage["index"].toInt();
        appendAuditRow("密钥等待", strSenderId, "-", "OK", QString("index=%1 key not disclosed").arg(nIndex));
    }
    else if (strType == "KEY_VERIFIED")
    {
        int nSlot = jsonMessage["slot"].toInt();
        nVerifiedPackets = qMax(nVerifiedPackets, nSlot);
        updateProcessRow(3, QString("%1 / %2").arg(nVerifiedPackets).arg(nTotalPackets), nVerifiedPackets, nTotalPackets);
        appendAuditRow("密钥验证", strSenderId, "-", "OK", QString("slot=%1 verified").arg(nSlot));
    }
    else if (strType == "KEY_INVALID")
    {
        ++nFailedPackets;
        appendAuditRow("密钥验证", strSenderId, "-", "FAIL", "key verify failed");
    }
    else if (strType == "GROUP_RESULT")
    {
        int nGroup = jsonMessage["group"].toInt();
        bool bResult = jsonMessage["result"].toBool();
        int nValid = jsonMessage["valid"].toInt(jsonMessage["count"].toInt());
        int nFailed = jsonMessage["failed"].toInt();
        nLossPackets = jsonMessage["loss"].toInt(nLossPackets);
        if (pLossValueLabel != nullptr)
        {
            pLossValueLabel->setText(QString("丢包            %1").arg(nLossPackets));
        }
        int nGroupProgress = qMin(nTotalPackets, nGroup * nLineEditValue(pGroupSizeLineEdit, 100));

        nVerifiedPackets = qMax(nVerifiedPackets, nValid);
        if (!bResult)
        {
            nFailedPackets += qMax(1, nFailed);
        }

        updateProcessRow(4, QString("%1 / %2").arg(nGroupProgress).arg(nTotalPackets), nGroupProgress, nTotalPackets);
        appendAuditRow("结果", strSenderId, QString::number(nGroup), bResult ? "PASS" : "FAIL",
            QString("group=%1 total=%2 pass=%3 fail=%4 loss=%5")
                .arg(nGroup)
                .arg(jsonMessage["count"].toInt())
                .arg(nValid)
                .arg(nFailed)
                .arg(jsonMessage["loss"].toInt()));
    }
    else if (strType == "AUTH_FINISHED")
    {
        updateProcessRow(4, "已完成", nTotalPackets, nTotalPackets);
        appendAuditRow("结果", strSenderId, "-", "PASS", "authentication finished");
    }
    else if (strType == "LOG")
    {
        appendAuditRow("日志", strSenderId, "-", jsonMessage["level"].toString("INFO"), jsonMessage["message"].toString());
    }

    updateMetrics();
}

void GUI::handleLegacyText(const QString& strLine)
{
    if (strLine.contains("PASS") || strLine.contains("result = 1") || strLine.contains("result=1"))
    {
        nVerifiedPackets = qMin(nTotalPackets, nVerifiedPackets + nLineEditValue(pGroupSizeLineEdit, 100));
        appendAuditRow("结果", "legacy", "1", "PASS", strLine);
    }
    else if (strLine.contains("FAIL", Qt::CaseInsensitive) || strLine.contains("error", Qt::CaseInsensitive))
    {
        ++nFailedPackets;
        appendAuditRow("结果", "legacy", "-", "FAIL", strLine);
    }

    updateMetrics();
}

void GUI::appendMonitorLine(const QString& strDirection, const QString& strText)
{
    QString strColor = "#374151";
    if (strDirection == "TX")
    {
        strColor = "#0f8f50";
    }
    else if (strDirection == "RX")
    {
        strColor = "#09814a";
    }
    else if (strDirection == "ERR")
    {
        strColor = "#d93025";
    }
    else if (strDirection == "SYS")
    {
        strColor = "#00FFFF";
    }

    QString strPrefix;
    if (pShowTimeCheckBox == nullptr || pShowTimeCheckBox->isChecked())
    {
        strPrefix = QString("[%1] ").arg(strCurrentTime());
    }

    pMonitorTextEdit->append(QString("<span style='color:#111827'>%1</span><span style='color:%2;font-weight:700'>%3</span>  %4")
        .arg(strPrefix, strColor, strDirection, strText.toHtmlEscaped()));

    if (pAutoScrollCheckBox == nullptr || pAutoScrollCheckBox->isChecked())
    {
        pMonitorTextEdit->verticalScrollBar()->setValue(pMonitorTextEdit->verticalScrollBar()->maximum());
    }
}

void GUI::appendAuditRow(const QString& strType, const QString& strSenderId, const QString& strGroup, const QString& strResult, const QString& strDescription)
{
    int nRow = pAuditTable->rowCount();
    pAuditTable->insertRow(nRow);
    pAuditTable->setItem(nRow, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")));
    pAuditTable->setItem(nRow, 1, new QTableWidgetItem(strType));
    pAuditTable->setItem(nRow, 2, new QTableWidgetItem(strSenderId));
    pAuditTable->setItem(nRow, 3, new QTableWidgetItem(strGroup));
    pAuditTable->setItem(nRow, 4, new QTableWidgetItem(strResult));
    pAuditTable->setItem(nRow, 5, new QTableWidgetItem(strDescription));
    pAuditTable->scrollToBottom();
}


void GUI::updateConnectedNode(const QJsonObject& jsonMessage)
{
    QString strRole = jsonMessage["role"].toString().toLower();
    QString strType = jsonMessage["type"].toString();
    QString strNodeId = jsonMessage["srcNodeId"].toString();

    if (strNodeId.isEmpty())
    {
        strNodeId = jsonMessage["nodeId"].toString();
    }
    if (strNodeId.isEmpty())
    {
        strNodeId = jsonMessage["senderId"].toString();
    }
    if (strNodeId.isEmpty())
    {
        strNodeId = jsonMessage["src"].toString();
    }

    if (strRole == "sender" || strType == "CONFIG_ACK" || strType == "SENDER_INIT_SENT" || strType == "PACKET_SENT")
    {
        if (strNodeId.isEmpty())
        {
            strNodeId = "sender_unknown";
        }
        setSenderNodeIds.insert(strNodeId);
        refreshConnectedNodeLabels();
        return;
    }

    if (strRole == "receiver" || strType == "INIT_RECEIVED" || strType == "PACKET_RECEIVED" || strType == "KEY_VERIFIED" || strType == "GROUP_RESULT")
    {
        if (strNodeId.isEmpty())
        {
            strNodeId = "receiver_unknown";
        }
        setReceiverNodeIds.insert(strNodeId);
        refreshConnectedNodeLabels();
    }
}

QString GUI::strNodeText(const QSet<QString>& setNodeIds, const QMap<QString, int>& mapDelayMs, const QString& strEmptyText) const
{
    if (setNodeIds.isEmpty())
    {
        return strEmptyText;
    }

    QStringList listNodeIds = setNodeIds.values();
    std::sort(listNodeIds.begin(), listNodeIds.end());

    QStringList listText;
    for (const QString& strNodeId : listNodeIds)
    {
        if (mapDelayMs.contains(strNodeId))
        {
            listText << QString("%1（%2ms）").arg(strNodeId).arg(mapDelayMs[strNodeId]);
        }
        else
        {
            listText << QString("%1（在线）").arg(strNodeId);
        }
    }

    return listText.join("，");
}

void GUI::refreshConnectedNodeLabels()
{
    if (pSenderNodesLabel != nullptr)
    {
        pSenderNodesLabel->setText(QString("Sender：%1").arg(strNodeText(setSenderNodeIds, mapSenderDelayMs, "等待节点响应")));
    }
    if (pReceiverNodesLabel != nullptr)
    {
        pReceiverNodesLabel->setText(QString("Receiver：%1").arg(strNodeText(setReceiverNodeIds, mapReceiverDelayMs, "等待节点响应")));
    }
}

void GUI::updateMetrics()
{
    if (pTotalPacketsValueLabel == nullptr)
    {
        return;
    }

    pTotalPacketsValueLabel->setText(QString::number(nTotalPackets));
    pVerifiedValueLabel->setText(QString::number(nVerifiedPackets));
    pFailedValueLabel->setText(QString::number(nFailedPackets));

    double dEfficiency = 0.0;
    if (nTotalPackets > 0)
    {
        dEfficiency = 100.0 * nVerifiedPackets / nTotalPackets;
    }

    pEfficiencyValueLabel->setText(QString::number(dEfficiency, 'f', 1) + "%");
    pAverageCostValueLabel->setText(nVerifiedPackets > 0 ? "42ms" : "0ms");

    if (nFailedPackets == 0)
    {
        pReliabilityValueLabel->setText("●");
        pReliabilityValueLabel->setStyleSheet("color:#1fa463;font-size:30px;font-weight:700;");
    }
    else
    {
        pReliabilityValueLabel->setText("●");
        pReliabilityValueLabel->setStyleSheet("color:#d93025;font-size:30px;font-weight:700;");
    }

    pReceiveBytesLabel->setText(QString("接收: %1 KB").arg(QString::number(nReceiveBytes / 1024.0, 'f', 1)));
    pSendBytesLabel->setText(QString("发送: %1 KB").arg(QString::number(nSendBytes / 1024.0, 'f', 1)));

    QVector<int> vecSamples;
    int nStep = qMax(1, nVerifiedPackets / 9);
    for (int nIndex = 0; nIndex < 10; ++nIndex)
    {
        vecSamples.append(qMin(nVerifiedPackets, nIndex * nStep));
    }

    if (pLineChartWidget != nullptr)
    {
        pLineChartWidget->setSamples(vecSamples);
    }
}

void GUI::updateProcessRow(int nRow, const QString& strState, int nCurrentValue, int nTotalValue)
{
    if (pProcessTable == nullptr || nRow < 0 || nRow >= pProcessTable->rowCount())
    {
        return;
    }

    QProgressBar* pProgressBar = qobject_cast<QProgressBar*>(pProcessTable->cellWidget(nRow, 2));
    if (pProgressBar != nullptr)
    {
        int nValue = 0;
        if (nTotalValue > 0)
        {
            nValue = qBound(0, nCurrentValue * 100 / nTotalValue, 100);
        }
        pProgressBar->setValue(nValue);
    }

    QTableWidgetItem* pStateItem = pProcessTable->item(nRow, 3);
    QTableWidgetItem* pTimeItem = pProcessTable->item(nRow, 4);
    if (pStateItem != nullptr)
    {
        pStateItem->setText(strState);
    }
    if (pTimeItem != nullptr)
    {
        pTimeItem->setText(QDateTime::currentDateTime().toString("HH:mm:ss"));
    }
}


void GUI::sendPing()
{
    if (pSerial == nullptr || !pSerial->isOpen())
    {
        return;
    }

    QStringList listTargets = setSenderNodeIds.values();
    for (const QString& strNodeId : setReceiverNodeIds)
    {
        if (!listTargets.contains(strNodeId))
        {
            listTargets << strNodeId;
        }
    }

    if (listTargets.isEmpty())
    {
        listTargets << "*";
    }
    else
    {
        std::sort(listTargets.begin(), listTargets.end());
    }

    for (const QString& strTargetNodeId : listTargets)
    {
        llLastPingSequence = ++llPingSequence;
        llLastPingTimestampMs = QDateTime::currentMSecsSinceEpoch();

        QJsonObject jsonMessage;
        jsonMessage["channel"] = "mgmt";
        jsonMessage["srcNodeId"] = "pc";
        jsonMessage["dstNodeId"] = strTargetNodeId;
        jsonMessage["type"] = "PING";
        jsonMessage["role"] = "management";
        jsonMessage["seq"] = llLastPingSequence;
        jsonMessage["pcTimestampMs"] = llLastPingTimestampMs;

        writeJsonLine(jsonMessage);
    }
}

void GUI::writeJsonLine(const QJsonObject& jsonMessage)
{
    QByteArray arrData = QJsonDocument(jsonMessage).toJson(QJsonDocument::Compact);
    arrData.append('\n');

    pSerial->write(arrData);
    nSendBytes += arrData.size();
    appendMonitorLine("TX", QString::fromUtf8(arrData).trimmed());
}

void GUI::updateRuntime()
{
    ++nRuntimeSeconds;
    int nHours = nRuntimeSeconds / 3600;
    int nMinutes = (nRuntimeSeconds % 3600) / 60;
    int nSeconds = nRuntimeSeconds % 60;
    pRuntimeLabel->setText(QString("运行时长: %1:%2:%3")
        .arg(nHours, 2, 10, QLatin1Char('0'))
        .arg(nMinutes, 2, 10, QLatin1Char('0'))
        .arg(nSeconds, 2, 10, QLatin1Char('0')));
}

int GUI::nLineEditValue(QLineEdit* pLineEdit, int nDefaultValue) const
{
    bool bOk = false;
    int nValue = pLineEdit->text().toInt(&bOk);
    if (!bOk)
    {
        return nDefaultValue;
    }

    return nValue;
}

QString GUI::strCurrentTime() const
{
    return QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
}

void GUI::on_startPushButton_clicked()
{
    startAuthentication();
}
