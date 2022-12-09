/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     xushitong<xushitong@uniontech.com>
 *
 * Maintainer: max-lv<lvwujun@uniontech.com>
 *             lanxuesong<lanxuesong@uniontech.com>
 *             zhangsheng<zhangsheng@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "sharecontrolwidget.h"
#include "utils/usersharehelper.h"

#include "dfm-base/base/schemefactory.h"
#include "dfm-base/utils/dialogmanager.h"
#include "dfm-base/utils/decorator/decoratorfile.h"
#include "dfm-base/utils/universalutils.h"
#include "dfm-base/file/local/localfilewatcher.h"
#include "dfm-base/widgets/dfmwindow/filemanagerwindowsmanager.h"

#include <dfm-framework/dpf.h>

#include <DGuiApplicationHelper>

#include <QCheckBox>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include <QIcon>
#include <QStandardPaths>
#include <QApplication>
#include <QClipboard>
#include <QPushButton>
#include <QNetworkInterface>
#include <QTextBrowser>

#include <unistd.h>
#include <pwd.h>

using namespace dfmplugin_dirshare;
DWIDGET_USE_NAMESPACE
DFMBASE_USE_NAMESPACE

namespace ConstDef {
static const int kWidgetFixedWidth { 196 };
static constexpr char kShareNameRegx[] { "^[^\\[\\]\"'/\\\\:|<>+=;,?*\r\n\t]*$" };
static constexpr char kShareFileDir[] { "/var/lib/samba/usershares" };
}

class SectionKeyLabel : public QLabel
{
    Q_OBJECT
public:
    explicit SectionKeyLabel(const QString &text = "", QWidget *parent = nullptr, Qt::WindowFlags f = {});
};

SectionKeyLabel::SectionKeyLabel(const QString &text, QWidget *parent, Qt::WindowFlags f)
    : QLabel(text, parent, f)
{
    setObjectName("SectionKeyLabel");
    setFixedWidth(112);
    QFont font = this->font();
    font.setWeight(QFont::Bold - 8);
    font.setPixelSize(13);
    setFont(font);
    setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
}

ShareControlWidget::ShareControlWidget(const QUrl &url, QWidget *parent)
    : DArrowLineDrawer(parent), url(url)
{
    setupUi();
    init();
    initConnection();
}

ShareControlWidget::~ShareControlWidget()
{
    dpfSignalDispatcher->unsubscribe("dfmplugin_dirshare", "signal_Share_ShareAdded", this, &ShareControlWidget::updateWidgetStatus);
    dpfSignalDispatcher->unsubscribe("dfmplugin_dirshare", "signal_Share_ShareRemoved", this, &ShareControlWidget::updateWidgetStatus);
    dpfSignalDispatcher->unsubscribe("dfmplugin_dirshare", "signal_Share_RemoveShareFailed", this, &ShareControlWidget::updateWidgetStatus);

    if (refreshIp) {
        refreshIp->stop();
        refreshIp->deleteLater();
        refreshIp = nullptr;
    }
}

void ShareControlWidget::setOption(QWidget *w, const QVariantHash &option)
{
    if (option.contains("Option_Key_ExtendViewExpand")) {
        ShareControlWidget *view = dynamic_cast<ShareControlWidget *>(w);
        if (view)
            view->setExpand(option.value("Option_Key_ExtendViewExpand").toBool());
    }
}

