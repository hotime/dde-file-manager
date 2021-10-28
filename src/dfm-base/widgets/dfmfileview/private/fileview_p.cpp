/*
 * Copyright (C) 2020 ~ 2021 Uniontech Software Technology Co., Ltd.
 *
 * Author:     huanyu<huanyub@uniontech.com>
 *
 * Maintainer: zhengyouge<zhengyouge@uniontech.com>
 *             yanghao<yanghao@uniontech.com>
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

#include "fileview_p.h"

DFMBASE_BEGIN_NAMESPACE
FileViewPrivate::FileViewPrivate(FileView *qq)
    : q(qq)
    , headview(new HeaderView(Qt::Orientation::Horizontal, qq))
{

}

int FileViewPrivate::iconModeColumnCount(int itemWidth) const
{
    int horizontalMargin = 0;
    int contentWidth = q->maximumViewportSize().width();
    return qMax((contentWidth - horizontalMargin - 1) / itemWidth, 1);
}

QUrl FileViewPrivate::modelIndexUrl(const QModelIndex &index) const
{
    return index.data().toUrl();
}

DFMBASE_END_NAMESPACE
