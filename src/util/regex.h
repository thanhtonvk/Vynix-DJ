#pragma once

#include <QRegularExpression>
#include <QStringList>
#include <QString>

namespace RegexUtils {

inline QString fileExtensionsRegex(QStringList extensions) {
    // Escape every extension appropriately
    for (int i = 0; i < extensions.size(); ++i) {
        extensions[i] = QRegularExpression::escape(extensions[i]);
    }
    // Turn the list into a "\\.(jpg|gif|etc)$" style regex string
    return QStringLiteral("\\.(%1)$").arg(extensions.join('|'));
}

} // namespace RegexUtils
