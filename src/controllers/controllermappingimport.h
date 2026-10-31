#pragma once

#include <QString>
#include <QStringList>

class QWidget;

namespace mixxx {

namespace controllers {

/// Result of importing controller mapping files into the user mapping folder
struct MappingImportResult {
    /// File names of the imported mapping XML files
    QStringList mappings;
    /// File names of the imported scripts
    QStringList scripts;
    /// Human readable problems, files that were not imported
    QStringList errors;
    /// Full path of the first imported mapping, empty if none
    QString firstMappingPath;
};

/// Copies mapping XML files, their JavaScript files and ZIP archives that
/// contain them into userMappingsDir, where they are found like the files
/// copied there by hand. Source paths may be content:// URIs on Android,
/// where the user mapping folder cannot be opened in a file manager.
MappingImportResult importMappingFiles(
        const QStringList& sourcePaths, const QString& userMappingsDir);

/// Asks for the files to import, imports them and shows the result.
/// Returns the path of the first imported mapping, empty if none.
QString importMappingFilesInteractive(QWidget* pParent, const QString& userMappingsDir);

} // namespace controllers

} // namespace mixxx
