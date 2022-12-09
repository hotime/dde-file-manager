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
#ifndef SHAREFILEINFO_H
#define SHAREFILEINFO_H

#include "dfmplugin_myshares_global.h"
#include "dfm-base/interfaces/abstractfileinfo.h"

namespace dfmplugin_myshares {
class ShareFileInfoPrivate;
class ShareFileInfo : public DFMBASE_NAMESPACE::AbstractFileInfo
{
    ShareFileInfoPrivate *d;

public:
    explicit ShareFileInfo(const QUrl &url);
    virtual ~ShareFileInfo() override;

    // AbstractFileInfo interface
public:
    virtual QString displayOf(const DisplayInfoType type) const override;
    virtual QString nameOf(const FileNameInfoType type) const override;
    virtual QUrl urlOf(const FileUrlInfoType type) const override;
    virtual bool isAttributes(const FileIsType type) const override;
    virtual bool canAttributes(const FileCanType type) const override;
    virtual void refresh() override;
};

}

#endif   // SHAREFILEINFO_H
