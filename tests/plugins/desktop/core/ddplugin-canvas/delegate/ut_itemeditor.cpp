// SPDX-FileCopyrightText: 2022 - 2023 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "delegate/itemeditor.h"
#include "view/canvasview_p.h"

#include "stubext.h"

#include <QLayout>

#include <DStyle>
#include <darrowrectangle.h>
#include <QAction>
#include <QMenu>
#include <gtest/gtest.h>

DWIDGET_USE_NAMESPACE
using namespace ddplugin_canvas;

TEST(ItemEditor, construct)
{
    ItemEditor ie;
    EXPECT_EQ(ie.maxTextLength, INT_MAX);
    EXPECT_EQ(ie.maxHeight, -1);
    EXPECT_NE(ie.textEditor, nullptr);
    EXPECT_EQ(ie.opacityEffect, nullptr);
    EXPECT_EQ(ie.tooltip, nullptr);

    EXPECT_EQ(ie.focusProxy(), ie.textEditor);
}

TEST(ItemEditor, editor)
{
    ItemEditor ie;
    EXPECT_EQ(ie.textEditor, ie.editor());
}

TEST(ItemEditor, setMaxHeight)
{
    ItemEditor ie;
    int h = 1000;
    ie.setMaxHeight(h);
    EXPECT_EQ(ie.maxHeight, h);
}

TEST(ItemEditor, setMaximumLength)
{
    ItemEditor ie;
    ie.maxTextLength = 1;
    ie.setMaximumLength(0);

    EXPECT_EQ(ie.maxTextLength, 1);

    ie.setMaximumLength(100);
    EXPECT_EQ(ie.maxTextLength, 100);
}

TEST(ItemEditor, maximumLength)
{
    ItemEditor ie;
    ie.maxTextLength = 14;
    EXPECT_EQ(ie.maximumLength(), 14);
}

TEST(ItemEditor, setBaseGeometry)
{
    ItemEditor ie;
    stub_ext::StubExt stub;
    bool ug = false;
    stub.set_lamda(&ItemEditor::updateGeometry, [&ug](){
        ug = true;
    });

    const QRect base(30, 30, 100, 200);
    const QSize itSize(90, 90);
    const QMargins margin(2,2,2,2);
    ie.setBaseGeometry(base, itSize, margin);
    ASSERT_NE(ie.layout(), nullptr);
    EXPECT_EQ(ie.layout()->spacing(), 0);
    EXPECT_EQ(ie.layout()->contentsMargins(), margin);
    EXPECT_EQ(ie.pos(), base.topLeft());
    EXPECT_EQ(ie.width(), base.width());
    EXPECT_EQ(ie.minimumHeight(), base.height());
    EXPECT_EQ(ie.itemSizeHint, itSize);

    EXPECT_TRUE(ug);
}

TEST(ItemEditor, updateGeometry)
{
    ItemEditor ie;
    ie.resize(100, 300);
    ie.itemSizeHint = QSize(90, 200);
    ie.layout()->setContentsMargins(0, 50, 0, 0);
    auto it = ie.textEditor;

    stub_ext::StubExt stub;
    stub.set_lamda(&QTextDocument::size, [](){
        return QSize(90, 300);
    });

    ASSERT_FALSE(it->isReadOnly());
    ie.maxHeight = 30;
    ie.updateGeometry();

    EXPECT_EQ(it->width(), 100);
    //EXPECT_EQ(it->document()->textWidth(), 90); why is it 100?
    EXPECT_EQ(it->height(), ie.fontMetrics().height() * 3 + 8);

    int minHeight = ie.fontMetrics().height() * 1 + 8;
    ie.maxHeight = minHeight + 25;

    ie.updateGeometry();
    EXPECT_EQ(it->height(), minHeight);

    ie.maxHeight = 300 + 50 + 1;

    ie.updateGeometry();
    EXPECT_EQ(it->height(), 300);

    ie.maxHeight = 300 + 50 - 1;

    ie.updateGeometry();
    EXPECT_EQ(it->height(), ie.maxHeight - 50);
}

