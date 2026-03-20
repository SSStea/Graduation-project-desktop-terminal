#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_GUI.h"


#include <QSerialPort>
#include <QSerialPortInfo>

class GUI : public QMainWindow
{
    Q_OBJECT

public:
    GUI(QWidget *parent = nullptr);
    ~GUI();

private:
    void on_startPushButton_clicked();   // 点击开始
    void readSerialData();                // 接收串口数据

private:
    Ui::GUIClass ui;

    QSerialPort* serial;

    void sendParameters();                // 发送参数
};