void ShareControlWidget::setupUi()
{
    const QString &userName = getpwuid(getuid())->pw_name;
    isSharePasswordSet = UserShareHelperInstance->isUserSharePasswordSet(userName);
    setTitle(tr("Sharing"));
    setExpandedSeparatorVisible(false);
    setSeparatorVisible(false);

    QFrame *frame = new QFrame(this);
    QGridLayout *gridLayout = new QGridLayout(frame);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    setContent(frame);
    mainLay = new QFormLayout(this);
    mainLay->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    mainLay->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    mainLay->setFormAlignment(Qt::AlignVCenter | Qt::AlignCenter);
    mainLay->setContentsMargins(10, 10, 10, 10);
    mainLay->setVerticalSpacing(6);
    gridLayout->addLayout(mainLay, 0, 0);
    gridLayout->setVerticalSpacing(0);
    frame->setLayout(gridLayout);

    shareSwitcher = new QCheckBox(tr("Share this folder"), this);
    QWidget *switcherContainer = new QWidget(this);
    QHBoxLayout *lay = new QHBoxLayout(this);
    switcherContainer->setLayout(lay);
    lay->addWidget(shareSwitcher);
    lay->setAlignment(Qt::AlignCenter);
    lay->setContentsMargins(0, 0, 0, 0);
    mainLay->addRow(switcherContainer);

    QPalette peMenuBg;
    QColor color = palette().color(QPalette::ColorGroup::Active, QPalette::ColorRole::Window);
    peMenuBg.setColor(QPalette::Window, color);

    shareNameEditor = new QLineEdit(this);
    shareNameEditor->setFixedWidth(ConstDef::kWidgetFixedWidth);
    mainLay->addRow(new SectionKeyLabel(tr("Share name"), this), shareNameEditor);
    sharePermissionSelector = new QComboBox(this);
    sharePermissionSelector->setPalette(peMenuBg);
    sharePermissionSelector->setFixedWidth(ConstDef::kWidgetFixedWidth);
    mainLay->addRow(new SectionKeyLabel(tr("Permission"), this), sharePermissionSelector);
    shareAnonymousSelector = new QComboBox(this);
    shareAnonymousSelector->setPalette(peMenuBg);
    shareAnonymousSelector->setFixedWidth(ConstDef::kWidgetFixedWidth);
    mainLay->addRow(new SectionKeyLabel(tr("Anonymous"), this), shareAnonymousSelector);

    QValidator *validator = new QRegularExpressionValidator(QRegularExpression(ConstDef::kShareNameRegx), this);
    shareNameEditor->setValidator(validator);
    QStringList permissions { tr("Read and write"), tr("Read only") };
    sharePermissionSelector->addItems(permissions);
    QStringList anonymousSelections { tr("Not allow"), tr("Allow") };
    shareAnonymousSelector->addItems(anonymousSelections);

    // More share info
    setupNetworkPath();
    setupUserName();
    setupSharePassword();
    setupShareNotes(gridLayout);

    timer = new QTimer(this);
    timer->setInterval(500);
}

void ShareControlWidget::setupNetworkPath()
{
    netScheme = new QLabel("smb://", this);
    networkAddrLabel = new QLabel("127.0.0.1", this);
    networkAddrLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QHBoxLayout *hBoxLine1 = new QHBoxLayout(this);
    hBoxLine1->addWidget(netScheme);
    hBoxLine1->addWidget(networkAddrLabel);
    hBoxLine1->setContentsMargins(0, 0, 2, 0);
    networkAddrLabel->setFixedWidth(ConstDef::kWidgetFixedWidth);
    mainLay->addRow(new SectionKeyLabel(tr("Network path"), this), hBoxLine1);
    if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType)
        copyNetAddr = new QPushButton(QIcon(":light/icons/property_bt_copy.svg"), "");
    else if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType)
        copyNetAddr = new QPushButton(QIcon(":dark/icons/property_bt_copy.svg"), "");
    else
        copyNetAddr = new QPushButton(QIcon(":light/icons/property_bt_copy.svg"), "");

    copyNetAddr->setFlat(true);
    copyNetAddr->setToolTip(tr("Copy"));
    QObject::connect(copyNetAddr, &QPushButton::clicked, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(netScheme->text() + networkAddrLabel->text());
    });
    hBoxLine1->addWidget(copyNetAddr);
}

