#include "Version.h"
#include <QStringList>

Version::Version(const QString &versionString) {
    QStringList parts = versionString.split('.');
    for (const QString &part : parts) {
        bool ok;
        int p = part.toInt(&ok);
        if (ok) {
            m_parts.append(p);
        } else {
            // Handle cases where a part is not an integer (e.g., "2023.12.01.1")
            // For simplicity, we can just append 0 or ignore, depending on desired behavior.
            // For now, let's append 0 for non-integer parts to keep the size consistent.
            m_parts.append(0);
        }
    }
}

bool Version::operator<(const Version &other) const {
    int minSize = qMin(m_parts.size(), other.m_parts.size());
    for (int i = 0; i < minSize; ++i) {
        if (m_parts[i] < other.m_parts[i]) {
            return true;
        }
        if (m_parts[i] > other.m_parts[i]) {
            return false;
        }
    }
    return m_parts.size() < other.m_parts.size();
}

bool Version::operator==(const Version &other) const {
    return m_parts == other.m_parts;
}

bool Version::operator<=(const Version &other) const {
    return (*this < other) || (*this == other);
}

bool Version::operator>=(const Version &other) const {
    return (*this > other) || (*this == other);
}

bool Version::operator>(const Version &other) const {
    return !(*this <= other);
}

QString Version::toString() const {
    QStringList parts;
    for (int part : m_parts) {
        parts.append(QString::number(part));
    }
    return parts.join(".");
}

bool Version::isValid() const {
    return !m_parts.isEmpty();
}
