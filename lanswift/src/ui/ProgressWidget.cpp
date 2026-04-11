#include "ProgressWidget.h"
#include <QDateTime>

ProgressWidget::ProgressWidget(QWidget* parent)
    : QWidget(parent)
{
    bar_ = new QProgressBar(this);
    bar_->setRange(0, 100);
    bar_->setValue(0);
    bar_->setTextVisible(true);
    bar_->setMinimumHeight(22);

    labelBytes_ = new QLabel("0 B / 0 B", this);
    labelSpeed_ = new QLabel("Speed: --", this);
    labelETA_   = new QLabel("ETA: --", this);

    auto* row = new QHBoxLayout;
    row->addWidget(labelBytes_);
    row->addStretch();
    row->addWidget(labelSpeed_);
    row->addStretch();
    row->addWidget(labelETA_);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(bar_);
    lay->addLayout(row);
}

void ProgressWidget::onProgress(qint64 transferred, qint64 total)
{
    if (total <= 0) return;
    if (startTime_ == 0)
        startTime_ = QDateTime::currentMSecsSinceEpoch();
    total_ = total;

    int pct = static_cast<int>(transferred * 100 / total);
    bar_->setValue(pct);
    labelBytes_->setText(fmtSize(transferred) + " / " + fmtSize(total));
}

void ProgressWidget::onSpeed(double bps)
{
    labelSpeed_->setText("Speed: " + fmtSpeed(bps));
    if (total_ > 0 && bps > 0) {
        qint64 transferred = bar_->value() * total_ / 100;
        qint64 remaining   = total_ - transferred;
        labelETA_->setText("ETA: " + fmtETA(remaining, bps));
    }
}

void ProgressWidget::reset()
{
    bar_->setValue(0);
    labelBytes_->setText("0 B / 0 B");
    labelSpeed_->setText("Speed: --");
    labelETA_->setText("ETA: --");
    startTime_ = 0;
    total_     = 0;
}

QString ProgressWidget::fmtSize(qint64 bytes)
{
    if (bytes < 1024)           return QString("%1 B").arg(bytes);
    if (bytes < 1024*1024)      return QString("%1 KB").arg(bytes/1024.0, 0,'f',1);
    if (bytes < 1024*1024*1024) return QString("%1 MB").arg(bytes/1024.0/1024,0,'f',2);
    return QString("%1 GB").arg(bytes/1024.0/1024/1024, 0,'f',3);
}

QString ProgressWidget::fmtSpeed(double bps)
{
    if (bps < 1024)           return QString("%1 B/s").arg(bps,0,'f',0);
    if (bps < 1024*1024)      return QString("%1 KB/s").arg(bps/1024,0,'f',1);
    if (bps < 1024*1024*1024) return QString("%1 MB/s").arg(bps/1024/1024,0,'f',2);
    return QString("%1 GB/s").arg(bps/1024/1024/1024, 0,'f',2);
}

QString ProgressWidget::fmtETA(qint64 remaining, double bps)
{
    if (bps <= 0) return "--";
    int secs = static_cast<int>(remaining / bps);
    if (secs < 60)   return QString("%1s").arg(secs);
    if (secs < 3600) return QString("%1m %2s").arg(secs/60).arg(secs%60);
    return QString("%1h %2m").arg(secs/3600).arg((secs%3600)/60);
}
