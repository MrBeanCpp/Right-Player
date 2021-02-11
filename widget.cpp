#include "widget.h"
#include "setwindowblur.h"
#include "ui_widget.h"
#include <QDebug>
#include <QFileInfo>
#include <QGraphicsEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMediaMetaData>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QScreen>
#include <QSettings>
#include <QStyle>
#include <QTimer>
#include <QToolTip>
#include <QWinThumbnailToolButton>
#include <QtWin>
#include <string.h>
#include <windows.h>
const QString Widget::shareKey = "Light-Music-Player-MrBeanC-SK";
const QStringList Widget::audioFilter = { "*.mp3", "*.flac", "*.wav" };

Widget::Widget(QWidget* parent) //需要安装 LAV Filters 提供解码
    : QWidget(parent), ui(new Ui::Widget), player(new QMediaPlayer(this)), playList(new QMediaPlaylist(this)), exList(new ExtendList) //再加亿点点细节
{ //                                                                                                            showEvent()绑定parent
    ui->setupUi(this);
    setFixedSize(size());
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    //↓不采用setWindowFlags()，会覆盖其它Flag导致click任务栏图标无法最小化
    setWindowFlag(Qt::FramelessWindowHint); //与Qt::WA_NoSystemBackground一起用 否则Windows报错
    setAttribute(Qt::WA_NoSystemBackground); //!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    setAttribute(Qt::WA_TranslucentBackground); //加上这句使得客户区也生效Blur

    ui->btn_play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    setIcon(ui->btn_volume, "volume");
    setIcon(ui->btn_pin, "pin_off");
    setIcon(ui->btn_playL, "play");

    player->setPlaylist(playList);
    ui->slider_volume->setPageStep(2); //滚轮step
    ui->slider_volume->setVisible(false);
    setButtonsVisible(false);
    ui->label_bg->move(0, 0);
    ui->label_bg->setFixedSize(size()); //
    setSongTitle("[Loading...]");

    ui->label_image->setAttribute(Qt::WA_Hover); //better than mouseTracking 在于HoverEvent可以传递到父窗体
    ui->btn_playL->setParent(ui->label_image); //父子关系才能传递HoverEvent(作为一个整体Enter Leave)
    ui->btn_pre->setParent(ui->label_image); //此时坐标系将相对于label_image
    ui->btn_next->setParent(ui->label_image);
    ui->btn_playBack->setParent(ui->label_image);
    initPosition();

    connect(ui->btn_playBack, &QPushButton::clicked, [=]() { changePlayBackMode(); });

    connect(ui->btn_pre, &QPushButton::clicked, playList, &QMediaPlaylist::previous);
    connect(ui->btn_next, &QPushButton::clicked, playList, &QMediaPlaylist::next);

    connect(ui->btn_play, &QPushButton::toggled, [=](bool checked) {
        setIcon(ui->btn_playL, checked ? "pause" : "play");
    });
    connect(ui->btn_playL, &QPushButton::clicked, [=](bool checked) {
        setIcon(ui->btn_playL, checked ? "pause" : "play");
        ui->btn_play->click();
    });

    connect(ui->btn_play, &QPushButton::toggled, this, &Widget::setThumbPlayBtnChecked); //whenever the state changes(include code)
    connect(ui->btn_play, &QPushButton::clicked, [=](bool checked) { //only click(exclude code)
        if (checked) {
            player->play();
            ui->btn_play->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        } else {
            player->pause();
            ui->btn_play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        }
    });
    connect(ui->btn_pin, &QPushButton::clicked, [=](bool checked) { //前置
        SetWindowPos(HWND(winId()), checked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
        setIcon(ui->btn_pin, checked ? "pin_on" : "pin_off");
    });
    connect(ui->btn_volume, &QPushButton::clicked, [=](bool checked) {
        ui->slider_volume->setVisible(checked);
        if (checked) ui->slider_volume->setFocus();
    });
    connect(player, &QMediaPlayer::positionChanged, [=](qint64 ms) { //ms but interval = 1s which can be set by setNotifyInterval()
        if (!isHandlePress)
            ui->slider_time->setValue(ms / 1000);
    });
    connect(player, &QMediaPlayer::stateChanged, [=](QMediaPlayer::State state) {
        if (state == QMediaPlayer::StoppedState) { //直接click会导致player->pause()，setMedia出错
            qDebug() << "Stopped";
            ui->btn_play->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
            ui->btn_play->setChecked(false);
            emit player->positionChanged(0);

            if (playList->currentIndex() == -1) //处理sequencial最后一首
                playList->previous();
        }
    });
    connect(ui->slider_time, &QSlider::sliderPressed, [=]() { isHandlePress = true; }); //防止移动滑块时，自动改变位置
    connect(ui->slider_time, &QSlider::valueChanged, [=](int sec) { ui->label_time->setText(toMS(sec)); }); //实时调整time，方便调节进度条
    connect(ui->slider_time, &QSlider::sliderReleased, [=]() { //ms//不采用valueChanged，防止移动时实时改变音频时间导致卡顿(参考网易云)
        player->setPosition(ui->slider_time->value() * 1000);
        isHandlePress = false; //有些音频(如 HO-YA - The Ideal Shore.mp3)在setPosition后出现定位不准确问题(时长已满还在播放)(网易云也是如此但Groove没问题)
    }); //推测采用大时段定位
    connect(ui->slider_volume, &QSlider::valueChanged, [=](int value) {
        if (!value && player->volume())
            setIcon(ui->btn_volume, "mute"); //静音
        else if (value && !player->volume())
            setIcon(ui->btn_volume, "volume"); //非静音
        player->setVolume(value);
    });
    connect(player, &QMediaPlayer::durationChanged, [=](qint64 dur) { //在构造函数中获取dur可能更新不及时(0)
        int sec = dur / 1000;
        ui->slider_time->setRange(0, sec); //s
    });
    connect(ui->btn_min, &QPushButton::clicked, this, &Widget::showMinimized);
    connect(ui->btn_close, &QPushButton::clicked, this, &Widget::close);

    connect(player, QOverload<>::of(&QMediaObject::metaDataChanged), [=]() {
        if (!player->isMetaDataAvailable()) return; //会触发两次changed，一次unavailable
        updateMetaData();
    });
    connect(player, &QMediaPlayer::currentMediaChanged, [=](const QMediaContent& media) {
        QString path = getMediaPath(media);
        if (path.isEmpty()) return; //切换时有可能 ==""
        qDebug() << "current-Media-Changed";
        if (QFile::exists(path))
            setTitle(getFileName(path));
        else { //考虑被删除的特殊情况
            qDebug() << path;
            int index = playList->currentIndex();
            playList->removeMedia(index); //导致pre index变化
            exList->removeItem(index);
            if (playList->mediaCount() == 0) {
                setInactive();
                return;
            }
        }
        updatePreNextBtn(); //适时disable [pre &| next buttons]
        QTimer::singleShot(10, [=]() { exList->scrollTo(playList->currentIndex()); }); //selectItem//& 防止UI显示前卡顿
    });

    connect(ui->btn_extend, &QPushButton::clicked, [=](bool checked) {
        const int Gap = 5;
        exList->move(x() - exList->width() - Gap, (y() + height() / 2) - (exList->height() / 2));
        exList->setVisible(checked);
        if (checked) exList->showNormal(); //最小化也被认为visible f**k
    });
    connect(exList, &ExtendList::itemSelected, [=](int index) {
        playList->setCurrentIndex(index);
        play();
    });

    ui->slider_volume->installEventFilter(this);
    ui->slider_time->installEventFilter(this);
    ui->btn_volume->installEventFilter(this);
    ui->label_time->installEventFilter(this);
    ui->label_image->installEventFilter(this);
    this->installEventFilter(this); //qApp事件过滤可能造成重复执行，最好单独类

    setShadowEffect(ui->label_image, cover_shadowR); //多种effect不能叠加

    readIniFile(); //volume & playBackMode

    createSharedMemory();
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Widget::readSharedMemory);
    timer->start(100);

    QStringList args = qApp->arguments();
    if (args.size() > 1)
        setMedia(args[1]);
    else
        setInactive(); //setMedia("D:\\CloudMusic\\花譜 - そして花になる.mp3", false);
}

