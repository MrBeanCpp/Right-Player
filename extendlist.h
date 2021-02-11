#ifndef EXTENDLIST_H
#define EXTENDLIST_H

#include <QDialog>
#include <QListWidget>
#include <QStringList>
namespace Ui {
class ExtendList;
}

class ExtendList : public QDialog {
    Q_OBJECT

public:
    explicit ExtendList(QWidget* parent = nullptr);
    ~ExtendList();

    void setList(const QStringList& list);
    void selectItem(QListWidgetItem* item);
    void scrollTo(int row, QAbstractItemView::ScrollHint hint = QAbstractItemView::EnsureVisible);
    void removeItem(int row);
    bool checkRange(int row);

private:
    Ui::ExtendList *ui;
    QListWidget* lw = nullptr;
    QListWidgetItem* curItem = nullptr;

signals:
    void itemSelected(int row);
};

#endif // EXTENDLIST_H
