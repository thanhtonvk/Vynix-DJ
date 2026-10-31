#include "controllers/controllermappingimport.h"

#include <QBuffer>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QRegularExpression>
#include <QStandardPaths>

// Private Qt API, in QtCore since Qt 6.8 and in QtGui before
#if __has_include(<QtCore/private/qzipreader_p.h>)
#include <QtCore/private/qzipreader_p.h>
#define MIXXX_MAPPING_IMPORT_ZIP
#elif __has_include(<QtGui/private/qzipreader_p.h>)
#include <QtGui/private/qzipreader_p.h>
#define MIXXX_MAPPING_IMPORT_ZIP
#endif

#include "controllers/defs_controllers.h"

namespace mixxx {

namespace controllers {

namespace {

struct ImportFile {
    QString name;
    QByteArray data;
};

bool isMappingXml(const QByteArray& data) {
    return data.contains("<MixxxControllerPreset") || data.contains("<MixxxMIDIPreset");
}

bool isZip(const QString& name, const QByteArray& data) {
    return name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) ||
            data.startsWith("PK\x03\x04");
}

/// File name for a mapping XML whose original name is not known, which
/// happens with some Android document providers
QString mappingFileNameFromContent(const QByteArray& data) {
    QDomDocument doc;
    doc.setContent(data);
    QString name = doc.documentElement()
                           .firstChildElement(QStringLiteral("info"))
                           .firstChildElement(QStringLiteral("name"))
                           .text()
                           .trimmed();
    name.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]+")), QStringLiteral("-"));
    if (name.isEmpty()) {
        name = QStringLiteral("Imported");
    }
    return name + QStringLiteral(MIDI_MAPPING_EXTENSION);
}

void appendZipEntries(const QString& zipName,
        const QByteArray& data,
        QList<ImportFile>* pFiles,
        QStringList* pErrors) {
#ifdef MIXXX_MAPPING_IMPORT_ZIP
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QZipReader zip(&buffer);
    if (!zip.isReadable() || zip.status() != QZipReader::NoError) {
        pErrors->append(QObject::tr("%1: not a readable ZIP file").arg(zipName));
        return;
    }
    int count = 0;
    for (const auto& entry : zip.fileInfoList()) {
        const QString name = QFileInfo(entry.filePath).fileName();
        // Skip folders and the resource forks added by macOS
        if (!entry.isFile || entry.filePath.contains(QStringLiteral("__MACOSX")) ||
                name.startsWith(QChar('.'))) {
            continue;
        }
        if (name.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive) ||
                name.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)) {
            pFiles->append({name, zip.fileData(entry.filePath)});
            ++count;
        }
    }
    if (count == 0) {
        pErrors->append(QObject::tr("%1: contains no mapping (.xml) or script (.js) files")
                        .arg(zipName));
    }
#else
    Q_UNUSED(data);
    Q_UNUSED(pFiles);
    pErrors->append(QObject::tr(
            "%1: ZIP files are not supported in this build, extract it first")
                    .arg(zipName));
#endif
}

} // namespace

MappingImportResult importMappingFiles(
        const QStringList& sourcePaths, const QString& userMappingsDir) {
    MappingImportResult result;

    QList<ImportFile> files;
    for (const auto& sourcePath : sourcePaths) {
        QFile file(sourcePath);
        // QFileInfo returns the display name for content:// URIs
        const QString name = QFileInfo(sourcePath).fileName();
        if (!file.open(QIODevice::ReadOnly)) {
            result.errors.append(QObject::tr("%1: cannot be read").arg(name));
            continue;
        }
        const QByteArray data = file.readAll();
        if (isZip(name, data)) {
            appendZipEntries(name, data, &files, &result.errors);
        } else {
            files.append({name, data});
        }
    }

    QDir dir(userMappingsDir);
    if (!dir.mkpath(QStringLiteral("."))) {
        result.errors.append(QObject::tr("Cannot create the user mapping folder %1")
                        .arg(userMappingsDir));
        return result;
    }

    for (auto& file : files) {
        const bool xml = isMappingXml(file.data);
        if (xml) {
            if (!file.name.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive)) {
                file.name = mappingFileNameFromContent(file.data);
            }
        } else if (file.name.endsWith(QStringLiteral(".xml"), Qt::CaseInsensitive)) {
            result.errors.append(QObject::tr("%1: not a Mixxx controller mapping")
                            .arg(file.name));
            continue;
        } else if (!file.name.endsWith(QStringLiteral(".js"), Qt::CaseInsensitive)) {
            result.errors.append(QObject::tr("%1: not a mapping (.xml), script (.js) "
                                             "or ZIP file")
                            .arg(file.name));
            continue;
        }

        const QString targetPath = dir.filePath(file.name);
        QFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
                target.write(file.data) != file.data.size()) {
            result.errors.append(QObject::tr("%1: cannot be written to %2")
                            .arg(file.name, userMappingsDir));
            continue;
        }
        if (xml) {
            result.mappings.append(file.name);
            if (result.firstMappingPath.isEmpty()) {
                result.firstMappingPath = targetPath;
            }
        } else {
            result.scripts.append(file.name);
        }
    }
    return result;
}

QString importMappingFilesInteractive(QWidget* pParent, const QString& userMappingsDir) {
#ifdef Q_OS_ANDROID
    // The Android document picker greys out files that do not match the
    // MIME types derived from a name filter, and .js / .xml have no
    // reliable MIME type there
    const QString filter;
#else
    const QString filter = QObject::tr("Controller mappings (*.xml *.js *.zip)");
#endif
    const QStringList sourcePaths = QFileDialog::getOpenFileNames(pParent,
            QObject::tr("Import controller mapping"),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
            filter);
    if (sourcePaths.isEmpty()) {
        return QString();
    }

    const MappingImportResult result = importMappingFiles(sourcePaths, userMappingsDir);

    QString text;
    if (!result.mappings.isEmpty()) {
        text += QObject::tr("Imported mappings:") + QStringLiteral("<br>") +
                result.mappings.join(QStringLiteral("<br>")) + QStringLiteral("<br><br>");
    }
    if (!result.scripts.isEmpty()) {
        text += QObject::tr("Imported scripts:") + QStringLiteral("<br>") +
                result.scripts.join(QStringLiteral("<br>")) + QStringLiteral("<br><br>");
    }
    if (!result.errors.isEmpty()) {
        text += QObject::tr("Not imported:") + QStringLiteral("<br>") +
                result.errors.join(QStringLiteral("<br>")) + QStringLiteral("<br><br>");
    }
    if (result.mappings.isEmpty() && result.scripts.isEmpty()) {
        QMessageBox::warning(pParent, QObject::tr("Import controller mapping"), text);
        return QString();
    }
    if (result.mappings.isEmpty()) {
        text += QObject::tr("No mapping (.xml) file was imported. Import the mapping "
                            "together with its scripts.");
    } else {
        text += QObject::tr("Select the mapping in \"Load Mapping\" of your "
                            "controller and press Apply or OK.");
    }
    QMessageBox::information(pParent, QObject::tr("Import controller mapping"), text);
    return result.firstMappingPath;
}

} // namespace controllers

} // namespace mixxx