Widget::~Widget()
{
    delete ui;
}

QString Widget::toMS(qint64 sec)
{
    int minute = sec / 60;
    int second = sec % 60;
    return QString("%1:%2").arg(minute, 2, 10, QChar('0')).arg(second, 2, 10, QChar('0')); //前导零
}

QPixmap Widget::setPixmapOpacity(const QPixmap& pixmap, qreal alpha)
{
    if (pixmap.isNull()) return pixmap;

    QPixmap temp(pixmap.size());
    temp.fill(Qt::transparent);

    QPainter painter(&temp);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(temp.rect(), QColor(0, 0, 0, alpha * 255)); //透明度

    return temp;
}

QPixmap Widget::setPixmapBlur(const QPixmap& pixmap, int radius)
{
    if (pixmap.isNull()) return pixmap;

    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    QGraphicsBlurEffect* blur = new QGraphicsBlurEffect(this);
    blur->setBlurRadius(radius);
    item.setPixmap(pixmap);
    item.setGraphicsEffect(blur);
    scene.addItem(&item);
    QPixmap res(pixmap.size());
    res.fill(Qt::transparent);
    QPainter ptr(&res);
    scene.render(&ptr);
    return res;
}

void Widget::setAdjustText(QLabel* label, const QString& text, int rLimit, int bLimit, int initFont, bool autoWarp, int fontLimit)
{
    if (label == nullptr) return;

    label->setMinimumWidth(0);
    label->setWordWrap(false);
    label->setText(text);
    label->adjustSize();
    if (rLimit <= label->x()) return;

    QFont font(label->font());
    int size = initFont;
    do { //自适应字号
        font.setPointSize(size);
        label->setFont(font);
        label->adjustSize();
    } while ((label->geometry().right() > rLimit) && (!autoWarp || size-- > fontLimit));
    if (autoWarp && label->geometry().right() > rLimit) { //单行情况下，到了极限size，需要换行
        if (bLimit <= label->y()) return;
        label->setWordWrap(true);
        label->setMinimumWidth(rLimit - label->x()); //固定width
        label->adjustSize();

        QFontMetrics metrics(font);
        int maxLine = (bLimit - label->y()) / metrics.height();
        int vWidth = maxLine * label->width();
        do {
            label->setText(metrics.elidedText(text, Qt::ElideRight, vWidth)); //省略
            vWidth -= metrics.horizontalAdvance("符");
            label->adjustSize();
        } while (label->geometry().bottom() > bLimit); //确保下限
    }
}

