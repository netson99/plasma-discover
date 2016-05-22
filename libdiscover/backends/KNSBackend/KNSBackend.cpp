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

// Qt includes
#include <QDebug>
#include <QDir>
#include <QFileInfo>

// Attica includes
#include <attica/content.h>
#include <attica/providermanager.h>

// KDE includes
#include <kns3/downloadmanager.h>
#include <KConfigGroup>
#include <KDesktopFile>
#include <KLocalizedString>

// DiscoverCommon includes
#include "Transaction/Transaction.h"
#include "Transaction/TransactionModel.h"

// Own includes
#include "KNSBackend.h"
#include "KNSResource.h"
#include "KNSReviews.h"
#include <resources/StandardBackendUpdater.h>

MUON_BACKEND_PLUGIN(KNSBackend)

QDebug operator<<(QDebug s, const Attica::Category& cat) {
    const QString name = cat.isValid() ? cat.name() : QStringLiteral("Invalid");
    s.nospace() << "Category(" << name << ')';
    return s.space();
}

QDebug operator<<(QDebug s, const Attica::Provider& prov) {
    if (prov.isValid())
        s.nospace() << "Provider(" << prov.name() << ':' << prov.baseUrl() << ')';
    else
        s.nospace() << "Provider(Invalid)";
    return s.space();
}

class SharedManager : public QObject
{
Q_OBJECT
public:
    SharedManager() {
        atticaManager.loadDefaultProviders();
    }

    void requestCategories(Attica::Provider &provider)
    {
        const auto it = m_categories.constFind(provider.baseUrl());
        if (it != m_categories.constEnd()) {
            Q_EMIT categoriesFor(provider.baseUrl(), it.value());
        } else {
            auto ret = provider.requestCategories();
            ret->setProperty("url", provider.baseUrl());
            connect(ret, &Attica::GetJob::finished, this, &SharedManager::removeJob);
            ret->start();
        }
    }

    void removeJob(Attica::BaseJob* job) {
        Attica::ListJob<Attica::Category>* j = static_cast<Attica::ListJob<Attica::Category>*>(job);

        const Attica::Category::List categoryList = j->itemList();
        const QUrl url = job->property("url").toUrl();
        m_categories[url] = categoryList;
        Q_EMIT categoriesFor(url, categoryList);
    }

Q_SIGNALS:
    void categoriesFor(const QUrl &prov, const Attica::Category::List& categoryList);

public:
    QHash<QUrl, Attica::Category::List> m_categories;
    Attica::ProviderManager atticaManager;
};

Q_GLOBAL_STATIC(SharedManager, s_shared);

KNSBackend::KNSBackend(QObject* parent)
    : AbstractResourcesBackend(parent)
    , m_fetching(false)
    , m_isValid(true)
    , m_page(0)
    , m_reviews(new KNSReviews(this))
    , m_updater(new StandardBackendUpdater(this))
{}

KNSBackend::~KNSBackend() = default;

void KNSBackend::markInvalid()
{
    qWarning() << "invalid kns backend!" << m_name;
    m_isValid = false;
    setFetching(false);
}

void KNSBackend::setMetaData(const QString& path)
{
    KDesktopFile cfg(path);
    KConfigGroup service = cfg.group("Desktop Entry");

    m_iconName = service.readEntry("Icon", QString());

    const QString knsrc = service.readEntry("X-Muon-Arguments", QString());
    m_name = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, knsrc);
    if (m_name.isEmpty()) {
        QString p = QFileInfo(path).dir().filePath(knsrc);
        if (QFile::exists(p))
            m_name = p;

    }

    if (m_name.isEmpty()) {
        markInvalid();
        qWarning() << "Couldn't find knsrc file" << knsrc;
        return;
    }

    const KConfig conf(m_name);
    if (!conf.hasGroup("KNewStuff3")) {
        markInvalid();
        qWarning() << "Config group not found! Check your KNS3 installation.";
        return;
    }

    const KConfigGroup group = conf.group("KNewStuff3");
    m_extends = group.readEntry("Extends", QStringList());
    const QUrl entry(group.readEntry("ProvidersUrl", QString()));
    if(!s_shared->atticaManager.providerFiles().contains(entry)) {
        s_shared->atticaManager.addProviderFile(entry);
    }
    connect(&s_shared->atticaManager, &Attica::ProviderManager::defaultProvidersLoaded, this, &KNSBackend::startFetchingCategories);

    const QStringList cats = group.readEntry("Categories", QStringList());
    Q_ASSERT(!cats.isEmpty());
    foreach(const QString& c, cats) {
        m_categories.insert(c, Attica::Category());
    }

    m_manager = new KNS3::DownloadManager(m_name, this);
    connect(m_manager, &KNS3::DownloadManager::searchResult, this, &KNSBackend::receivedEntries);
    connect(m_manager, &KNS3::DownloadManager::entryStatusChanged, this, &KNSBackend::statusChanged);

    //otherwise this will be executed when defaultProvidersLoaded is emitted
    if (!s_shared->atticaManager.providers().isEmpty()) {
        startFetchingCategories();
    }
}

void KNSBackend::setFetching(bool f)
{
    if(m_fetching!=f) {
        m_fetching = f;
        emit fetchingChanged();
    }
}

bool KNSBackend::isValid() const
{
    return m_isValid;
}

void KNSBackend::startFetchingCategories()
{
    if (s_shared->atticaManager.providers().isEmpty()) {
        qWarning() << "no providers for" << m_name;
        markInvalid();
        return;
    }

    setFetching(true);
    m_provider = s_shared->atticaManager.providers().last();

    connect(s_shared, &SharedManager::categoriesFor, this, &KNSBackend::categoriesLoaded);
    s_shared->requestCategories(m_provider);
}

