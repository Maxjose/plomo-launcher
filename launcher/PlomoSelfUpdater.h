// SPDX-License-Identifier: GPL-3.0-only
// Updates the launcher itself: downloads the installer, checks its SHA-256 and runs it silently. The installer replaces
// the program files in place (player data lives outside the installed files) and starts PLOMO again.
#pragma once

#include <QCryptographicHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

class PlomoSelfUpdater : public QObject {
    Q_OBJECT
   public:
    explicit PlomoSelfUpdater(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~PlomoSelfUpdater() override;

    // Only a launcher installed with PLOMO's installer (its uninstaller sits next to PLOMO.exe) updates itself: a portable
    // or development folder would otherwise be left behind while the installer writes to another place.
    static bool canSelfUpdate();
    // The installer is only fetched from PLOMO's own hosts. `allowAnyHost` is for testing against a staging launcher.json.
    static bool isTrustedUrl(const QUrl& url, bool allowAnyHost);
    static bool isValidSha256(const QString& sha256);

    void start(const QUrl& installerUrl, const QString& sha256, const QString& version);
    void cancel();

   signals:
    void progressChanged(int percent);  // -1 when the size is unknown
    void failed(const QString& message);
    void launched();  // the installer is running: the caller should quit so it can replace the files

   private:
    void finish(QNetworkReply* reply);
    void fail(const QString& message);
    void cleanup();

    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
    QFile* m_file = nullptr;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_expected;
    QString m_path;
    bool m_failed = false;
};
