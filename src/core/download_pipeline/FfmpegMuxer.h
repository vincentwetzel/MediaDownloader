#pragma once

#include <QObject>
#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QList>

struct SubtitleFile {
    QString path;
    QString language;
};

class FfmpegMuxer : public QObject
{
    Q_OBJECT
public:
    explicit FfmpegMuxer(QObject *parent = nullptr);

    // Merges multiple input files into a single output file, embedding optional metadata and subtitles
    void merge(const QString &ffmpegPath, const QStringList &inputFiles, const QString &outputFile, const QString &title = QString(), const QString &artworkPath = QString(), const QList<SubtitleFile> &subtitleFiles = {});
    void cancel();

signals:
    void mergeSuccess(const QString &outputFile);
    void mergeFailed(const QString &error);

private:
    void appendProcessOutput(const QByteArray &data);

    QProcess *m_process;
    QString m_currentOutputFile;
    QStringList m_currentInputFiles;
    QList<SubtitleFile> m_currentSubtitleFiles;
    QString m_processOutputTail;
};