void Widget::setMedia(QString path, bool autoPlay)
{
    if (player == nullptr) return;

    setActive();
    QStringList fileList = getFileList(path);
    int index = fileList.indexOf(path.replace('\\', '/'));
    setPlayList(fileList, index);

    if (autoPlay) play();
}

void Widget::setPlayList(const QStringList& fileList, int index)
{
    if (player == nullptr) return;

    if (fileList != getPlayerFileList()) { //相同目录不必重复加载，提升性能
        qDebug() << "Not Same fileList";
        playList->clear();
        for (const QString& file : fileList)
            playList->addMedia(QUrl::fromLocalFile(file));
        setExList(fileList);
    }
    playList->setCurrentIndex(index); //放在player->setPlaylist(playList);之前不生效(特别是file过多时，可能与加载缓慢有关)
} //若file すごい多ければ 可采用将当前歌曲设为第一首，先播放，其余(多线程)加载，然后movePos

bool Widget::createSharedMemory()
{
    const int SIZE = 256;
    shareM = new QSharedMemory(shareKey, this);
    bool flag = shareM->create(SIZE);
    if (flag) memset(shareM->data(), 0, SIZE);
    return flag;
}

void Widget::readSharedMemory()
{
    if (shareM == nullptr || *((char*)shareM->data()) == '\0') return;
    shareM->lock();
    QString data = QString::fromUtf8((char*)shareM->data());
    qDebug() << data;
    memset(shareM->data(), 0, 1); //flag for unavailable
    shareM->unlock();

    setMedia(data); //自带autoPlay
    if (!ui->btn_pin->isChecked()) setWindowTop(this);
    showNormal();
}