void ShareControlWidget::setupUserName()
{
    userNamelineLabel = new QLabel(this);
    userNamelineLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    userNamelineLabel->setText(getpwuid(getuid())->pw_name);
    userNamelineLabel->setFixedWidth(ConstDef::kWidgetFixedWidth);
    QHBoxLayout *hBoxLine2 = new QHBoxLayout(this);
    hBoxLine2->addWidget(userNamelineLabel);
    hBoxLine2->setContentsMargins(0, 0, 2, 0);
    mainLay->addRow(new SectionKeyLabel(tr("Username"), this), hBoxLine2);

    if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::LightType)
        copyUserNameBt = new QPushButton(QIcon(":light/icons/property_bt_copy.svg"), "");
    else if (DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType)
        copyUserNameBt = new QPushButton(QIcon(":dark/icons/property_bt_copy.svg"), "");
    else
        copyUserNameBt = new QPushButton(QIcon(":light/icons/property_bt_copy.svg"), "");

    copyUserNameBt->setFlat(true);
    copyUserNameBt->setToolTip(tr("Copy"));
    QObject::connect(copyUserNameBt, &QPushButton::clicked, [=]() {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(userNamelineLabel->text());
    });
    hBoxLine2->addWidget(copyUserNameBt);
}

void ShareControlWidget::setupSharePassword()
{
    sharePasswordlineEditor = new QLineEdit(this);
    sharePasswordlineEditor->setStyleSheet("QLineEdit{background-color:rgba(0,0,0,0)}");   // just clear the background
    QFont font = this->font();
    int defaultFontSize = font.pointSize();
    font.setPointSize(isSharePasswordSet ? 4 : defaultFontSize);
    sharePasswordlineEditor->setFont(font);
    sharePasswordlineEditor->setEchoMode(isSharePasswordSet ? QLineEdit::Password : QLineEdit::Normal);
    sharePasswordlineEditor->setAlignment(Qt::AlignJustify | Qt::AlignLeft);
    sharePasswordlineEditor->setText(isSharePasswordSet ? "-----" : tr("None"));
    sharePasswordlineEditor->setDisabled(true);
    sharePasswordlineEditor->setCursorPosition(0);
    QHBoxLayout *hBoxLine3 = new QHBoxLayout(this);
    hBoxLine3->addWidget(sharePasswordlineEditor);
    hBoxLine3->setContentsMargins(0, 0, 0, 0);

    setPasswordBt = new QPushButton(tr("Set password"));
    setPasswordBt->setText(isSharePasswordSet ? tr("Change password") : tr("Set password"));
    setPasswordBt->setContentsMargins(0, 0, 0, 0);
    setPasswordBt->setFlat(true);
    setPasswordBt->setToolTip(setPasswordBt->text());
    QObject::connect(setPasswordBt, &QPushButton::clicked, [=]() {
        if (FMWindowsIns.windowIdList().count() <= 0)
            return;
        // TODO(zhuangshu):when all windows are closed, the property dialog is still shown, in this
        // condition, the changing share password dialog can not be popup (because no parent winId), it is a bug.
        dpfSlotChannel->push("dfmplugin_titlebar", "slot_SharePasswordSettingsDialog_Show", FMWindowsIns.windowIdList().first());
    });
    hBoxLine3->addWidget(setPasswordBt);
    hBoxLine3->setStretch(0, 1);
    mainLay->addRow(new SectionKeyLabel(tr("Share password"), this), hBoxLine3);
}

