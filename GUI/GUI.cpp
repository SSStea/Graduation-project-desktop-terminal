#include "GUI.h"
#include <QJsonObject>
#include <QJsonDocument>

GUI::GUI(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);
    serial = new QSerialPort(this);

    serial->setPortName("COM3");        // 修改为你的串口
    serial->setBaudRate(57600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (serial->open(QIODevice::ReadWrite))
    {
        ui.processTextEdit->append("serial port open success");
    }
    else
    {
        ui.processTextEdit->append("serial port open fail");
    }

    connect(serial, &QSerialPort::readyRead, this, &GUI::readSerialData);
}

GUI::~GUI()
{}

void GUI::on_startPushButton_clicked()
{
    sendParameters();
}

void GUI::sendParameters()
{
    QJsonObject obj;

    obj["keyInterval"] = ui.keyDelayLineEdit->text();
    obj["keyCount"] = ui.keyTotalLineEdit->text();
    obj["initKey"] = ui.keyInitLineEdit->text();
    obj["groupSize"] = ui.groupSizeLineEdit->text();
    obj["detectCount"] = ui.dectectingLineEdit->text();
    obj["sendMsg"] = ui.sendMessageLineEdit->text();
    obj["context"] = ui.contextLineEdit->text();

    QJsonDocument doc(obj);

    QByteArray data = doc.toJson(QJsonDocument::Compact);

    data.append("\n");

    serial->write(data);

    ui.processTextEdit->append("发送参数:");
    ui.processTextEdit->append(data);
}

void GUI::readSerialData()
{
    QByteArray data = serial->readAll();

    QString msg = QString::fromUtf8(data);

    ui.processTextEdit->append("收到:");
    ui.processTextEdit->append(msg);

    if (msg.contains("RESULT"))
    {
        ui.processTextEdit->append(msg);
    }
}