/*
  safetyfilterproxymodel.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2015-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "modelcontentproxymodel.h"

using namespace GammaRay;

ModelContentProxyModel::ModelContentProxyModel(QObject *parent)
    : QIdentityProxyModel(parent)
{
}

ModelContentProxyModel::~ModelContentProxyModel()
{
}

QVariant ModelContentProxyModel::data(const QModelIndex &proxyIndex, int role) const
{
    // Work around crash in QQmlListModel for unknown roles
#if QT_VERSION < QT_VERSION_CHECK(5, 6, 1)
    if (sourceModel() && sourceModel()->inherits("QQmlListModel")) {
        // data on anything not in roleNames() crashes
        if (!sourceModel()->roleNames().contains(role))
            return QVariant();
    }
#endif

    return QIdentityProxyModel::data(proxyIndex, role);
}

Qt::ItemFlags ModelContentProxyModel::flags(const QModelIndex &index) const
{
    const auto f = QIdentityProxyModel::flags(index);
    if (!index.isValid())
        return f;
    return f | Qt::ItemIsEnabled | Qt::ItemIsSelectable; // always enable items for inspection
}