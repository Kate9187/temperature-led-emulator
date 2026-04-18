#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QActionGroup>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_StartPB_clicked();

    void on_LedOnPB_clicked();

    void on_LedOffPB_clicked();

    void randTemp();

    void logStatus();

    void updateElapsedTimer();

    void ledController();

signals:
    void newTempSignal();

private:
    Ui::MainWindow *ui;
    QAction* actPage1;
    QAction* actPage2;
    QActionGroup* group;
    QTimer* tempTimer;
    QTimer* jsonTimer;
    QTimer* dispUpdateTimer;
    QElapsedTimer ledOnElapsedTimer;
    const int intervalTemp = 2000;
    const int intervalJSON = 5000;
    const double maxTempForLedOff = 30.0;
    double temp;
    qint64 elapsedMs;
    qint64 lastElapsedMs;
    enum mode{ON, OFF}ledMode;
    enum controlMode{AUTO, NOT_AUTO}controlLedMode;
    double minValTemp;
    double maxValTemp;
    void switchLED();
};
#endif // MAINWINDOW_H
