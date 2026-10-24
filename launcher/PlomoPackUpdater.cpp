// SPDX-License-Identifier: GPL-3.0-only
#include "PlomoPackUpdater.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QTimer>
#include <thread>
#include <utility>

namespace {
constexpr auto kManifestUrl = "https://cdn.playplomo.com/manifest.json";
// Simultaneous file downloads. Thousands of small files make per-request latency the bottleneck, not bandwidth.
constexpr int kParallelDownloads = 16;  // 18 made Cloudflare answer "excessive load" over HTTP/2
constexpr int kMaxAttempts = 4;

QString fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd())
        hash.addData(file.read(1024 * 1024));
    return QString::fromLatin1(hash.result().toHex());
}

bool writeAtomic(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
}  // namespace

PlomoPackUpdater::PlomoPackUpdater(QString instanceRoot, QObject* parent)
    : QObject(parent), m_instanceRoot(std::move(instanceRoot))
{
}

PlomoPackUpdater::~PlomoPackUpdater()
{
    if (m_reply)
        m_reply->abort();
    abortAll();
}

struct PlomoPackUpdater::Download {
    FileEntry entry;
    QSaveFile* file = nullptr;
    QCryptographicHash hash{QCryptographicHash::Sha256};
    QPointer<QNetworkReply> reply;
    qint64 received = 0;
};

void PlomoPackUpdater::abortAll()
{
    const auto active = std::exchange(m_active, {});
    for (auto* d : active) {
        if (d->reply) {
            QObject::disconnect(d->reply, nullptr, this, nullptr);
            d->reply->abort();
            d->reply->deleteLater();
        }
        d->file->cancelWriting();
        delete d->file;
        delete d;
    }
}

qint64 PlomoPackUpdater::activeBytes() const
{
    qint64 total = 0;
    for (const auto* d : m_active)
        total += d->received;
    return total;
}

void PlomoPackUpdater::setState(State state)
{
    m_state = state;
    qInfo() << "PLOMO pack state" << static_cast<int>(state);
    emit stateChanged(state);
}

void PlomoPackUpdater::fail(const QString& message)
{
    qWarning() << "PLOMO pack error:" << message;
    abortAll();
    m_retry.clear();
    m_fullVerify = false;
    m_canPlayOffline = m_state == State::Checking && isInstalled();
    setState(State::Failed);
    emit errorOccurred(message);
}

bool PlomoPackUpdater::validatePath(const QString& path) const
{
    const auto parts = path.split('/');
    if (path.isEmpty() || path.startsWith('/') || path.contains('\\') || path.contains(':') ||
        parts.contains("..") || parts.contains(".") || parts.contains("") || parts.size() < 2)
        return false;
    const QString top = parts.first().toLower();
    return top != "saves" && top != "screenshots" && top != "logs" && top != "crash-reports" &&
           top != "options.txt" && top != "optionsof.txt" && top != "usercache.json" &&
           top != "servers.dat" && top != "servers.dat_old";
}

QString PlomoPackUpdater::destination(const QString& path) const
{
    return QDir(m_instanceRoot).filePath("minecraft/" + path);
}

QString PlomoPackUpdater::sha256(const QString& path) const
{
    return fileSha256(path);
}

