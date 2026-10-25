// SPDX-License-Identifier: GPL-3.0-only
#include "PlomoSelfUpdater.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

PlomoSelfUpdater::PlomoSelfUpdater(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network) {}

PlomoSelfUpdater::~PlomoSelfUpdater()
{
    cancel();
}

bool PlomoSelfUpdater::canSelfUpdate()
{
#ifdef Q_OS_WIN
    return QFileInfo::exists(QDir(QCoreApplication::applicationDirPath()).filePath("unins000.exe"));
#else
    return false;
#endif
}

bool PlomoSelfUpdater::isTrustedUrl(const QUrl& url, bool allowAnyHost)
{
    if (!url.isValid() || url.host().isEmpty())
        return false;
    if (allowAnyHost)
        return url.scheme() == "https" || url.scheme() == "http";
    if (url.scheme() != "https")
        return false;
    const QString host = url.host().toLower();
    if (host == "github.com")
        return url.path().startsWith("/Maxjose/plomo-launcher/releases/download/");
    return host == "cdn.playplomo.com" || host == "playplomo.com";
}

bool PlomoSelfUpdater::isValidSha256(const QString& sha256)
{
    if (sha256.size() != 64)
        return false;
    for (const auto ch : sha256) {
        if (!((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')))
            return false;
    }
    return true;
}

void PlomoSelfUpdater::start(const QUrl& installerUrl, const QString& sha256, const QString& version)
{
    if (m_reply)
        return;
    m_failed = false;
    m_expected = sha256;
    m_hash.reset();
    m_path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath("PLOMO-Setup-" + version + ".exe");
    QFile::remove(m_path);
    m_file = new QFile(m_path);
    if (!m_file->open(QIODevice::WriteOnly)) {
        cleanup();
        fail(tr("No se pudo guardar la actualización en la carpeta temporal."));
        return;
    }

    QNetworkRequest request(installerUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);  // GitHub redirects to its CDN
    request.setTransferTimeout(30000);  // no data for 30 s: give up
    m_reply = m_network->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (!m_reply || !m_file)
            return;
        const auto chunk = m_reply->readAll();
        m_hash.addData(chunk);
        if (m_file->write(chunk) != chunk.size() && !m_failed) {
            m_failed = true;
            m_reply->abort();  // finish() reports it
        }
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit progressChanged(total > 0 ? int(received * 100 / total) : -1);
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, reply = m_reply.data()] { finish(reply); });
}

void PlomoSelfUpdater::cancel()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file) {
        cleanup();
        QFile::remove(m_path);
    }
}

void PlomoSelfUpdater::cleanup()
{
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
}

void PlomoSelfUpdater::fail(const QString& message)
{
    emit failed(message);
}

void PlomoSelfUpdater::finish(QNetworkReply* reply)
{
    const auto error = reply->error();
    const auto errorString = reply->errorString();
    reply->deleteLater();
    m_reply = nullptr;
    cleanup();
    if (m_failed) {
        QFile::remove(m_path);
        fail(tr("No se pudo escribir la actualización en el disco."));
        return;
    }
    if (error != QNetworkReply::NoError) {
        QFile::remove(m_path);
        fail(tr("No se pudo descargar la actualización: %1").arg(errorString));
        return;
    }
    if (QString::fromLatin1(m_hash.result().toHex()) != m_expected) {
        QFile::remove(m_path);
        fail(tr("La descarga está dañada o no es la esperada (la suma SHA-256 no coincide). No se instaló nada."));
        return;
    }
    // Silent install in place; /PLOMOUPDATE=1 tells the installer to start PLOMO again when it is done.
    const QStringList args{"/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CLOSEAPPLICATIONS", "/PLOMOUPDATE=1"};
    if (!QProcess::startDetached(m_path, args)) {
        fail(tr("No se pudo iniciar el instalador de la actualización."));
        return;
    }
    emit launched();
}
