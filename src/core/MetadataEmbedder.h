#ifndef METADATAEMBEDDER_H
#define METADATAEMBEDDER_H

#include <QObject>
#include <QProcess>

class MetadataEmbedder : public QObject {
    Q_OBJECT

public:
    explicit MetadataEmbedder(QObject *parent = nullptr);
    void embedTrackNumber(const QString &filePath, int trackNumber);

signals:
    void finished(bool success, const QString &error);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    QString m_tempFilePath;
    QString m_originalFilePath;
};

#endif // METADATAEMBEDDER_H