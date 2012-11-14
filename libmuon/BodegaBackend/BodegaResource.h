/***************************************************************************
 *   Copyright © 2012 Aleix Pol Gonzalez <aleixpol@blue-systems.com>       *
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

#ifndef BODEGARESOURCE_H
#define BODEGARESOURCE_H

#include <resources/AbstractResource.h>
#include <bodega/globals.h>

namespace Bodega {
    class AssetInfo;
}

class BodegaResource : public AbstractResource
{
    Q_OBJECT
    public:
        explicit BodegaResource(const Bodega::AssetInfo& info, AbstractResourcesBackend* parent);
        
        virtual QList< PackageState > addonsInformation() { return QList<PackageState>(); }
        virtual QUrl homepage() const { return QUrl(); }
        virtual QString longDescription() const { return QString(); }
        virtual State state() { return AbstractResource::None; }
        virtual QString icon() const { return "preferences-desktop-wallpaper"; }
        virtual QString section() { return "123"; }
        virtual QString installedVersion() const { return "only"; }
        virtual QString sizeDescription() { return "33GiB"; }
        virtual QString origin() const { return "fuuu"; }
        virtual QString categories() { return "plasma/wallpapers"; }
        
        virtual QString name() { return m_info.name; }
        virtual QString availableVersion() const { return m_info.version; }
        virtual QString comment() { return m_info.description; }
        virtual QString packageName() const { return m_info.id; }
        virtual QString license() { return m_info.license; }
        virtual QUrl screenshotUrl();
        virtual QUrl thumbnailUrl();

        QString assetId() const {return m_info.id; }

    private:
        Bodega::AssetInfo m_info;
};

#endif // BODEGARESOURCE_H
