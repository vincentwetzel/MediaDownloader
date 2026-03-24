#ifndef YTDLPJSONPARSER_H
#define YTDLPJSONPARSER_H

#include <QObject>
#include <QJsonObject>
#include <QFutureWatcher>

class YtDlpJsonParser : public QObject
{
    Q_OBJECT
public:
    explicit YtDlpJsonParser(QObject *parent = nullptr);

    QJsonObject getExtractors();
    void startGeneration();

signals:
    void extractorsReady();

private:
    QJsonObject loadOrCreateExtractors();
    QJsonObject loadExtractorsFromFile(const QString &path) const;
    QJsonObject loadExtractorsFromAppDir() const;

    QJsonObject m_extractors;
    QFutureWatcher<QJsonObject> *m_loader;
};

#endif // YTDLPJSONPARSER_H
