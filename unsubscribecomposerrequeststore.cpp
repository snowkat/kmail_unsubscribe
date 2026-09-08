#include "unsubscribecomposerrequeststore.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

using namespace KMailUnsubscribe;

ComposerRequestStore::ComposerRequestStore(const QString &directory)
    : mDirectory(directory)
{
    if (mDirectory.isEmpty())
    {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        if (!runtime.isEmpty())
        {
            mDirectory = runtime + QStringLiteral("/kmail-unsubscribe-composers");
        }
    }
}

QString ComposerRequestStore::pathForToken(const QString &token) const
{
    const QUuid uuid(token);
    if (mDirectory.isEmpty() || uuid.isNull() || uuid.toString(QUuid::WithoutBraces) != token)
    {
        return {};
    }
    return mDirectory + QLatin1Char('/') + token + QStringLiteral(".json");
}

bool ComposerRequestStore::remember(const QString &token, const ComposerDeleteRequest &request)
{
    const QString path = pathForToken(token);
    if (path.isEmpty() || request.originalId <= 0 || !QDir().mkpath(mDirectory)
        || !QFile::setPermissions(mDirectory, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner))
    {
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner))
    {
        return false;
    }
    const QByteArray data = QJsonDocument(QJsonObject{
        {QStringLiteral("originalId"), QString::number(request.originalId)},
        {QStringLiteral("subject"), request.subject},
    }).toJson(QJsonDocument::Compact);
    return file.write(data) == data.size() && file.commit();
}

std::optional<ComposerDeleteRequest> ComposerRequestStore::take(const QString &token)
{
    const QString path = pathForToken(token);
    // An atomic claim also prevents two composer instances using the consent.
    const QString claimed = path + QStringLiteral(".claimed");
    if (path.isEmpty() || !QFile::rename(path, claimed))
    {
        return std::nullopt;
    }
    QFile file(claimed);
    const bool opened = file.open(QIODevice::ReadOnly);
    const QByteArray data = opened ? file.read(16384) : QByteArray();
    file.close();
    file.remove();
    const auto object = QJsonDocument::fromJson(data).object();
    bool validId = false;
    const qint64 id = object.value(QStringLiteral("originalId")).toString().toLongLong(&validId);
    if (!validId || id <= 0)
    {
        return std::nullopt;
    }
    return ComposerDeleteRequest{id, object.value(QStringLiteral("subject")).toString()};
}

void ComposerRequestStore::discard(const QString &token)
{
    const QString path = pathForToken(token);
    if (!path.isEmpty())
    {
        QFile::remove(path);
    }
}