void ShareControlWidget::setupShareNotes(QGridLayout *gridLayout)
{
    QPalette pe;
    pe.setColor(QPalette::Text, QColor("#526A7F"));
    m_shareNotes = new QTextBrowser(this);
    m_shareNotes->setFont(this->font());
    m_shareNotes->setContentsMargins(0, 0, 0, 0);
    pe.setColor(QPalette::Text, QColor("#526A7F"));
    m_shareNotes->setPalette(pe);

    static QString notice = tr("This password will be applied to all shared folders, and users without the password can only access shared folders that allow anonymous access. ");
    m_shareNotes->setPlainText(notice);
    m_shareNotes->setFixedHeight(60);
    m_shareNotes->setReadOnly(true);
    m_shareNotes->setFrameStyle(QFrame::NoFrame);
    connect(m_shareNotes, &QTextBrowser::copyAvailable, this, [=](bool yesCopy) {
        if (yesCopy) {
            QTextCursor textCursor = m_shareNotes->textCursor();
            if (textCursor.hasSelection()) {
                textCursor.clearSelection();
                m_shareNotes->setTextCursor(textCursor);
            }
        }
    });
    QGridLayout *notesLayout = new QGridLayout;
    notesLayout->setContentsMargins(9, 0, 9, 9);
    notesLayout->addWidget(m_shareNotes, 0, 0);
    gridLayout->addLayout(notesLayout, 1, 0);
}

void ShareControlWidget::init()
{
    info = InfoFactory::create<AbstractFileInfo>(url);
    if (!info) {
        qWarning() << "cannot create file info of " << url;
        return;
    }

    if (!watcher) {
        watcher = WatcherFactory::create<AbstractFileWatcher>(info->urlOf(UrlInfoType::kParentUrl));
        watcher->startWatcher();
    }

    QString filePath = url.path();
    auto shareName = UserShareHelperInstance->shareNameByPath(filePath);
    if (shareName.isEmpty())
        shareName = info->displayOf(DisPlayInfoType::kFileDisplayName);
    shareNameEditor->setText(shareName);

    bool isShared = UserShareHelperInstance->isShared(filePath);
    shareSwitcher->setChecked(isShared);
    if (isShared) {
        auto shareInfo = UserShareHelperInstance->shareInfoByPath(filePath);
        sharePermissionSelector->setCurrentIndex(shareInfo.value(ShareInfoKeys::kWritable).toBool() ? 0 : 1);
        shareAnonymousSelector->setCurrentIndex(shareInfo.value(ShareInfoKeys::kAnonymous).toBool() ? 1 : 0);
    }

    sharePermissionSelector->setEnabled(isShared);
    shareAnonymousSelector->setEnabled(isShared);
}

void ShareControlWidget::initConnection()
{
    connect(shareSwitcher, &QCheckBox::clicked, this, [this](bool checked) {
        sharePermissionSelector->setEnabled(checked);
        shareAnonymousSelector->setEnabled(checked);
        shareSwitcher->setEnabled(false);
        timer->start();
        if (checked)
            this->shareFolder();
        else
            this->unshareFolder();

        showMoreInfo(checked);
    });

    connect(shareAnonymousSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ShareControlWidget::updateShare);
    connect(sharePermissionSelector, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &ShareControlWidget::updateShare);
    connect(shareNameEditor, &QLineEdit::editingFinished, this, &ShareControlWidget::updateShare);
    connect(UserShareHelper::instance(), &UserShareHelper::sambaPasswordSet, this, &ShareControlWidget::onSambaPasswordSet);

    dpfSignalDispatcher->subscribe("dfmplugin_dirshare", "signal_Share_ShareAdded", this, &ShareControlWidget::updateWidgetStatus);
    dpfSignalDispatcher->subscribe("dfmplugin_dirshare", "signal_Share_ShareRemoved", this, &ShareControlWidget::updateWidgetStatus);
    dpfSignalDispatcher->subscribe("dfmplugin_dirshare", "signal_Share_RemoveShareFailed", this, &ShareControlWidget::updateWidgetStatus);

    connect(watcher.data(), &AbstractFileWatcher::fileRename, this, &ShareControlWidget::updateFile);

    // the timer is used to control the frequency of switcher action.
    connect(timer, &QTimer::timeout, this, [this] { shareSwitcher->setEnabled(true); });

    // this timer is used to refresh shared ip address every 2 seconds.
    refreshIp = new QTimer();
    refreshIp->setInterval(0);
    connect(refreshIp, &QTimer::timeout, this, [this]() {
        selfIp = UserShareHelper::instance()->sharedIP();
        if (networkAddrLabel->text() != selfIp) {
            networkAddrLabel->setText(selfIp);
        }
        refreshIp->setInterval(2000);
    });

    showMoreInfo(shareSwitcher->isChecked());
}

