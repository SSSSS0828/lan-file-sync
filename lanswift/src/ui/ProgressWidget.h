#pragma once
#include <QWidget>
#include <QProgressBar>
#include <QLabel>
#include <QVBoxLayout>

class ProgressWidget : public QWidget {
    Q_OBJECT
public:
    explicit ProgressWidget(QWidget* parent = nullptr);

public slots:
    void onProgress(qint64 transferred, qint64 total);
    void onSpeed(double bytesPerSec);
    void reset();

private:
    QProgressBar* bar_;
    QLabel*       labelBytes_;
    QLabel*       labelSpeed_;
    QLabel*       labelETA_;
    qint64        startTime_ = 0;
    qint64        total_     = 0;

    static QString fmtSize(qint64 bytes);
    static QString fmtSpeed(double bps);
    static QString fmtETA(qint64 remaining, double bps);
};