QString PlomoPackUpdater::installedVersion() const
{
    QFile manifest(QDir(m_instanceRoot).filePath(".plomo/installed-manifest.json"));
    if (!manifest.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(manifest.readAll()).object().value("version").toString();
}

void PlomoPackUpdater::verify()
{
    if (m_state == State::Installing || m_state == State::Checking || m_state == State::Verifying || !isInstalled())
        return;
    m_fullVerify = true;  // check() refreshes the manifest, then plan() hands over to startVerify()
    check();
}

void PlomoPackUpdater::startVerify()
{
    setState(State::Verifying);
    emit verifyProgressChanged(0);
    qint64 total = 0;
    for (const auto& entry : m_files)
        total += entry.size;
    const auto files = m_files;
    const QString root = QDir(m_instanceRoot).filePath("minecraft/");
    // Detached on purpose: the thread only touches its own copies and reports back through queued calls,
    // which are dropped automatically if this object is gone.
    std::thread([this, files, root, total] {
        QVector<FileEntry> bad;
        qint64 done = 0;
        int lastPercent = 0;
        for (const auto& entry : files) {
            const QFileInfo local(root + entry.path);
            if (!local.isFile() || local.size() != entry.size || fileSha256(local.filePath()) != entry.hash)
                bad.push_back(entry);
            done += entry.size;
            const int percent = total > 0 ? int(done * 100 / total) : 100;
            if (percent != lastPercent) {
                lastPercent = percent;
                QMetaObject::invokeMethod(this, [this, percent] { emit verifyProgressChanged(percent); }, Qt::QueuedConnection);
            }
        }
        QMetaObject::invokeMethod(this, [this, bad] { finishVerify(bad); }, Qt::QueuedConnection);
    }).detach();
}

void PlomoPackUpdater::finishVerify(const QVector<FileEntry>& bad)
{
    m_pending = bad;
    m_total = 0;
    for (const auto& entry : bad)
        m_total += entry.size;
    const bool current = installedVersion() == m_manifest.value("version").toString();
    qInfo() << "PLOMO pack verify: bad files" << bad.size();
    setState(bad.isEmpty() && current ? State::Ready : State::Update);
    emit verified(int(bad.size()));  // after the state change so the window can replace the generic text
}

bool PlomoPackUpdater::isInstalled() const
{
    return QFileInfo::exists(QDir(m_instanceRoot).filePath("instance.cfg")) &&
           QFileInfo::exists(QDir(m_instanceRoot).filePath("mmc-pack.json"));
}

void PlomoPackUpdater::check()
{
    if (m_reply)
        return;
    m_canPlayOffline = false;
    setState(State::Checking);
    QNetworkRequest request(QUrl(QString::fromLatin1(kManifestUrl)));
    request.setTransferTimeout(10000);
    // The manifest is ~1.6 MB; with the last copy and its ETag the server only has to answer "304 not modified".
    const QString cacheBody = QDir(m_instanceRoot).filePath(".plomo/manifest-cache.json");
    const QString cacheTag = QDir(m_instanceRoot).filePath(".plomo/manifest-cache.etag");
    QByteArray cachedBody, cachedTag;
    if (isInstalled()) {
        QFile body(cacheBody), tag(cacheTag);
        if (body.open(QIODevice::ReadOnly) && tag.open(QIODevice::ReadOnly)) {
            cachedBody = body.readAll();
            cachedTag = tag.readAll().trimmed();
        }
    }
    if (!cachedBody.isEmpty() && !cachedTag.isEmpty())
        request.setRawHeader("If-None-Match", cachedTag);
    m_reply = m_network.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this, cachedBody, cacheBody, cacheTag] {
        auto reply = m_reply;
        m_reply = nullptr;
        if (!reply)
            return;
        const auto error = reply->error();
        const auto errorString = reply->errorString();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray etag = reply->rawHeader("ETag");
        QByteArray bytes = reply->readAll();
        reply->deleteLater();
        if (error == QNetworkReply::NoError && status == 304 && !cachedBody.isEmpty())
            bytes = cachedBody;
        if (error != QNetworkReply::NoError) {
            fail(tr("No se pudo conectar con el servidor de PLOMO: %1").arg(errorString));
            return;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            fail(tr("El servidor de PLOMO devolvió datos no válidos"));
            return;
        }
        m_manifest = document.object();
        if (m_manifest.value("schema").toInt() != 1 || m_manifest.value("pack").toString() != "PLOMO" ||
            m_manifest.value("minecraft").toString() != "1.20.1" ||
            m_manifest.value("forge").toString() != "47.4.16" ||
            m_manifest.value("java").toInt() != 17 || !m_manifest.value("mmc_pack").isObject() ||
            !m_manifest.value("files").isArray() || m_manifest.value("version").toString().isEmpty()) {
            fail(tr("La versión publicada de PLOMO no es compatible con este launcher"));
            return;
        }
        m_baseUrl = QUrl(m_manifest.value("base_url").toString());
        if (!m_baseUrl.isValid() || !m_baseUrl.toString().endsWith('/') ||
            (m_baseUrl.scheme() != "http" && m_baseUrl.scheme() != "https")) {
            fail(tr("La dirección de descarga de PLOMO no es válida"));
            return;
        }
        m_files.clear();
        QSet<QString> seen;
        for (const auto& item : m_manifest.value("files").toArray()) {
            const auto object = item.toObject();
            FileEntry entry{object.value("path").toString(), object.value("sha256").toString(),
                            object.value("size").toVariant().toLongLong()};
            bool validHash = entry.hash.size() == 64;
            for (const auto ch : entry.hash)
                validHash = validHash && ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'));
            if (!validatePath(entry.path) || seen.contains(entry.path.toCaseFolded()) ||
                !object.value("size").isDouble() || entry.size < 0 || !validHash) {
                fail(tr("El servidor de PLOMO publicó un archivo con datos no válidos"));
                return;
            }
            seen.insert(entry.path.toCaseFolded());
            m_files.push_back(entry);
        }
        if (status != 304 && !etag.isEmpty() && isInstalled()) {
            writeAtomic(cacheBody, bytes);
            writeAtomic(cacheTag, etag);
        }
        plan();
    });
}

