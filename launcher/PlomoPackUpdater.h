// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QUrl>
#include <QHash>
#include <QVector>

class QNetworkReply;
class QSaveFile;

// Handles the one managed PLOMO instance. The manifest is authoritative only
// for files it lists; player files are never included in that set.
class PlomoPackUpdater : public QObject {
    Q_OBJECT

public:
    enum class State { Checking, Download, Update, Ready, Installing, Failed, Verifying };
    explicit PlomoPackUpdater(QString instanceRoot, QObject* parent = nullptr);
    ~PlomoPackUpdater() override;
    void check();
    void install();
    // Full check by content (SHA-256 of every file) in a worker thread; wrong or missing files are queued for install().
    void verify();
    State state() const { return m_state; }
    // Latest version published on the server (from the manifest).
    QString version() const { return m_manifest.value("version").toString(); }
    // Version actually installed on disk; empty when nothing is installed.
    QString installedVersion() const;
    // Channel the installed version came from ("alpha" or empty for the stable release).
    QString installedChannel() const;
    // Picks which manifest check() reads: the stable release (default) or the alpha one. Call check() afterwards.
    void setAlphaChannel(bool alpha) { m_alpha = alpha; }
    bool alphaChannel() const { return m_alpha; }
    bool isInstalled() const;
    // True when the manifest could not be fetched but a previous install can still be played.
    bool canPlayOffline() const { return m_canPlayOffline; }

signals:
    void stateChanged(PlomoPackUpdater::State state);
    void progressChanged(qint64 completed, qint64 total);
    void errorOccurred(const QString& message);
    void installed();
    void verifyProgressChanged(int percent);
    void verified(int mismatches);

private:
    struct FileEntry {
        QString path;
        QString hash;
        qint64 size;
    };
    void setState(State state);
    void fail(const QString& message);
    void plan();
    void startVerify();
    void finishVerify(const QVector<FileEntry>& bad);
    void pump();
    void startDownload(const FileEntry& entry);
    void abortAll();
    qint64 activeBytes() const;
    void finishInstall();
    bool validatePath(const QString& path) const;
    QString destination(const QString& path) const;
    QString sha256(const QString& path) const;

    QString m_instanceRoot;
    bool m_alpha = false;
    QNetworkAccessManager m_network;
    QJsonObject m_manifest;
    QUrl m_baseUrl;
    QVector<FileEntry> m_files;
    QVector<FileEntry> m_pending;
    QPointer<QNetworkReply> m_reply;
    struct Download;
    QVector<Download*> m_active;
    QVector<FileEntry> m_retry;
    QHash<QString, int> m_attempts;
    int m_next = 0;
    qint64 m_completed = 0;
    qint64 m_total = 0;
    State m_state = State::Checking;
    bool m_canPlayOffline = false;
    bool m_fullVerify = false;
};
