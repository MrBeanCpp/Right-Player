#ifndef WIDGET_H
#define WIDGET_H

#include "extendlist.h"
#include <QAbstractButton>
#include <QApplication>
#include <QDirIterator>
#include <QLabel>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QSharedMemory>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QWinThumbnailToolBar>
QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

private:
    Ui::Widget* ui;

    QMediaPlayer* player = nullptr;
    bool isHandlePress = false;
    bool isMousePress = false;
    QPoint preMousePos;
    QSharedMemory* shareM = nullptr;
    bool hasWindowBlur = false;
    QWinThumbnailToolBar* thumbBar = nullptr;
    const QString iniPath = QApplication::applicationDirPath() + "/settings.ini";
    QPixmap blurCover;
    QPixmap cover;
    QMediaPlaylist* playList = nullptr;
    ExtendList* exList = nullptr;

    const int initVolume = 25;
    const int bg_blurR = 32;
    const int cover_blurR = 20;
    const int cover_shadowR = 8;
    const QPixmap defaultCover = QPixmap(":/images/default_cover.jpg");

public:
    static const QString shareKey;
    static const QStringList audioFilter;

private:
    QString toMS(qint64 sec); //format to mm:ss
    QPixmap setPixmapOpacity(const QPixmap& pixmap, qreal alpha); //透明度
    QPixmap setPixmapBlur(const QPixmap& pixmap, int radius); //模糊
    void setAdjustText(QLabel* label, const QString& text, int rLimit, int bLimit, int initFont, bool autoWarp = true, int fontLimit = 12); //自适应
    void setMedia(QString path, bool autoPlay = true);
    void setPlayList(const QStringList& fileList, int index);
    bool createSharedMemory(void); //for sharing Music path
    void readSharedMemory(void);
    void setWindowTop(QWidget* widget); //for a while
    void setIcon(QAbstractButton* btn, const QString& iconName, const QString& suffix = ".ico", const QString& dir = "images"); //default for inner resource
    void setThumbnailPixmap(const QPixmap& pixmap); //obsoleted ×
    void createThumbnailBar(void); //任务栏缩略图栏
    void writeIniFile(void); //写配置文件
    void readIniFile(void); //读↑
    void setBlurEffect(QWidget* widget, int radius);
    void setShadowEffect(QWidget* widget, int radius, QPointF offset = QPointF(0, 0), QColor color = Qt::black);
    QStringList getFileList(const QString& path, const QStringList& suffixFilter = audioFilter, QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags);
    void setTitle(const QString& title, bool withWindowTitle = true);
    void initPosition(void);
    void setButtonsVisible(bool visible); //pre play next(封面(label_image)下的按钮组)
    void changePlayBackMode(int next = true, QMediaPlaylist::PlaybackMode playBackMode = QMediaPlaylist::Sequential); //播放模式
    int adjustTextColor(QLabel* label, std::function<int(int)> rule = nullptr, QRect rect = QRect()); //自适应color
    int avgGray(const QImage& image, int step = 5); //平均灰度
    void setExList(const QStringList& pathList); //设置外挂列表 not that "外挂"
    void play(void);
    void updatePreNextBtn(void); //更新上下首可用性
    void updateMetaData(void); //更新封面、歌名等
    QString getFileName(const QString& path);
    void setCover(const QPixmap& pixmap);
    void setBackground(const QPixmap& pixmap);
    void setSongTitle(const QString& str, const QString& defaultStr = "Title");
    void setSinger(const QString& str, const QString& defaultStr = "Singer");
    void setAlbum(const QString& str, const QString& defaultStr = "Album");
    void setInactive(void);
    void setActive(void);
    void adjustTextsColor(void); //封装所有TextColor
    QStringList getPlayerFileList(void); //获取playList实时path列表(可能变化)
    QString getMediaPath(const QMediaContent& media);
    void clickToChecked(QAbstractButton* btn, bool checked);
    void updateThumbBtnEnable(void);
    void update_Song_Singer_Album(void);

signals:
    void setThumbPlayBtnChecked(bool checked); //play btn

public:
    bool eventFilter(QObject* watched, QEvent* event) override;

    // QWidget interface
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    // QWidget interface
protected:
    void closeEvent(QCloseEvent* event) override;
    void showEvent(QShowEvent* event) override;

    // QWidget interface
protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
};
#endif // WIDGET_H