void KNSBackend::categoriesLoaded(const QUrl &provBaseUrl, const Attica::Category::List& categoryList)
{
    if (provBaseUrl != m_provider.baseUrl()) {
        return;
    }
    if(categoryList.isEmpty()) {
        qWarning() << "Network error";
        markInvalid();
        return;
    }

    foreach(const Attica::Category& category, categoryList) {
        const auto it = m_categories.find(category.name());
        if (it != m_categories.end()) {
//             qDebug() << "Adding category: " << category.name();
            *it = category;
        }
    }
    
    // Remove categories for which we got no matching category from the provider.
    // Otherwise we'll fetch an empty category specificier which can return
    // everything and the kitchen sink if the remote provider feels like it.
    for (auto it = m_categories.begin(); it != m_categories.end();) {
        if (!it.value().isValid()) {
            qWarning() << "Found invalid category" << it.key();
            it = m_categories.erase(it);
        } else
            ++it;
    }
    if (m_categories.isEmpty()) {
        qDebug() << "didn't find categories" << categoryList;
        markInvalid();
        return;
    }

    Attica::ListJob<Attica::Content>* jj =
        m_provider.searchContents(m_categories.values(), QString(), Attica::Provider::Alphabetical, m_page, 100);
    connect(jj, &Attica::GetJob::finished, this, &KNSBackend::receivedContents);
    jj->start();
}

void KNSBackend::receivedContents(Attica::BaseJob* job)
{
    if(job->metadata().error() != Attica::Metadata::NoError) {
        qWarning() << "Network error";
        setFetching(false);
        return;
    }
    Attica::ListJob<Attica::Content>* listJob = static_cast<Attica::ListJob<Attica::Content>*>(job);
    Attica::Content::List contents = listJob->itemList();
    
    if(contents.isEmpty()) {
        m_page = 0;
        m_manager->search();
        return;
    }
    QString filename = QFileInfo(m_name).fileName();
    foreach(const Attica::Content& c, contents) {
        KNSResource* r = new KNSResource(c, filename, this);
        m_resourcesByName.insert(c.id(), r);
        connect(r, &KNSResource::stateChanged, this, &KNSBackend::updatesCountChanged);
    }
    m_page++;
    Attica::ListJob<Attica::Content>* jj =
        m_provider.searchContents(m_categories.values(), QString(), Attica::Provider::Alphabetical, m_page, 100);
    connect(jj, &Attica::GetJob::finished, this, &KNSBackend::receivedContents);
    jj->start();
}

void KNSBackend::receivedEntries(const KNS3::Entry::List& entries)
{
    if(entries.isEmpty()) {
        setFetching(false);
        return;
    }
    
    foreach(const KNS3::Entry& entry, entries) {
        statusChanged(entry);
    }
    ++m_page;
    m_manager->search(m_page);
}

void KNSBackend::statusChanged(const KNS3::Entry& entry)
{
    KNSResource* r = qobject_cast<KNSResource*>(m_resourcesByName.value(entry.id()));
    if(r)
        r->setEntry(entry);
    else
        qWarning() << "unknown entry changed" << entry.id() << entry.name();
}

class LambdaTransaction : public Transaction
{
public:
    LambdaTransaction(QObject* parent, AbstractResource* res, Transaction::Role role)
        : Transaction(parent, res, role)
    {
        setCancellable(false);
        TransactionModel::global()->addTransaction(this);
    }

    ~LambdaTransaction() override {
        TransactionModel::global()->removeTransaction(this);
    }

    void cancel() override {}
};

void KNSBackend::removeApplication(AbstractResource* app)
{
    QScopedPointer<Transaction> t(new LambdaTransaction(this, app, Transaction::RemoveRole));
    KNSResource* r = qobject_cast<KNSResource*>(app);
    Q_ASSERT(r->entry());
    m_manager->uninstallEntry(*r->entry());
}

void KNSBackend::installApplication(AbstractResource* app)
{
    QScopedPointer<Transaction> t(new LambdaTransaction(this, app, Transaction::InstallRole));
    KNSResource* r = qobject_cast<KNSResource*>(app);
    Q_ASSERT(r->entry());
    m_manager->installEntry(*r->entry());
}

void KNSBackend::installApplication(AbstractResource* app, const AddonList& /*addons*/)
{
    installApplication(app);
}

AbstractResource* KNSBackend::resourceByPackageName(const QString& name) const
{
    return m_resourcesByName[name];
}

int KNSBackend::updatesCount() const
{
    return m_updater->updatesCount();
}

AbstractReviewsBackend* KNSBackend::reviewsBackend() const
{
    return m_reviews;
}

QList<AbstractResource*> KNSBackend::searchPackageName(const QString& searchText)
{
    QList<AbstractResource*> ret;
    foreach(AbstractResource* r, m_resourcesByName) {
        if(r->name().contains(searchText, Qt::CaseInsensitive) || r->comment().contains(searchText, Qt::CaseInsensitive))
            ret += r;
    }
    return ret;
}

QVector<AbstractResource*> KNSBackend::allResources() const
{
    return containerValues<QVector<AbstractResource*>>(m_resourcesByName);
}

bool KNSBackend::isFetching() const
{
    return m_fetching;
}

AbstractBackendUpdater* KNSBackend::backendUpdater() const
{
    return m_updater;
}

#include "KNSBackend.moc"