void Widget::setWindowTop(QWidget* widget) //for a while
{
    widget->showNormal();
    SetWindowPos(HWND(widget->winId()), HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(HWND(widget->winId()), HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
}

void Widget::setIcon(QAbstractButton* btn, const QString& iconName, const QString& suffix, const QString& dir)
{
    if (btn == nullptr) return;
    QString path = ":/" + dir + "/" + iconName + suffix;
    btn->setIcon(QIcon(path));
}

void Widget::setThumbnailPixmap(const QPixmap& pixmap) //废弃，いろんな原因，效果不达标(Groove也没实现~)
{
    if (thumbBar == nullptr) return;
    thumbBar->setIconicThumbnailPixmap(pixmap);
    //thumbBar->setIconicLivePreviewPixmap(qApp->primaryScreen()->grabWindow(this->winId()));
}

void Widget::createThumbnailBar()
{
    if (thumbBar != nullptr) return;
    thumbBar = new QWinThumbnailToolBar(this);
    thumbBar->setWindow(windowHandle()); //必须在窗口显示后(构造完成后) or Handle == 0

    QWinThumbnailToolButton* btn_play = new QWinThumbnailToolButton(thumbBar);
    QWinThumbnailToolButton* btn_pre = new QWinThumbnailToolButton(thumbBar);
    QWinThumbnailToolButton* btn_next = new QWinThumbnailToolButton(thumbBar);
    btn_play->setIcon(QIcon(":/images/play_w.png"));
    btn_pre->setIcon(QIcon(":/images/pre_w.png"));
    btn_next->setIcon(QIcon(":/images/next_w.png"));

    static bool bPlay = false;
    connect(btn_play, &QWinThumbnailToolButton::clicked, [=]() {
        setThumbPlayBtnChecked(!bPlay);
        ui->btn_play->click();
    });
    connect(this, &Widget::setThumbPlayBtnChecked, [=](bool checked) { //两个信号(1.实际触发 2.改变图标)防止循环触发
        bPlay = checked;
        btn_play->setIcon(bPlay ? QIcon(":/images/pause_w.png") : QIcon(":/images/play_w.png"));
    });

    connect(btn_pre, &QWinThumbnailToolButton::clicked, ui->btn_pre, &QPushButton::click);
    connect(btn_next, &QWinThumbnailToolButton::clicked, ui->btn_next, &QPushButton::click);

    thumbBar->addButton(btn_pre);
    thumbBar->addButton(btn_play);
    thumbBar->addButton(btn_next);

    updateThumbBtnEnable();
    setThumbPlayBtnChecked(ui->btn_play->isChecked()); //确认状态
}

void Widget::writeIniFile()
{
    QSettings iniSet(iniPath, QSettings::IniFormat);
    iniSet.setValue("volume", player->volume());
    iniSet.setValue("playBackMode", playList->playbackMode());
}

void Widget::readIniFile()
{
    QSettings iniSet(iniPath, QSettings::IniFormat);
    int volume = iniSet.value("volume", initVolume).toInt();
    ui->slider_volume->setValue(volume); //若initVolume与UI界面value一致则不会触发valueChanged
    player->setVolume(volume); //所以双层保险

    QMediaPlaylist::PlaybackMode playBackMode = iniSet.value("playBackMode", QMediaPlaylist::Sequential).value<QMediaPlaylist::PlaybackMode>();
    changePlayBackMode(false, playBackMode);
}

void Widget::setBlurEffect(QWidget* widget, int radius)
{
    QGraphicsBlurEffect* blur = new QGraphicsBlurEffect(this);
    blur->setBlurRadius(radius);
    widget->setGraphicsEffect(blur);
}

void Widget::setShadowEffect(QWidget* widget, int radius, QPointF offset, QColor color)
{
    QGraphicsDropShadowEffect* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setOffset(offset);
    shadow->setColor(color);
    shadow->setBlurRadius(radius);
    widget->setGraphicsEffect(shadow);
}

QStringList Widget::getFileList(const QString& path, const QStringList& suffixFilter, QDirIterator::IteratorFlags flags)
{
    QStringList fileList;
    if (path.isEmpty()) return fileList;
    QFileInfo fileInfo(path);
    QString dir = fileInfo.isDir() ? path : fileInfo.absoluteDir().absolutePath();
    qDebug() << "Dir: " << dir;

    QDirIterator iter(dir, suffixFilter, QDir::Files | QDir::NoSymLinks, flags);
    while (iter.hasNext()) {
        iter.next();
        fileList << iter.filePath(); //包含文件名的文件的全路径
        if (fileList.size() % 500 == 0) qApp->processEvents(); //响应界面
    }
    return fileList;
}

void Widget::setTitle(const QString& title, bool withWindowTitle)
{
    QFontMetrics metrics(ui->label_fileName->font());
    ui->label_fileName->setText(metrics.elidedText(title, Qt::ElideRight, ui->label_fileName->width())); //Title
    if (withWindowTitle) setWindowTitle(title);
}

void Widget::initPosition()
{
    auto setCenterX = [](QWidget* w, int x) {
        w->move(x - w->width() / 2, w->y());
    };
    auto setCenterY = [](QWidget* w, int y) {
        w->move(w->x(), y - w->height() / 2);
    };
    auto setCenter = [=](QWidget* w, int x, int y) { //对齐中心点
        setCenterX(w, x);
        setCenterY(w, y);
    };
    const int Margin = 22;
    QRect pPos = ui->label_image->geometry();
    setCenter(ui->btn_playL, pPos.width() / 2, pPos.height() / 2);
    setCenter(ui->btn_pre, Margin, pPos.height() / 2);
    setCenter(ui->btn_next, pPos.width() - Margin, pPos.height() / 2);
    setCenter(ui->btn_playBack, pPos.width() / 2, pPos.height() - Margin);
}

void Widget::setButtonsVisible(bool visible)
{
    ui->btn_playL->setVisible(visible);
    ui->btn_pre->setVisible(visible);
    ui->btn_next->setVisible(visible);
    ui->btn_playBack->setVisible(visible);
}

void Widget::changePlayBackMode(int next, QMediaPlaylist::PlaybackMode playBackMode)
{
    static int index = 0;
    static QMap<int, QMediaPlaylist::PlaybackMode> map = {
        { 0, QMediaPlaylist::Sequential },
        { 1, QMediaPlaylist::Loop },
        { 2, QMediaPlaylist::CurrentItemInLoop },
        { 3, QMediaPlaylist::Random }
    };
    if (next == false)
        index = map.key(playBackMode, QMediaPlaylist::Sequential);
    else
        index++;
    switch (index %= 4) {
    case 0:
        setIcon(ui->btn_playBack, "sequence", ".png");
        break;
    case 1:
        setIcon(ui->btn_playBack, "loop", ".png");
        break;
    case 2:
        setIcon(ui->btn_playBack, "singleLoop", ".png");
        break;
    case 3:
        setIcon(ui->btn_playBack, "random", ".png");
        break;
    }
    playList->setPlaybackMode(map.value(index));
    updatePreNextBtn();

    qDebug() << map.value(index);
}

int Widget::adjustTextColor(QLabel* label, std::function<int(int)> rule, QRect rect)
{
    QImage bg = ui->label_bg->pixmap()->toImage();
    if (rect == QRect()) rect = label->geometry();
    int Gray = avgGray(bg.copy(rect));

    const int l = 10, r = 220, best = 30, lGap = 40, rGap = 120;
    auto check = [=](int res) -> bool {
        return res >= l && res <= r;
    };
    auto make = [=](int res) -> int {
        return (res - l + r - l) % (r - l) + l;
    };

    int res = make(Gray - lGap);
    if (qAbs(Gray - best) >= lGap)
        res = best;
    else if (check(Gray - lGap))
        res = Gray - lGap;
    else if (check(Gray + rGap))
        res = Gray + rGap;

    res = (rule == nullptr ? res : rule(Gray));

    //    int nColor = label->palette().windowText().color().red();//Just for Function show(Show motion)!!!!!!!!!!!!!!!!!!!!!!!!!!
    //    bool plus = (res - nColor > 0);
    //    for (int i = nColor; plus ? (i <= res) : (i >= res); i += (plus ? 1 : -1)) {
    //        label->setStyleSheet(QString("color:rgb(%1, %1, %1)").arg(i));
    //        qApp->processEvents();
    //        Sleep(5);
    //    }//

    label->setStyleSheet(QString("color:rgb(%1, %1, %1)").arg(res));
    return res;
}

int Widget::avgGray(const QImage& image, int step) //Gray = R*0.299 + G*0.587 + B*0.114
{
    auto gray = [](int r, int g, int b, int a) -> int {
        return (r * 0.299 + g * 0.587 + b * 0.114) * a / 255;
    };

    qint64 Gray = 0;
    int w = image.width(), h = image.height(), cnt = 0;
    for (int i = 0; i < w; i += step)
        for (int j = 0; j < h; j += step) {
            QColor color = image.pixelColor(i, j);
            Gray += gray(color.red(), color.green(), color.blue(), color.alpha());
            cnt++;
        }
    return Gray / cnt;
}

void Widget::setExList(const QStringList& pathList)
{
    QStringList nameList;
    for (const QString& path : pathList)
        nameList << getFileName(path);
    exList->setList(nameList);
}

void Widget::play()
{
    if (!ui->btn_play->isChecked())
        ui->btn_play->click();
}

void Widget::updatePreNextBtn()
{
    int pos = playList->currentIndex();
    int pre = playList->previousIndex();
    int next = playList->nextIndex();
    bool deadLoop = (pre == pos && pos == next);
    qDebug() << pre << pos << next;

    ui->btn_pre->setDisabled(pre == -1 || deadLoop);
    ui->btn_next->setDisabled(next == -1 || deadLoop);

    updateThumbBtnEnable();

    while (playList->playbackMode() == QMediaPlaylist::Random && playList->mediaCount() > 1 && playList->nextIndex() == pos) { //重置random
        playList->setPlaybackMode(QMediaPlaylist::Sequential); //防止死循环↑
        playList->setPlaybackMode(QMediaPlaylist::Random);
        qDebug() << "random reset";
    }
}

void Widget::updateMetaData()
{
    QPixmap pixmap = QPixmap::fromImage(player->metaData(QMediaMetaData::ThumbnailImage).value<QImage>()); //Not "CoverArtImage" //QVariant属于QtCore不提供QGUI转换，使用value模板函数转换
    if (pixmap.isNull()) pixmap = defaultCover; //不存在，则用默认图片
    setCover(pixmap);
    setBackground(pixmap); //半透明 & 模糊 & 缩放 & 裁切

    if (!isMinimized()) update_Song_Singer_Album(); //防止最小化后闪现bug

    adjustTextsColor();
}

QString Widget::getFileName(const QString& path)
{
    return QFileInfo(path).completeBaseName();
}

void Widget::setCover(const QPixmap& pixmap)
{
    bool isCursorInImage = ui->label_image->geometry().contains(mapFromGlobal(QCursor::pos())); //也可以用btn->isVisible()但坐标更符合直接逻辑
    cover = pixmap;
    blurCover = setPixmapBlur(cover, cover_blurR);
    ui->label_image->setPixmap(isCursorInImage ? blurCover : cover);
}

void Widget::setBackground(const QPixmap& pixmap)
{
    QPixmap bg = setPixmapBlur(setPixmapOpacity(pixmap, 0.6), bg_blurR).scaledToWidth(this->width()).copy(this->rect());
    ui->label_bg->setPixmap(bg); //background-> 半透明 & 模糊 & 缩放 & 裁切
}

void Widget::setSongTitle(const QString& str, const QString& defaultStr)
{
    setAdjustText(ui->label_song, str.isEmpty() ? defaultStr : str, width() - 15, ui->label_singer->y(), 24, true, 14);
}

void Widget::setSinger(const QString& str, const QString& defaultStr)
{
    setAdjustText(ui->label_singer, str.isEmpty() ? defaultStr : str, width() - 15, 0, 12, false);
}

void Widget::setAlbum(const QString& str, const QString& defaultStr)
{
    setAdjustText(ui->label_album, "『" + (str.isEmpty() ? defaultStr : str) + "』", width() - 15, ui->slider_time->y(), 12);
}

void Widget::setInactive()
{
    setCover(defaultCover);
    setBackground(defaultCover);

    setTitle("Right Player - MrBeanC");
    setSongTitle("No Music");
    setSinger("音楽ありません");
    setAlbum("代码没有心");
    adjustTextsColor();

    clickToChecked(ui->btn_extend, false); //close exList
    clickToChecked(ui->btn_volume, false);

    ui->btn_play->setDisabled(true);
    ui->btn_playL->setDisabled(true);
    ui->btn_pre->setDisabled(true);
    ui->btn_next->setDisabled(true);
    ui->btn_playBack->setDisabled(true);
    ui->btn_extend->setDisabled(true);
    ui->slider_time->setDisabled(true);
    ui->btn_volume->setDisabled(true);

    ui->label_time->setText(toMS(0));
    updateThumbBtnEnable();
}

void Widget::setActive()
{
    ui->btn_play->setDisabled(false);
    ui->btn_playL->setDisabled(false);
    ui->btn_pre->setDisabled(false);
    ui->btn_next->setDisabled(false);
    ui->btn_playBack->setDisabled(false);
    ui->btn_extend->setDisabled(false);
    ui->slider_time->setDisabled(false);
    ui->btn_volume->setDisabled(false);

    updateThumbBtnEnable();
}

void Widget::adjustTextsColor()
{
    adjustTextColor(ui->label_fileName, [](int gray) { return gray < 70 ? 180 : 25; }); //标题单独计算

    int color = adjustTextColor(ui->label_song);
    setShadowEffect(ui->label_song, color > 70 ? 8 : 0); //亮色字体加上阴影(8px)

    QRect rect(ui->label_singer->geometry().topLeft(), QPoint(ui->label_singer->geometry().right(), ui->label_album->geometry().bottom())); //保证 [singer & album] color相同
    adjustTextColor(ui->label_singer, nullptr, rect);
    adjustTextColor(ui->label_album, nullptr, rect);
}

QStringList Widget::getPlayerFileList()
{
    if (playList == nullptr) return QStringList();

    QStringList fileList;
    int sum = playList->mediaCount();
    for (int i = 0; i < sum; i++)
        fileList << getMediaPath(playList->media(i));
    return fileList;
}

QString Widget::getMediaPath(const QMediaContent& media)
{
    return media.request().url().toLocalFile();
}

void Widget::clickToChecked(QAbstractButton* btn, bool checked)
{
    if (!btn->isCheckable()) return;
    if (btn->isChecked() != checked)
        btn->click();
}

void Widget::updateThumbBtnEnable()
{
    if (thumbBar != nullptr) {
        thumbBar->buttons().at(0)->setEnabled(ui->btn_pre->isEnabled()); //pre
        thumbBar->buttons().at(1)->setEnabled(ui->btn_play->isEnabled()); //play
        thumbBar->buttons().at(2)->setEnabled(ui->btn_next->isEnabled()); //next
    }
}

void Widget::update_Song_Singer_Album()
{
    QString title = player->metaData(QMediaMetaData::Title).toString();
    QString author = player->metaData(QMediaMetaData::Author).toStringList().join(',');
    QString album = player->metaData(QMediaMetaData::AlbumTitle).toString();

    setSongTitle(title, "无题");
    setSinger(author, "佚名");
    setAlbum(album, "分からない"); //adjustSize导致最小化时，仍旧渲染窗体//参见log
}

bool Widget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == ui->slider_time && event->type() == QEvent::MouseButtonRelease && !isHandlePress) { //鼠标定位进度条
        if (!ui->slider_time->isEnabled()) return false;
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
        qreal percent = (qreal)(mouseEvent->pos().x() - 4) / (ui->slider_time->width() - 8); //与qss padding-left(4px) padding-right(4px)一致
        ui->slider_time->setValue(percent * ui->slider_time->maximum());
        emit ui->slider_time->sliderReleased();
    } else if ((watched == ui->slider_time || watched == this) && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if ((keyEvent->key() == Qt::Key_Left || keyEvent->key() == Qt::Key_Right) && ui->slider_time->isEnabled()) { //<- ->控制进度条
            ui->slider_time->event(event);
            emit ui->slider_time->sliderReleased();
            return true;
        } else if (keyEvent->key() == Qt::Key_Space) { //Space控制Play/Pause
            ui->btn_play->click();
            return true;
        }
    } else if (watched == ui->btn_volume && event->type() == QEvent::Wheel && ui->slider_volume->isVisible()) { //(单方面)共享滚轮事件
        qApp->sendEvent(ui->slider_volume, event);
    } else if (watched == ui->label_time) { //手动控制ToolTip
        if (event->type() == QEvent::MouseButtonPress) {
            QPoint pos(QWidget::mapToGlobal(ui->label_time->pos()));
            QToolTip::showText(QPoint(pos.x() - 8, pos.y() - 40), toMS(player->duration() / 1000), this); //继承Parent Qss
            return true; //防止单击使ToolTip消失
        } else if (event->type() == QEvent::MouseButtonRelease)
            QToolTip::hideText();
    } else if (watched == ui->slider_volume && event->type() == QEvent::FocusOut && !ui->btn_volume->hasFocus()) { //失去焦点Volume_Slider隐藏(volume按钮特判 or 触发两次)
        ui->btn_volume->click();
    } else if (watched == ui->label_image) {
        if (event->type() == QEvent::HoverEnter) {
            ui->label_image->setPixmap(blurCover);
            setButtonsVisible(true);
        } else if (event->type() == QEvent::HoverLeave) {
            ui->label_image->setPixmap(cover);
            setButtonsVisible(false);
        }
    }
    return false;
}

