// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "optionswindow.h"
#include "optionswindow_p.h"
#include "config/configpresenter.h"

#include <dfm-framework/dpf.h>

#include <DTitlebar>

#include <QApplication>
#include <QDesktopWidget>
#include <QScreen>
#include <QSize>

using namespace ddplugin_organizer;
DWIDGET_USE_NAMESPACE

OptionsWindowPrivate::OptionsWindowPrivate(OptionsWindow *qq)
    : QObject(qq)
    , q(qq)
{
    dpfSignalDispatcher->subscribe("ddplugin_canvas", "signal_CanvasManager_AutoArrangeChanged", this, &OptionsWindowPrivate::autoArrangeChanged);
}

OptionsWindowPrivate::~OptionsWindowPrivate()
{
    dpfSignalDispatcher->unsubscribe("ddplugin_canvas", "signal_CanvasManager_AutoArrangeChanged", this, &OptionsWindowPrivate::autoArrangeChanged);
}

bool OptionsWindowPrivate::isAutoArrange()
{
    return dpfSlotChannel->push("ddplugin_canvas", "slot_CanvasManager_AutoArrange").toBool();
}

void OptionsWindowPrivate::setAutoArrange(bool on)
{
    dpfSlotChannel->push("ddplugin_canvas", "slot_CanvasManager_SetAutoArrange", on);
}

void OptionsWindowPrivate::autoArrangeChanged(bool on)
{
    // sync sate that changed on canvas.
    if (autoArrange && autoArrange->checked() != on) {
        autoArrange->setChecked(on);
    }
}

void OptionsWindowPrivate::enableChanged(bool enable)
{
    if (organization) {
        autoArrange->setVisible(!enable);
        organization->reset();

        // adjust size when subwidgets size changed.
        contentWidget->adjustSize();
        q->adjustSize();
    }
}

OptionsWindow::OptionsWindow(QWidget *parent)
    : DAbstractDialog(parent)
    , d(new OptionsWindowPrivate(this))
{

}

OptionsWindow::~OptionsWindow()
{

}

bool OptionsWindow::initialize()
{
    Q_ASSERT(!layout());
    Q_ASSERT(!d->mainLayout);

    // main layout
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize); // update size when subwidget is removed.
    setLayout(mainLayout);
    d->mainLayout = mainLayout;

    // title
    auto titleBar = new DTitlebar(this);
    titleBar->setMenuVisible(false);
    titleBar->setBackgroundTransparent(true);
    titleBar->setTitle(tr("Desktop options"));
    mainLayout->addWidget(titleBar, 0, Qt::AlignTop);

    // content
    d->contentWidget = new QWidget(this);
    mainLayout->addWidget(d->contentWidget);

    auto contentLayout = new QVBoxLayout(d->contentWidget);
    contentLayout->setContentsMargins(10, 0, 10, 10);
    contentLayout->setSpacing(0);
    contentLayout->setSizeConstraint(QLayout::SetFixedSize);
    d->contentLayout = contentLayout;
    d->contentWidget->setLayout(contentLayout);

    // organization
    d->organization = new OrganizationGroup(d->contentWidget);
    d->organization->reset();
    contentLayout->addWidget(d->organization);

    contentLayout->addSpacing(10);

    // auto arrange
    d->autoArrange = new SwitchWidget(tr("Auto arrange icons"), this);
    d->autoArrange->setChecked(d->isAutoArrange());
    d->autoArrange->setFixedSize(400, 48);
    d->autoArrange->setRoundEdge(SwitchWidget::kBoth);
    contentLayout->addWidget(d->autoArrange);
    connect(d->autoArrange, &SwitchWidget::checkedChanged, this, [this](bool check){
        d->setAutoArrange(check);
    });
    // hide auto arrange if organizer is on.
    d->autoArrange->setVisible(!CfgPresenter->isEnable());

    contentLayout->addSpacing(10);

    // size slider
    d->sizeSlider = new SizeSlider(this);
    d->sizeSlider->setRoundEdge(SwitchWidget::kBoth);
    d->sizeSlider->setFixedSize(400, 94);
    d->sizeSlider->init();
    contentLayout->addWidget(d->sizeSlider);

    adjustSize();

    // must be queued
    connect(CfgPresenter, &ConfigPresenter::changeEnableState, d, &OptionsWindowPrivate::enableChanged, Qt::QueuedConnection);
    return true;
}

void OptionsWindow::moveToCenter(const QPoint &cursorPos)
{
    if (auto screen = QGuiApplication::screenAt(cursorPos)) {
        auto pos = (screen->size() - size()) / 2.0;
        if (!pos.isValid()) {
            move(screen->geometry().topLeft());
        } else {
            auto topleft = screen->geometry().topLeft();
            move(topleft.x() + pos.width(), topleft.y() + pos.height());
        }
    }
}
