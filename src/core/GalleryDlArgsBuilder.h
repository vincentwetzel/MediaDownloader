#ifndef GALLERYDLARGSBUILDER_H
#define GALLERYDLARGSBUILDER_H

#include <QStringList>
#include <QVariantMap>
#include "ConfigManager.h"

class GalleryDlArgsBuilder {
public:
    GalleryDlArgsBuilder(ConfigManager *configManager);

    QStringList build(const QString &url, const QVariantMap &options);

private:
    ConfigManager *m_configManager;
};

#endif // GALLERYDLARGSBUILDER_H
