#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QList>
#include <QFileInfo>
#include <QMap>

// A struct to hold the details of a single file to be downloaded by aria2
struct DownloadTarget {
    enum class Type { Generic, Video, Audio, Subtitle, Thumbnail };
    Type type = Type::Generic;
    QString url;
    QString filename;
    // For subtitles
    QString lang;
    QString ext;
};

class YtDlpDownloadInfoExtractor : public QObject
{
    Q_OBJECT
public:
    explicit YtDlpDownloadInfoExtractor(QObject *parent = nullptr);

    void extract(const QString &ytDlpPath, const QStringList &args);
    void cancel();

signals:
    // Emitted on successful extraction
    void extractionSuccess(const QString &title, const QString &thumbnailUrl, const QList<DownloadTarget> &targets, const QString &finalFilename, const QMap<QString, QString> &httpHeaders, const QVariantMap &metadata);
    
    // Emitted on failure
    void extractionFailed(const QString &error);

private:
    QProcess *m_process;
};
