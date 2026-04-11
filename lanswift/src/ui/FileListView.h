#pragma once
#include <QWidget>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>

class FileListView : public QWidget {
    Q_OBJECT
public:
    explicit FileListView(QWidget* parent = nullptr);

    void addEntry(const QString& msg, bool isError = false);
    void clear();

private:
    QListWidget* list_;
};
