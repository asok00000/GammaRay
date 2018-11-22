/*
  problemreporterwidget.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2018 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>

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

#include "problemreporterwidget.h"
#include "ui_problemreporterwidget.h"
#include "problemclientmodel.h"
#include "problemreporterclient.h"

#include <ui/searchlinecontroller.h>
#include <ui/contextmenuextension.h>

#include <common/objectbroker.h>
#include <common/objectmodel.h>
#include <common/tools/problemreporter/problemmodelroles.h>

#include <QMenu>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QMouseEvent>
#include <QStyleOptionViewItemV4>


#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
Q_DECLARE_METATYPE(Qt::CheckState)
#endif

using namespace GammaRay;

class ProblemFilterDelegate : public QStyledItemDelegate
{
public:
    explicit ProblemFilterDelegate(QAbstractItemView *view)
     : QStyledItemDelegate(view)
    {}

protected:
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        QString title = index.data(Qt::DisplayRole).toString();
        QString description = index.data(Qt::ToolTipRole).toString();
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();

        opt.text = index.data(Qt::DisplayRole).toString() + QChar::LineSeparator + index.data(Qt::ToolTipRole).toString();
        auto textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt);

        // We first paint a normal ItemViewItem but with no text and then draw the title and description ourselves
        painter->save();
        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);
        style->drawItemText(painter, textRect, Qt::AlignLeft | Qt::AlignTop, opt.palette, opt.state & QStyle::State_Enabled, title, QPalette::Text);
        painter->setOpacity(0.5);
        style->drawItemText(painter, textRect, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap, opt.palette,
                            opt.state & QStyle::State_Enabled, description, QPalette::Text);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        opt.text = index.data(Qt::DisplayRole).toString() + QChar::LineSeparator + index.data(Qt::ToolTipRole).toString();
        QStyle *style = option.widget ? option.widget->style() : QApplication::style();
        return style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), option.widget);
    }
};


static QObject *createProblemReporterClient(const QString & /*name*/, QObject *parent)
{
    return new ProblemReporterClient(parent);
}

ProblemReporterWidget::ProblemReporterWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProblemReporterWidget)
    , m_stateManager(this)
{
    ui->setupUi(this);

    ObjectBroker::registerClientObjectFactoryCallback<ProblemReporterInterface *>(
        createProblemReporterClient);
    ProblemReporterInterface *iface = ObjectBroker::object<ProblemReporterInterface *>();

    connect(ui->scanButton, SIGNAL(clicked()), iface, SLOT(requestScan()));
    connect(ui->scanButton, SIGNAL(clicked()), ui->progressBar, SLOT(show()));
    connect(iface, SIGNAL(problemScansFinished()), ui->progressBar, SLOT(hide()));
    ui->progressBar->setVisible(false);

    m_problemsModel = new ProblemClientModel(this);
    m_problemsModel->setSourceModel(ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.ProblemModel")));
    ui->problemView->header()->setObjectName("problemViewHeader");
    ui->problemView->setDeferredResizeMode(0, QHeaderView::ResizeToContents);
    ui->problemView->setDeferredResizeMode(1, QHeaderView::ResizeToContents);
    ui->problemView->setModel(m_problemsModel);
    ui->problemView->sortByColumn(0, Qt::AscendingOrder);

    connect(ui->problemView, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(problemViewContextMenu(QPoint)));

    new SearchLineController(ui->searchLine, m_problemsModel);

    m_availableCheckersModel = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.AvailableProblemCheckersModel"));
    ui->problemfilterwidget->viewport()->setAutoFillBackground(false);
    ui->problemfilterwidget->setAttribute(Qt::WA_MacShowFocusRect, false);
    ui->problemfilterwidget->setItemDelegate(new ProblemFilterDelegate(ui->problemfilterwidget));
    ui->problemfilterwidget->setModel(m_availableCheckersModel);

    connect(m_availableCheckersModel, SIGNAL(dataChanged(QModelIndex, QModelIndex, QVector<int>)), this, SLOT(updateFilter(QModelIndex, QModelIndex, QVector<int>)));
}

ProblemReporterWidget::~ProblemReporterWidget()
{
}

void ProblemReporterWidget::problemViewContextMenu(const QPoint &p)
{
    QModelIndex index = ui->problemView->indexAt(p);
    ObjectId objectId = index.data(ObjectModel::ObjectIdRole).value<ObjectId>();

    QMenu menu;
    ContextMenuExtension ext(objectId);
    auto sourceLocations = index.data(ProblemModelRoles::SourceLocationRole).value<QVector<SourceLocation>>();
    foreach (const SourceLocation &sourceLocation, sourceLocations) {
        ext.setLocation(ContextMenuExtension::GoTo, sourceLocation);
    }
    ext.populateMenu(&menu);

    menu.exec(ui->problemView->viewport()->mapToGlobal(p));
}

void GammaRay::ProblemReporterWidget::updateFilter(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if (!roles.empty() && !roles.contains(Qt::CheckStateRole))
        return;

    for (int i = topLeft.row(); i <= bottomRight.row(); ++i) {
        auto index = topLeft.sibling(i, 0);
        auto checkState = index.data(Qt::CheckStateRole);
        auto id = index.data(Qt::EditRole).toString();
        if (!checkState.canConvert<Qt::CheckState>())
            continue;

        if (index.data(Qt::CheckStateRole).value<Qt::CheckState>() == Qt::Checked) {
            m_problemsModel->enableChecker(id);
        } else {
            m_problemsModel->disableChecker(id);
        }
    }
}