// Decides what to download from the two manifests (the published one and the one saved by the last install),
// without reading the game files: hashing ~850 MB on every start froze the window for seconds.
//  - missing file                          -> download (repair)
//  - listed with a new hash in the pack    -> download (the pack changed it)
//  - otherwise                             -> leave it alone, even if the game or the player rewrote it
void PlomoPackUpdater::plan()
{
    if (m_fullVerify) {
        m_fullVerify = false;
        startVerify();
        return;
    }
    QElapsedTimer timer;
    timer.start();
    QHash<QString, QString> baseline;  // case-folded path -> sha256 installed last time
    QString installedVersion;
    QFile installedManifest(QDir(m_instanceRoot).filePath(".plomo/installed-manifest.json"));
    if (installedManifest.open(QIODevice::ReadOnly)) {
        const auto object = QJsonDocument::fromJson(installedManifest.readAll()).object();
        installedVersion = object.value("version").toString();
        for (const auto& item : object.value("files").toArray()) {
            const auto file = item.toObject();
            baseline.insert(file.value("path").toString().toCaseFolded(), file.value("sha256").toString());
        }
    }
    m_pending.clear();
    m_total = 0;
    for (const auto& entry : m_files) {
        const QFileInfo local(destination(entry.path));
        bool needed;
        if (!local.isFile())
            needed = true;
        else if (!baseline.isEmpty())
            needed = baseline.value(entry.path.toCaseFolded()) != entry.hash;
        else  // no record of a previous install (interrupted one): fall back to checking the content
            needed = local.size() != entry.size || sha256(local.filePath()) != entry.hash;
        if (needed) {
            m_pending.push_back(entry);
            m_total += entry.size;
        }
    }
    const bool installed = isInstalled();
    const bool current = installedVersion == m_manifest.value("version").toString();
    qInfo() << "PLOMO pack version" << m_manifest.value("version").toString() << "files to update" << m_pending.size()
            << "planned in" << timer.elapsed() << "ms";
    setState(m_pending.empty() && installed && current ? State::Ready : installed ? State::Update : State::Download);
}

void PlomoPackUpdater::install()
{
    if (m_state != State::Download && m_state != State::Update)
        return;
    m_next = 0;
    m_completed = 0;
    m_retry.clear();
    m_attempts.clear();
    setState(State::Installing);
    emit progressChanged(0, m_total);
    pump();
}

// Keeps up to kParallelDownloads files in flight; finishes the install once the queue and the pool are empty.
void PlomoPackUpdater::pump()
{
    while (m_state == State::Installing && m_active.size() < kParallelDownloads &&
           (!m_retry.isEmpty() || m_next < m_pending.size()))
        startDownload(!m_retry.isEmpty() ? m_retry.takeFirst() : m_pending.at(m_next++));
    if (m_state == State::Installing && m_active.isEmpty() && m_retry.isEmpty() && m_next >= m_pending.size())
        finishInstall();
}

