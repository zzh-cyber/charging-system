#ifndef CHARGEPAGE_H
#define CHARGEPAGE_H

#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class QDoubleSpinBox;

class ChargePage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargePage(QWidget *parent = nullptr);

    // 预约成功后由外部把订单号交给充电页
    void setReservedOrder(const QString &orderNo);

    // start_charge 成功后调用
    void setChargingState();

    // settle 成功后调用
    void setSettledResult(double kwh, double amount);

    // 清空当前订单
    void reset();

signals:
    // 这里只发 Qt 信号，不创建新协议。
    // 后续由 MainWindow 使用现有 NetClient + start_charge 接口处理。
    void startChargeRequested(const QString &orderNo);

    // 后续使用现有 settle 接口处理
    void settleRequested(const QString &orderNo, double kwh);

private:
    enum class ChargeState
    {
        Empty,
        Reserved,
        Charging,
        Settled
    };

    void refreshUi();

    ChargeState m_state = ChargeState::Empty;

    QString m_orderNo;

    double m_settledKwh = 0.0;
    double m_settledAmount = 0.0;

    QLabel *m_stateTitle = nullptr;
    QLabel *m_orderLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_resultLabel = nullptr;
    QLabel *m_tipLabel = nullptr;

    QDoubleSpinBox *m_kwhSpin = nullptr;

    QPushButton *m_startButton = nullptr;
    QPushButton *m_settleButton = nullptr;
};

#endif // CHARGEPAGE_H