TEST(ItemEditor, showAlertMessage)
{
    ItemEditor ie;
    stub_ext::StubExt stub;
    bool show = false;;
    stub.set_lamda((void (*)(DArrowRectangle *, int, int))((void (DArrowRectangle::*)(int, int))&DArrowRectangle::show), [&show](){
        show = true;
    });

    ie.showAlertMessage("sssssssss");

    EXPECT_NE(ie.tooltip, nullptr);
    EXPECT_TRUE(show);
}

TEST(ItemEditor, select)
{
    ItemEditor ie;
    ie.setText("test");
    ie.select("es");

    auto cur = ie.textEditor->textCursor();
    EXPECT_EQ(cur.position(), 2);
}

TEST(ItemEditor, setOpacity)
{
    ItemEditor ie;
    ie.setOpacity(0.5);
    ASSERT_NE(ie.opacityEffect, nullptr);
    EXPECT_EQ(ie.opacityEffect->opacity(), 0.5);

    ie.setOpacity(1.2);
    EXPECT_EQ(ie.opacityEffect, nullptr);
}

TEST(RenameEdit, adjustStyle)
{
    RenameEdit re;
    ASSERT_NE(re.document(), nullptr);
    EXPECT_EQ(re.document()->documentMargin(), 4);
    const int frameRadius = DStyle::pixelMetric(re.style(), DStyle::PM_FrameRadius, nullptr, &re);
    EXPECT_EQ(frameRadius, 0);
}

TEST(RenameEdit, redoundo)
{
    RenameEdit re;
    re.pushStatck("t");
    re.pushStatck("te");

    re.undo();
    EXPECT_EQ(re.toPlainText(), QString("t"));

    re.redo();
    EXPECT_EQ(re.toPlainText(), QString("te"));
}
TEST(RenameEdit, keyPressEvent)
{
    stub_ext::StubExt stub;
    bool callKeyPressEvent = false;
    typedef void(*fun_type)(QKeyEvent*);
    stub.set_lamda((fun_type)(&DTextEdit::keyPressEvent),[&callKeyPressEvent](QKeyEvent*){
        __DBG_STUB_INVOKE__
                callKeyPressEvent = true;
    });
    RenameEdit re;
    QKeyEvent event (QEvent::Type::KeyPress, 0, Qt::KeyboardModifier::AltModifier);
    event.k = Qt::Key_Backtab;
    re.keyPressEvent(&event);
    EXPECT_TRUE(event.m_accept);
    event.k = Qt::Key_Escape;
    re.keyPressEvent(&event);
    EXPECT_TRUE(callKeyPressEvent);
}
TEST(RenameEdit, contextMenuEvent)
{
   stub_ext::StubExt stub;

   auto fun_type = static_cast<QAction *(QMenu::*)(const QPoint &pos, QAction *at)>(&QMenu::exec);
   QAction action;
   stub.set_lamda(fun_type,[&action](QMenu*,const QPoint &pos, QAction *at){return &action;});
   QContextMenuEvent event(QContextMenuEvent::Reason::Mouse,QPoint(10,10),QPoint(20,20),Qt::NoModifier);
   RenameEdit re;
   EXPECT_NO_FATAL_FAILURE(re.contextMenuEvent(&event));
}
TEST(RenameEdit, focusOutEvent)
{
    stub_ext::StubExt stub;
    bool callfocusOutEvent = false;
    typedef void(*fun_type)(QFocusEvent*);
    stub.set_lamda((fun_type)&DTextEdit::focusOutEvent,[&callfocusOutEvent](QFocusEvent*){
        __DBG_STUB_INVOKE__
                callfocusOutEvent = true;
    });
    QFocusEvent event(QEvent::FocusIn);
    RenameEdit re;
    re.focusOutEvent(&event);
    EXPECT_TRUE(callfocusOutEvent);
}
