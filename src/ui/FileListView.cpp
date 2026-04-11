#include "FileListView.h"
#include <QDateTime>

FileListView::FileListView(QWidget* parent) : QWidget(parent)
{
    list_ = new QListWidget(this);
    list_->setAlternatingRowColors(true);
    list_->setWordWrap(true);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel("Log:", this));
    lay->addWidget(list_);
}

void FileListView::addEntry(const QString& msg, bool isError)
{
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    auto* item = new QListWidgetItem("[" + ts + "] " + msg, list_);
    if (isError) {
        item->setForeground(Qt::red);
    }
    list_->scrollToBottom();
}

void FileListView::clear()
{
    list_->clear();
}
