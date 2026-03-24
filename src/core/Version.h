#ifndef VERSION_H
#define VERSION_H

#include <QString>
#include <QList>

class Version {
public:
    explicit Version(const QString &versionString);

    bool operator<(const Version &other) const;
    bool operator==(const Version &other) const;
    bool operator<=(const Version &other) const;
    bool operator>=(const Version &other) const;
    bool operator>(const Version &other) const;

    QString toString() const;
    bool isValid() const; // Added isValid method

private:
    QList<int> m_parts;
};

#endif // VERSION_H
