#ifndef DOWNLOADITEM_H
#define DOWNLOADITEM_H

#include <QString>
#include <QVariantMap>

struct DownloadItem {
    QString id;
    QString url;
    QVariantMap options;
    QString tempFilePath;
    QString originalDownloadedFilePath;
    QVariantMap metadata;
    int playlistIndex = -1;
};

#endif // DOWNLOADITEM_H