#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GUI.h"

#include <QByteArray>
#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QProgressBar>
#include <QPushButton>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QSet>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVector>
#include <QWidget>

class CLineChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CLineChartWidget(QWidget* pParent = nullptr);

    void setSamples(const QVector<int>& vecSamples);

protected:
    void paintEvent(QPaintEvent* pEvent) override;

private:
    QVector<int> vecChartSamples;
};

class GUI : public QMainWindow
{
    Q_OBJECT

public:
    GUI(QWidget* pParent = nullptr);
    ~GUI();

private slots:
    void refreshSerialPorts();
    void connectSerialPort();
    void disconnectSerialPort();
    void saveConfig();
    void sendConfig();
    void startAuthentication();
    void stopAuthentication();
    void clearMonitor();
    void exportLog();
    void readSerialData();
    void updateRuntime();
    void sendPing();
    void on_startPushButton_clicked();

private:
    Ui::GUIClass      ui;
    QSerialPort*      pSerial = nullptr;
    QByteArray        arrSerialBuffer;
    QTimer*           pRuntimeTimer = nullptr;
    QTimer*           pPingTimer = nullptr;

    QComboBox*        pPortComboBox = nullptr;
    QComboBox*        pBaudComboBox = nullptr;
    QLabel*           pHeaderSerialLabel = nullptr;
    QLabel*           pHeaderStateLabel = nullptr;
    QLabel*           pReceiveBytesLabel = nullptr;
    QLabel*           pSendBytesLabel = nullptr;
    QLabel*           pRuntimeLabel = nullptr;
    QLabel*           pDelayValueLabel = nullptr;
    QLabel*           pLossValueLabel = nullptr;
    QLabel*           pSenderNodesLabel = nullptr;
    QLabel*           pReceiverNodesLabel = nullptr;

    QLineEdit*        pInitKeyLineEdit = nullptr;
    QLineEdit*        pKeyCountLineEdit = nullptr;
    QLineEdit*        pDelayLineEdit = nullptr;
    QLineEdit*        pIntervalLineEdit = nullptr;
    QLineEdit*        pGroupSizeLineEdit = nullptr;
    QLineEdit*        pDetectLineEdit = nullptr;
    QLineEdit*        pContextLineEdit = nullptr;
    QLineEdit*        pMessageLineEdit = nullptr;

    QTextEdit*        pMonitorTextEdit = nullptr;
    QCheckBox*        pAutoScrollCheckBox = nullptr;
    QCheckBox*        pShowTimeCheckBox = nullptr;
    QTableWidget*     pProcessTable = nullptr;
    QTableWidget*     pAuditTable = nullptr;
    CLineChartWidget* pLineChartWidget = nullptr;

    QLabel*           pTotalPacketsValueLabel = nullptr;
    QLabel*           pVerifiedValueLabel = nullptr;
    QLabel*           pFailedValueLabel = nullptr;
    QLabel*           pEfficiencyValueLabel = nullptr;
    QLabel*           pAverageCostValueLabel = nullptr;
    QLabel*           pReliabilityValueLabel = nullptr;

    int               nTotalPackets = 0;
    int               nVerifiedPackets = 0;
    int               nFailedPackets = 0;
    int               nAbnormalPackets = 0;
    int               nReceiveBytes = 0;
    int               nSendBytes = 0;
    int               nRuntimeSeconds = 0;
    int               nLinkDelayMs = -1;
    int               nLossPackets = 0;
    qint64            llPingSequence = 0;
    qint64            llLastPingSequence = 0;
    qint64            llLastPingTimestampMs = 0;
    QSet<QString>     setSenderNodeIds;
    QSet<QString>     setReceiverNodeIds;
    QMap<QString, int> mapSenderDelayMs;
    QMap<QString, int> mapReceiverDelayMs;
    bool              bConfigSent = false;
    bool              bConfigSaved = false;

    void buildInterface();
    void applyStyle();
    QWidget* pCreateHeader();
    QFrame* pCreateSectionFrame(const QString& strTitle);
    QWidget* pCreateCommunicationSection();
    QWidget* pCreateParameterSection();
    QWidget* pCreateProcessSection();
    QWidget* pCreateMonitorSection();
    QWidget* pCreateAnalysisSection();
    QWidget* pCreateLogSection();
    QWidget* pCreateMetricCard(const QString& strTitle, const QString& strValue, const QString& strColor);

    void setupProcessTable();
    void appendMonitorLine(const QString& strDirection, const QString& strText);
    void appendAuditRow(const QString& strType, const QString& strSenderId, const QString& strGroup, const QString& strResult, const QString& strDescription);
    void processSerialLine(const QByteArray& arrLine);
    void handleJsonMessage(const QJsonObject& jsonMessage);
    void handleLegacyText(const QString& strLine);
    void updateMetrics();
    void updateConnectedNode(const QJsonObject& jsonMessage);
    void refreshConnectedNodeLabels();
    QString strNodeText(const QSet<QString>& setNodeIds, const QMap<QString, int>& mapDelayMs, const QString& strEmptyText) const;
    void updateProcessRow(int nRow, const QString& strState, int nCurrentValue, int nTotalValue);
    void writeJsonLine(const QJsonObject& jsonMessage);
    QJsonObject jsonBuildConfig() const;
    int nLineEditValue(QLineEdit* pLineEdit, int nDefaultValue) const;
    QString strCurrentTime() const;
};
