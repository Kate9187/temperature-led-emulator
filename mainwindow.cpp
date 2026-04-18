#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTimer>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->toolBar->setMovable(false); //отключена возможность перемещения

    // Создание действий
    actPage1 = new QAction("Main", this);
    actPage2 = new QAction("Settings", this);
    actPage1->setCheckable(true);
    actPage2->setCheckable(true);

    // Группа для взаимного исключения
    group = new QActionGroup(this);
    group->addAction(actPage1);
    group->addAction(actPage2);
    group->setExclusive(true);
    actPage1->setChecked(true);

    // Добавление действий в тулбар
    ui->toolBar->addAction(actPage1);
    ui->toolBar->addAction(actPage2);

    // Стартовая страница
    ui->stackedWidget->setCurrentIndex(0);

    // Связь кликов с переключением страниц
    connect(actPage1, &QAction::triggered, this, [this](){ui->stackedWidget->setCurrentIndex(0); });
    connect(actPage2, &QAction::triggered, this, [this](){ui->stackedWidget->setCurrentIndex(1); });

    // Диапазон температуры
    minValTemp = 20.0;
    maxValTemp = 35.0;

    // Настройка элементов выбора диапазона температуры
    ui->minSpBox->setRange(-50.0, 100.0); // Общий диапазон для выбора
    ui->minSpBox->setSingleStep(0.1);     // Шаг изменения
    ui->minSpBox->setDecimals(1);         // Один знак после запятой
    ui->minSpBox->setValue(minValTemp);
    ui->maxSpBox->setMinimum(minValTemp);

    ui->maxSpBox->setRange(-50.0, 100.0);
    ui->maxSpBox->setSingleStep(0.1);
    ui->maxSpBox->setDecimals(1);
    ui->maxSpBox->setValue(maxValTemp);
    ui->minSpBox->setMaximum(maxValTemp);

    // Изменение диапазонов температуры
    connect(ui->minSpBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value){
        ui->maxSpBox->setMinimum(value);
        minValTemp = value;
    });

    connect(ui->maxSpBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value){
        ui->minSpBox->setMaximum(value);
        maxValTemp = value;
    });

    // Таймер генерации температуры
    tempTimer = new QTimer(this);
    connect(tempTimer, &QTimer::timeout, this, &MainWindow::randTemp);

    // Обработка новых значений температуры
    connect(this, &MainWindow::newTempSignal, this, &MainWindow::ledController);

    // Таймер отправки JSON-пакетов
    jsonTimer = new QTimer(this);
    connect(jsonTimer, &QTimer::timeout, this, &MainWindow::logStatus);

    // Настройки стиля LCD-дисплея
    ui->lcdNumLed->setStyleSheet("QLCDNumber { background-color: #ffffff; border: 0.5px solid #CBCBCB;}");
    ui->lcdNumLed->setDigitCount(8);

    // Таймер обновления времени на LCD-дисплее
    dispUpdateTimer = new QTimer(this);
    connect(dispUpdateTimer, &QTimer::timeout, this, &MainWindow::updateElapsedTimer);

    // Настройка стиля светодиода
    ui->ledLabel->setFixedSize(40, 40);
    ui->ledLabel->setStyleSheet("background-color: #ff0000; border-radius: 20px;");

    // Стартовые настройки ярлыков
    ui->sensorValueLabel->setText("");
    ui->ledLabel->setText("");

    ui->sensorValueLabel->setStyleSheet("background-color: #ffffff; padding-right: 5px; border: 0.5px solid #CBCBCB;");
    ui->sensorValueLabel->setAlignment(Qt::AlignBottom | Qt::AlignRight);

    // Стартовые настройки кнопок
    ui->LedOnPB->setCheckable(true);
    ui->LedOffPB->setCheckable(true);
    ui->StartPB->setCheckable(true);

    ui->LedOnPB->setEnabled(false);
    ui->LedOffPB->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_StartPB_clicked()
{
    if(ui->StartPB->isChecked()){ //при однократном нажатии на кнопку start запуск приложения
        controlLedMode = AUTO; //включение светодиода зависит от температуры
        elapsedMs = 0;
        lastElapsedMs = 0;
        temp = 0;

        randTemp();
        logStatus();
        tempTimer->start(intervalTemp);
        jsonTimer->start(intervalJSON);

        ui->LedOnPB->setEnabled(true);
        ui->LedOffPB->setEnabled(true);
    }
    else{ //при повторном нажатии на кнопку start остановка работы приложения
        jsonTimer->stop();
        tempTimer->stop();

        ledMode = OFF;
        switchLED();

        ui->LedOnPB->setEnabled(false);
        ui->LedOffPB->setEnabled(false);
        ui->LedOnPB->setChecked(false);
        ui->LedOffPB->setChecked(false);

        ui->lcdNumLed->display(0);
        ui->sensorValueLabel->clear();
    }
}