bool ShareControlWidget::validateShareName()
{
    const QString &name = shareNameEditor->text().trimmed();
    if (name.isEmpty())
        return false;

    if (name == ".." || name == ".") {
        DialogManagerInstance->showErrorDialog(tr("The share name must not be two dots (..) or one dot (.)"), "");
        return false;
    }

    bool isShared = UserShareHelperInstance->isShared(url.path());
    if (isShared) {
        const auto &&sharedName = UserShareHelperInstance->shareNameByPath(url.path());
        if (sharedName == name.toLower())
            return true;
    }

    const auto &&shareFileLst = QDir(ConstDef::kShareFileDir).entryInfoList(QDir::Files);
    for (const auto &shareFile : shareFileLst) {
        if (name.toLower() == shareFile.fileName()) {
            DDialog dlg(this);
            dlg.setIcon(QIcon::fromTheme("dialog-warning"));

            if (!shareFile.isWritable()) {
                dlg.setTitle(tr("The share name is used by another user."));
                dlg.addButton(tr("OK", "button"), true);
            } else {
                dlg.setTitle(tr("The share name already exists. Do you want to replace the shared folder?"));
                dlg.addButton(tr("Cancel", "button"), true);
                dlg.addButton(tr("Replace", "button"), false, DDialog::ButtonWarning);
            }

            if (dlg.exec() != DDialog::Accepted) {
                shareNameEditor->setFocus();
                return false;
            }
            break;
        }
    }

    return true;
}

void ShareControlWidget::updateShare()
{
    shareFolder();
}

void ShareControlWidget::shareFolder()
{
    if (!shareSwitcher->isChecked())
        return;

    if (!validateShareName()) {
        shareSwitcher->setChecked(false);
        sharePermissionSelector->setEnabled(false);
        shareAnonymousSelector->setEnabled(false);
        return;
    }

    bool writable = sharePermissionSelector->currentIndex() == 0;
    bool anonymous = shareAnonymousSelector->currentIndex() == 1;
    if (anonymous) {   // set the directory's access permission to 777
        // 1. set the permission of shared folder to 777;
        DecoratorFile file(url);
        if (file.exists() && writable) {
            using namespace DFMIO;
            bool ret = file.setPermissions(file.permissions() | DFile::Permission::kWriteGroup | DFile::Permission::kExeGroup
                                           | DFile::Permission::kWriteOther | DFile::Permission::kExeOther);
            if (!ret)
                qWarning() << "set permission of " << url << "failed.";
        }

        // 2. set the mode 'other' of  /home/$USER to r-x when enable anonymous access,
        // otherwise the anonymous user cannot mount the share successfully.
        // and never change the mode of /root
        if (getuid() != 0) {
            QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
            DecoratorFile home(homePath);
            if (home.exists()) {
                using namespace DFMIO;
                bool ret = home.setPermissions(home.permissions() | DFile::Permission::kReadOther | DFile::Permission::kExeOther);
                if (!ret)
                    qWarning() << "set permission for user home failed: " << homePath;
            }
        }
    }
    ShareInfo info {
        { ShareInfoKeys::kName, shareNameEditor->text().trimmed() },
        { ShareInfoKeys::kPath, url.path() },
        { ShareInfoKeys::kComment, "" },
        { ShareInfoKeys::kWritable, writable },
        { ShareInfoKeys::kAnonymous, anonymous }
    };
    bool success = UserShareHelperInstance->share(info);
    if (!success) {
        shareSwitcher->setChecked(false);
        sharePermissionSelector->setEnabled(false);
        shareAnonymousSelector->setEnabled(false);
    }
}

