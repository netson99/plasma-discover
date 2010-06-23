/***************************************************************************
 *   Copyright © 2010 Jonathan Thomas <echidnaman@kubuntu.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "StatusWidget.h"

// Qt includes
#include <QtGui/QLabel>

// KDE includes
#include <KGlobal>
#include <KLocale>

// LibQApt includes
#include <libqapt/backend.h>

StatusWidget::StatusWidget(QWidget *parent)
    : KHBox(parent)
    , m_backend(0)
{
    m_countsLabel = new QLabel(this);
    m_countsLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    m_changesLabel = new QLabel(this);
    m_changesLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);

    m_downloadLabel = new QLabel(this);
    m_downloadLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
}

StatusWidget::~StatusWidget()
{
}

void StatusWidget::setBackend(QApt::Backend *backend)
{
    m_backend = backend;
    connect(m_backend, SIGNAL(packageChanged()), this, SLOT(updateStatus()));
    updateStatus();
}

void StatusWidget::updateStatus()
{
    m_countsLabel->setText(i18n("Packages: %1 installed, %2 upgradeable, %3 available",
                                m_backend->packageCount(QApt::Package::Installed),
                                m_backend->packageCount(QApt::Package::Upgradeable),
                                m_backend->packageCount()));

    if (m_backend->markedPackages().count() > 0) {
        m_changesLabel->setText(i18n("Changes: %1 to install, %2 to upgrade, %3 to remove",
                                    m_backend->packageCount(QApt::Package::ToInstall) - m_backend->packageCount(QApt::Package::ToUpgrade),
                                    m_backend->packageCount(QApt::Package::ToUpgrade),
                                    m_backend->packageCount(QApt::Package::ToRemove)));

        m_downloadLabel->setText(i18n("Download size: %1, Space needed: %2",
                                      KGlobal::locale()->formatByteSize(3546),
                                      KGlobal::locale()->formatByteSize(4546)));

        m_changesLabel->show();
        m_downloadLabel->show();
    } else {
        m_changesLabel->hide();
        m_downloadLabel->hide();
    }
}

#include "StatusWidget.moc"