#include "widget.h"
#include <QApplication>
#include <QMessageBox>
#include <QSharedMemory>
#include <QString>
#include <string.h>
int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    QSharedMemory shareM(Widget::shareKey); //确保单例
    if (shareM.attach()) {
        if (argc > 1) {
            QString path = qApp->arguments()[1];
            shareM.lock();
            //argv可能编码错误，在UTF-8编码里面存在一个特殊的字符，其编码是"0xC2 0xA0"，转换成字符的时候表现为一个半角空格，argv中直接变成'?'导致错误
            //比如这首歌 Ellivs - Start From Scratch.mp3 的最后两个空格 "D:\\CloudMusic\\Ellivs - Start?From?Scratch.mp3"
            //QApplication根本没用argv
            //参见QCoreApplication::arguments()：Otherwise, the arguments() are constructed from the return value of GetCommandLine().
            //综上，最好用qApp->arguments()代替argv，并采用toUtf8().data()保存
            memcpy(shareM.data(), path.toUtf8().data(), path.toUtf8().length() + 1);
            shareM.unlock();
        }
        return -1;
    }

    Widget w;
    w.show();
    return a.exec();
}
