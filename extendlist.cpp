#include "extendlist.h"
#include "setwindowblur.h"
#include "ui_extendlist.h"
#include <QDebug>
ExtendList::ExtendList(QWidget* parent)
    : QDialog(parent), ui(new Ui::ExtendList)
{
    ui->setupUi(this);
    setFixedSize(size());
    setWindowFlag(Qt::FramelessWindowHint); //与Qt::WA_NoSystemBackground一起用 否则Windows报错
    setAttribute(Qt::WA_NoSystemBackground); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    setAttribute(Qt::WA_TranslucentBackground); //加上这句使得客户区也生效Blur
    setWindowBlur(HWND(winId()));

    lw = ui->listWidget;
    lw->move(0, 0);
    lw->setFixedSize(size());

    connect(lw, &QListWidget::itemDoubleClicked, this, &ExtendList::selectItem);
    connect(lw, &QListWidget::itemDoubleClicked, [=](QListWidgetItem* item) {
        emit itemSelected(lw->row(item));
    });
}

ExtendList::~ExtendList()
{
    delete ui;
}

void ExtendList::setList(const QStringList& list)
{
    lw->clear();
    curItem = nullptr; //or 异常退出
    for (const QString& str : list) {
        QListWidgetItem* item = new QListWidgetItem(str, lw);
        item->setToolTip(str);
        lw->addItem(item);
    }
}

void ExtendList::selectItem(QListWidgetItem* item)
{
    if (curItem != nullptr) {
        curItem->setForeground(QColor(Qt::white));
        curItem->setIcon(QIcon());
    }
    item->setForeground(QColor(Qt::red));
    item->setIcon(QIcon(":/images/ICON_right.ico"));
    curItem = item;
}

void ExtendList::scrollTo(int row, QAbstractItemView::ScrollHint hint)
{
    if (!checkRange(row)) return;
    QListWidgetItem* item = lw->item(row);
    lw->scrollToItem(item, hint);
    selectItem(item);
}

void ExtendList::removeItem(int row)
{
    if (!checkRange(row)) return;
    lw->takeItem(row);
}

bool ExtendList::checkRange(int row)
{
    return row >= 0 && row < lw->count();
}