void MainWindow::on_LedOnPB_clicked()
{
    if(ui->LedOnPB->isChecked()){
        controlLedMode = NOT_AUTO; // включение светодиода независимо от температуры
        if(ledMode != ON)
        {
            ledMode = ON;
            switchLED();
            if(ui->LedOffPB->isChecked())ui->LedOffPB->setChecked(false);
        }
    }
    else{
        controlLedMode = AUTO;
    }
}

void MainWindow::on_LedOffPB_clicked()
{
    if(ui->LedOffPB->isChecked()){
        controlLedMode = NOT_AUTO; // выключение светодиода независимо от температуры
        if(ledMode != OFF)
        {
            ledMode = OFF;
            switchLED();
            if(ui->LedOnPB->isChecked())ui->LedOnPB->setChecked(false);
        }
    }
    else{
        controlLedMode = AUTO;
    }
}

// Генерация температуры в заданном диапазоне
void MainWindow::randTemp()
{
    int min = static_cast<int>(minValTemp * 10);
    int max = static_cast<int>(maxValTemp * 10);
    if (min > max) return;
    int tmpTemp = QRandomGenerator::global()->bounded(min, max + 1); // [min10, max10]
    temp = tmpTemp / 10.0;

    ui->sensorValueLabel->setText(QString::number(temp, 'f', 1));
    emit newTempSignal();
}

// Отправка JSON-пакета на консоль
void MainWindow::logStatus()
{
    QJsonObject jsonObject;
    jsonObject["temp"] = temp;
    jsonObject["led"] = ledMode == 0? "ON" : "OFF" ;

    QJsonDocument jsonDoc(jsonObject);

    qInfo().noquote() << QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));
}

// Обновление времени на LCD-дисплее
void MainWindow::updateElapsedTimer()
{
    if(!ledOnElapsedTimer.isValid()) return; //проверка работы таймера
    qint64 current = ledOnElapsedTimer.elapsed(); // время, прошедшее с момента запуска таймера
    elapsedMs += (current - lastElapsedMs); //к общей сумме добавляется приращение от с прошлой проверки
    lastElapsedMs = current;
    qint64 seconds = elapsedMs / 1000;
    qint64 milliseconds = elapsedMs % 1000;
    ui->lcdNumLed->display(QString("%1.%2").arg(seconds, 2, 10, QChar('0')).arg(milliseconds, 3, 10, QChar('0')));
}

// Контроллер для проверки температуры и переключения состояния светодиода
void MainWindow::ledController()
{
    if(controlLedMode == NOT_AUTO) return;

    if(temp > maxTempForLedOff){
        if(ledMode != ON)
        {
            ledMode = ON;
            switchLED();
        }
    }
    else{
        if(ledMode != OFF)
        {
            ledMode = OFF;
            switchLED();
        }
    }
}

// Устанавливает режимы светодиода ON/OFF
void MainWindow::switchLED()
{
    switch(ledMode){
    case ON:
        ui->ledLabel->setStyleSheet("background-color: #00ff00; border-radius: 20px;");
        ledOnElapsedTimer.start();
        dispUpdateTimer->start(100); //запуск с интервалом 100 мс
        break;
    case OFF:
        ui->ledLabel->setStyleSheet("background-color: #ff0000; border-radius: 20px;");
        ledOnElapsedTimer.invalidate();
        dispUpdateTimer->stop();
        break;
    }
}

