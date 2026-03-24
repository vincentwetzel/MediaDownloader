#ifndef EXTRACTORJSONPARSER_H
#define EXTRACTORJSONPARSER_H

#include <QObject>
#include <QJsonObject>
#include <QFutureWatcher>
#include <QPair>

class ExtractorJsonParser : public QObject
{
    Q_OBJECT
public:
    explicit ExtractorJsonParser(QObject *parent = nullptr);

    QJsonObject getYtDlpExtractors() const;
    QJsonObject getGalleryDlExtractors() const;
    QJsonObject getAllExtractors() const; // Merges both for convenience
    void startGeneration();

signals:
    void extractorsReady();

private:
    QPair<QJsonObject, QJsonObject> loadExtractors();
    QJsonObject loadExtractorsFromFile(const QString &path) const;

    QJsonObject m_ytDlpExtractors;
    QJsonObject m_galleryDlExtractors;
    QFutureWatcher<QPair<QJsonObject, QJsonObject>> *m_loader;
};

#endif // EXTRACTORJSONPARSER_H
