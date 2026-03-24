#ifndef YTDLPARGSBUILDER_H
#define YTDLPARGSBUILDER_H

#include <QStringList>
#include <QVariantMap>
#include "ConfigManager.h"

class YtDlpArgsBuilder {
public:
    YtDlpArgsBuilder();

    QStringList build(ConfigManager *configManager, const QString &url, const QVariantMap &options);

private:
    // Translates UI codec names to yt-dlp format names
    QString getCodecMapping(const QString& codecName);

};

#endif // YTDLPARGSBUILDER_H