void PlomoPackUpdater::startDownload(const FileEntry& entry)
{
    const auto target = destination(entry.path);
    if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
        fail(tr("No se pudo crear la carpeta para %1").arg(entry.path));
        return;
    }
    auto* d = new Download;
    d->entry = entry;
    d->file = new QSaveFile(target);
    if (!d->file->open(QIODevice::WriteOnly)) {
        delete d->file;
        delete d;
        fail(tr("No se pudo escribir %1").arg(entry.path));
        return;
    }
    m_active.push_back(d);

    const auto url = m_baseUrl.resolved(QUrl(QString::fromUtf8(QUrl::toPercentEncoding(entry.path, "/"))));
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);  // no data for 30 s: give up on this attempt and retry
    // HTTP/2 multiplexes every download on one connection, and Cloudflare kills that connection ("excessive load")
    // under this many parallel streams, stalling the whole install. Plain HTTP/1.1 is only slightly slower and robust.
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    d->reply = m_network.get(request);
    connect(d->reply, &QNetworkReply::readyRead, this, [this, d] {
        const auto chunk = d->reply->readAll();
        d->hash.addData(chunk);
        if (d->file->write(chunk) != chunk.size()) {
            fail(tr("No se pudo escribir %1").arg(d->entry.path));
            return;
        }
        d->received += chunk.size();
        emit progressChanged(m_completed + activeBytes(), m_total);
    });
    connect(d->reply, &QNetworkReply::finished, this, [this, d] {
        auto* reply = d->reply.data();
        const auto error = reply->error();
        const auto errorString = reply->errorString();
        const auto tail = reply->readAll();
        reply->deleteLater();
        m_active.removeOne(d);
        d->hash.addData(tail);
        const QString path = d->entry.path;
        QString problem;
        bool retryable = false;  // network errors and bad hashes are transient; disk errors are not
        if (error != QNetworkReply::NoError) {
            problem = tr("Falló la descarga de %1: %2").arg(path, errorString);
            retryable = true;
        }
        else if (!tail.isEmpty() && d->file->write(tail) != tail.size())
            problem = tr("No se pudo escribir %1").arg(path);
        else if (d->file->size() != d->entry.size || QString::fromLatin1(d->hash.result().toHex()) != d->entry.hash) {
            problem = tr("La verificación de %1 falló; vuelve a intentarlo").arg(path);
            retryable = true;
        }
        else if (!d->file->commit())
            problem = tr("No se pudo instalar %1").arg(path);
        if (!problem.isEmpty() && retryable && ++m_attempts[path] < kMaxAttempts) {
            qWarning() << "PLOMO retry" << m_attempts[path] << path << problem;
            const auto entry = d->entry;
            d->file->cancelWriting();
            delete d->file;
            delete d;
            m_retry.push_back(entry);
            QTimer::singleShot(1000 * m_attempts[path], this, &PlomoPackUpdater::pump);
            return;
        }
        if (!problem.isEmpty()) {
            d->file->cancelWriting();
            delete d->file;
            delete d;
            fail(problem);
            return;
        }
        m_completed += d->entry.size;
        delete d->file;
        delete d;
        emit progressChanged(m_completed + activeBytes(), m_total);
        pump();
    });
}

void PlomoPackUpdater::finishInstall()
{
    const QDir root(m_instanceRoot);
    const auto packPath = root.filePath("mmc-pack.json");
    const auto configPath = root.filePath("instance.cfg");
    if (!QFileInfo::exists(packPath) &&
        !writeAtomic(packPath, QJsonDocument(m_manifest.value("mmc_pack").toObject()).toJson())) {
        fail(tr("No se pudieron crear los metadatos de la instancia PLOMO"));
        return;
    }
    if (!QFileInfo::exists(configPath) &&
        !writeAtomic(configPath, "[General]\nConfigVersion=1.3\nInstanceType=OneSix\nname=PLOMO\nAutomaticJava=true\n")) {
        fail(tr("No se pudo crear la configuración de la instancia PLOMO"));
        return;
    }
    const auto previousPath = root.filePath(".plomo/installed-manifest.json");
    QFile previousFile(previousPath);
    if (previousFile.open(QIODevice::ReadOnly)) {
        const auto oldFiles = QJsonDocument::fromJson(previousFile.readAll()).object().value("files").toArray();
        previousFile.close();
        QSet<QString> current;
        for (const auto& entry : m_files)
            current.insert(entry.path.toCaseFolded());
        for (const auto& item : oldFiles) {
            const auto object = item.toObject();
            const auto path = object.value("path").toString();
            const auto oldHash = object.value("sha256").toString();
            if (validatePath(path) && !current.contains(path.toCaseFolded()) &&
                sha256(destination(path)) == oldHash)
                QFile::remove(destination(path));
        }
    }
    if (!writeAtomic(previousPath, QJsonDocument(m_manifest).toJson())) {
        fail(tr("No se pudo guardar la versión instalada de PLOMO"));
        return;
    }
    setState(State::Ready);
    emit installed();
}