void Widget::mousePressEvent(QMouseEvent* event)
{
    QPoint curPos = event->screenPos().toPoint();
    QRect rect = ui->captionBar->geometry();
    rect.setBottom(rect.bottom() + 15); //贴心拓展，防止手残
    if (rect.contains(mapFromGlobal(curPos))) {
        isMousePress = true;
        preMousePos = curPos;
    }
}

void Widget::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    isMousePress = false;
}

void Widget::mouseMoveEvent(QMouseEvent* event)
{
    if (isMousePress) {
        QPoint curPos = event->screenPos().toPoint(); //QCursor::pos()更新相对滞后，不准确
        QPoint delta = curPos - preMousePos;
        this->move(pos() + delta);
        if (exList->isVisible())
            exList->move(exList->pos() + delta);
        preMousePos = curPos;
    }
}

void Widget::closeEvent(QCloseEvent* event)
{
    Q_UNUSED(event);
    exList->close();
    hide(); //保证观感上快速响应
    player->stop();
    writeIniFile(); //关闭时保存(虽然异常关闭就無駄了)，节省资源(反正Volume也不是什么要紧事~)
}

void Widget::showEvent(QShowEvent* event)
{
    Q_UNUSED(event);
    update_Song_Singer_Album();
    qDebug() << "show";

    if (hasWindowBlur) return; //稍后启用磨砂效果，保证快速启动//once
    createThumbnailBar(); //必须在窗口显示后(构造完成后)
    setWindowBlur(HWND(winId()));
    exList->setParent(this, exList->windowFlags()); //保持原有flags并绑定parent 同步显示层级//必须在setWindowBlur后setParent否则blur失效
    hasWindowBlur = true;
}

void Widget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
        SwitchToThisWindow(HWND(winId()), true);
    } else
        event->ignore();
}

void Widget::dropEvent(QDropEvent* event)
{
    QString path = event->mimeData()->urls().at(0).toLocalFile();
    setMedia(path);
}