void ShareControlWidget::unshareFolder()
{
    UserShareHelperInstance->removeShareByPath(url.path());
}

void ShareControlWidget::updateWidgetStatus(const QString &filePath)
{
    if (filePath != url.path())
        return;

    auto shareInfo = UserShareHelperInstance->shareInfoByPath(filePath);
    bool valid = !shareInfo.value(ShareInfoKeys::kName).toString().isEmpty()
            && QFile(shareInfo.value(ShareInfoKeys::kPath).toString()).exists();
    if (valid) {
        shareSwitcher->setChecked(true);
        const auto &&name = shareInfo.value(ShareInfoKeys::kName).toString();
        shareNameEditor->setText(name);
        if (shareInfo.value(ShareInfoKeys::kWritable).toBool())
            sharePermissionSelector->setCurrentIndex(0);
        else
            sharePermissionSelector->setCurrentIndex(1);

        if (shareInfo.value(ShareInfoKeys::kAnonymous).toBool())
            shareAnonymousSelector->setCurrentIndex(1);
        else
            shareAnonymousSelector->setCurrentIndex(0);

        uint shareUid = UserShareHelperInstance->whoShared(name);
        if ((shareUid != info->extendAttributes(ExtInfoType::kOwnerId).toUInt() || shareUid != getuid()) && getuid() != 0)
            this->setEnabled(false);

        sharePermissionSelector->setEnabled(true);
        shareAnonymousSelector->setEnabled(true);
    } else {
        shareSwitcher->setChecked(false);
        sharePermissionSelector->setEnabled(false);
        shareAnonymousSelector->setEnabled(false);
    }
}

void ShareControlWidget::updateFile(const QUrl &oldOne, const QUrl &newOne)
{
    if (UniversalUtils::urlEquals(oldOne, url))
        url = newOne;
    init();
}

void ShareControlWidget::onSambaPasswordSet(bool result)
{
    isSharePasswordSet = result;

    QFont font = this->font();
    int defaultFontSize = font.pointSize();
    font.setPointSize(isSharePasswordSet ? 4 : defaultFontSize);
    sharePasswordlineEditor->setFont(font);
    sharePasswordlineEditor->setFixedWidth(isSharePasswordSet ? ConstDef::kWidgetFixedWidth - 140 : ConstDef::kWidgetFixedWidth - 128);
    sharePasswordlineEditor->setEchoMode(isSharePasswordSet ? QLineEdit::Password : QLineEdit::Normal);
    sharePasswordlineEditor->setText(isSharePasswordSet ? "-----" : tr("None"));
    setPasswordBt->setText(isSharePasswordSet ? tr("Change password") : tr("Set password"));
}

void ShareControlWidget::showMoreInfo(bool showMore)
{
    // line 4: Network path
    mainLay->itemAt(4, QFormLayout::LabelRole)->widget()->setHidden(!showMore);
    // line 5: Username
    mainLay->itemAt(5, QFormLayout::LabelRole)->widget()->setHidden(!showMore);
    // line 6: Share password
    mainLay->itemAt(6, QFormLayout::LabelRole)->widget()->setHidden(!showMore);
    // The bottom notice
    m_shareNotes->setHidden(!showMore);
    auto visibleControl = [this](int row, bool visible) {
        QLayoutItem *layoutItem = mainLay->itemAt(row, QFormLayout::FieldRole);
        QHBoxLayout *hBoxLayout = dynamic_cast<QHBoxLayout *>(layoutItem);
        int count = hBoxLayout->count();
        for (int i = 0; i < count; i++) {
            hBoxLayout->itemAt(i)->widget()->setHidden(visible);
        }
    };
    visibleControl(4, !showMore);
    visibleControl(5, !showMore);
    visibleControl(6, !showMore);

    if (refreshIp) {
        if (showMore)
            refreshIp->start();
        else
            refreshIp->stop();
    }
}

#include "sharecontrolwidget.moc"
