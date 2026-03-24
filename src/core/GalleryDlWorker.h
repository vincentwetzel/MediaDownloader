#ifndef GALLERYDLWORKER_H
#define GALLERYDLWORKER_H

#include <QObject>
#include <QProcess>
#include <QVariantMap>

class GalleryDlWorker : public QObject {
    Q_OBJECT

public:
    explicit GalleryDlWorker(const QString &id, const QStringList &args, QObject *parent = nullptr);
    QString getId() const { return m_id; }
    void start();
    void killProcess();

signals:
    void progressUpdated(const QString &id, const QVariantMap &progressData);
    void finished(const QString &id, bool success, const QString &message, const QString &finalFilename, const QVariantMap &metadata);
    void outputReceived(const QString &id, const QString &output);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onReadyReadStandardOutput();

private:
    QString m_id;
    QStringList m_args;
    QProcess *m_process;
    QString m_processOutput;

    QString resolveExecutablePath(const QString &name) const;
};

#endif // GALLERYDLWORKER_H
