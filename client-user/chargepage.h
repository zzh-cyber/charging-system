#ifndef CHARGEPAGE_H
#define CHARGEPAGE_H

#include <QString>
#include <QWidget>
#include <QDateTime>

class QLabel;
class QPushButton;

class QResizeEvent;
class QTimer;
class QFrame;



class ChargePage : public QWidget
{
    Q_OBJECT

public:
    explicit ChargePage(QWidget *parent = nullptr);

    // 预约成功后由外部把订单号交给充电页
    void setReservedOrder(const QString &orderNo);

// start_charge 成功后调用
// powerKw / unitPrice 暂时允许为 0，兼容当前服务端
void setChargingState(
    const QString &startTime = QString(),
    double powerKw = 0.0,
    double unitPrice = 0.0);

    // finish_charge 成功：充电结束，进入待支付
void setPendingPaymentResult(
    qint64 durationSeconds,
    double kwh,
    double amount);

// pay_charge 成功：支付完成
void setPaidResult(
    qint64 durationSeconds,
    double kwh,
    double amount,
    double balance);


    // 清空当前订单
    void reset();

protected:
    void resizeEvent(
        QResizeEvent *event) override;

signals:
    // 这里只发 Qt 信号，不创建新协议。
    // 后续由 MainWindow 使用现有 NetClient + start_charge 接口处理。
    void startChargeRequested(const QString &orderNo);

    // 用户点击“结束充电”
void finishChargeRequested(
    const QString &orderNo);

// 用户点击“确认支付”
void payChargeRequested(
    const QString &orderNo);


private:
    void applyResponsiveStyle();
        // 充电中的实时记录
    void startChargeTimer();
    void stopChargeTimer();
    void updateChargingInfo();

    enum class ChargeState
{
    Empty,
    Reserved,
    Charging,
    PendingPayment,
    Paid
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
        // 充电实时记录卡
    QFrame *m_chargingRecordCard = nullptr;

    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_powerLabel = nullptr;
    QLabel *m_currentKwhLabel = nullptr;
    QLabel *m_estimatedFeeLabel = nullptr;

    QPushButton *m_startButton = nullptr;
    QPushButton *m_settleButton = nullptr;
    QPushButton *m_payButton = nullptr;

        // 每秒刷新充电数据
    QTimer *m_chargeTimer = nullptr;

    // 服务端 start_charge 返回的实际开始时间
    QDateTime m_chargeStartedAt;

    // 服务端返回的电桩功率和订单单价
    double m_powerKw = 0.0;
    double m_unitPrice = 0.0;

    // 当前实时预估值
    double m_currentKwh = 0.0;
    double m_estimatedAmount = 0.0;
    qint64 m_finalDurationSeconds = 0;
double m_finalBalance = 0.0;


};

#endif // CHARGEPAGE_H
