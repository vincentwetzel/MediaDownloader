#include "YtDlpJsonParser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QtConcurrent>
#include <QFuture>

YtDlpJsonParser::YtDlpJsonParser(QObject *parent) : QObject(parent)
{
    m_loader = new QFutureWatcher<QJsonObject>(this);
    connect(m_loader, &QFutureWatcher<QJsonObject>::finished, this, [this]() {
        m_extractors = m_loader->result();
        emit extractorsReady();
    });
}

QJsonObject YtDlpJsonParser::getExtractors()
{
    return m_extractors;
}

void YtDlpJsonParser::startGeneration()
{
    if (m_loader->isRunning()) {
        return;
    }
    QFuture<QJsonObject> future = QtConcurrent::run([this]() {
        return loadOrCreateExtractors();
    });
    m_loader->setFuture(future);
}

QJsonObject YtDlpJsonParser::loadOrCreateExtractors()
{
    QJsonObject fromAppDir = loadExtractorsFromAppDir();
    if (!fromAppDir.isEmpty()) {
        qInfo() << "YtDlpJsonParser: Loaded extractors from application directory.";
        return fromAppDir;
    }

    qWarning() << "YtDlpJsonParser: extractors.json not found next to the app executable.";
    return QJsonObject();
}

QJsonObject YtDlpJsonParser::loadExtractorsFromFile(const QString &path) const
{
    QFile file(path);
    if (!file.exists()) {
        return QJsonObject();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "YtDlpJsonParser: Failed to open extractors file:" << path << "Error:" << file.errorString();
        return QJsonObject();
    }

    QByteArray fileContent = file.readAll();
    file.close();
    if (fileContent.isEmpty()) {
        return QJsonObject();
    }

    QJsonDocument doc = QJsonDocument::fromJson(fileContent);
    return doc.object();
}

QJsonObject YtDlpJsonParser::loadExtractorsFromAppDir() const
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appFile = QDir(appDir).filePath("extractors.json");
    return loadExtractorsFromFile(appFile);
}
