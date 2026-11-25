//
// C++ Implementation: parserm3u
//
// Description: module to parse m3u(plaintext) formatted playlists
//
//
// Author: Ingo Kossyk <kossyki@cs.tu-berlin.de>, (C) 2004
// Author: Tobias Rafreider trafreider@mixxx.org, (C) 2011
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "library/parserm3u.h"

#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>
#include <QUrl>
#include <QtDebug>
#include <algorithm>
#include <array>

#include "errordialoghandler.h"

namespace {
// according to http://en.wikipedia.org/wiki/M3U the default encoding of m3u is Windows-1252
// see also http://tools.ietf.org/html/draft-pantos-http-live-streaming-07

/// Unicode code points of the upper half of the Windows-1250 code page, the
/// standard encoding of .m3u files. Qt only provides this codec in Qt5Compat
/// or if it has been built with ICU.
constexpr std::array<char16_t, 128> kWindows1250 = {
        0x20AC, 0x0081, 0x201A, 0x0083, 0x201E, 0x2026, 0x2020, 0x2021,
        0x0088, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x0098, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
        0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
        0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
        0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
        0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
        0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
        0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
        0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
        0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
        0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
        0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
        0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
        0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

QString fromWindows1250(const QByteArray& bytes) {
    QString result;
    result.reserve(bytes.size());
    for (const char c : bytes) {
        const auto byte = static_cast<unsigned char>(c);
        result.append(byte < 0x80 ? QChar(byte) : QChar(kWindows1250[byte - 0x80]));
    }
    return result;
}

/// Characters that are not available in the code page are replaced by '?'
QByteArray toWindows1250(const QString& string) {
    QByteArray result;
    result.reserve(string.size());
    for (const QChar c : string) {
        if (c.unicode() < 0x80) {
            result.append(static_cast<char>(c.unicode()));
            continue;
        }
        const auto it = std::find(
                kWindows1250.cbegin(), kWindows1250.cend(), c.unicode());
        result.append(it == kWindows1250.cend()
                        ? '?'
                        : static_cast<char>(0x80 + (it - kWindows1250.cbegin())));
    }
    return result;
}
const char kM3uHeader[] = "#EXTM3U";
const char kM3uCommentPrefix[] = "#";
// Note: The RegEx pattern is compiled, when first used the first time
const auto kUniveralEndOfLineRegEx = QRegularExpression(QStringLiteral("\r\n|\r|\n"));
} // anonymous namespace

/**
   ToDo:
    - parse ALL information from the pls file if available ,
          not only the filepath;

          Userinformation :
          The M3U format is just a headerless plaintext format
          where every line of text either represents
          a file location or a comment. comments are being
          preceded by a '#'. This parser will try to parse all
          file information from the given file and add the filepaths
          to the locations ptrlist when the file is existing locally
          or on a mounted harddrive.
 **/

// static
bool ParserM3u::isPlaylistFilenameSupported(const QString& fileName) {
    return fileName.endsWith(".m3u", Qt::CaseInsensitive) ||
            fileName.endsWith(".m3u8", Qt::CaseInsensitive);
}

QList<QString> ParserM3u::parseAllLocations(const QString& playlistFile) {
    QList<QString> paths;

    QFile file(playlistFile);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning()
                << "Failed to open playlist file"
                << playlistFile
                << file.errorString();
        return paths;
    }

    QByteArray byteArray = file.readAll();
    QString fileContents;
    if (Parser::isUtf8(byteArray.constData())) {
        fileContents = QString::fromUtf8(byteArray);
    } else {
        fileContents = fromWindows1250(byteArray);
    }

    if (!fileContents.startsWith(kM3uHeader)) {
        qWarning() << "M3U playlist file" << playlistFile << "does not start with" << kM3uHeader;
    }

    const QStringList fileLines = fileContents.split(kUniveralEndOfLineRegEx);
    for (const QString& line : fileLines) {
        if (line.startsWith(kM3uCommentPrefix)) {
            // Skip lines with comments
            continue;
        }
        paths.append(line);
    }
    return paths;
}

bool ParserM3u::writeM3UFile(const QString &file_str, const QList<QString> &items, bool useRelativePath) {
    return writeM3UFile(file_str, items, useRelativePath, false);
}

bool ParserM3u::writeM3U8File(const QString &file_str, const QList<QString> &items, bool useRelativePath) {
    return writeM3UFile(file_str, items, useRelativePath, true);
}

bool ParserM3u::writeM3UFile(const QString &file_str, const QList<QString> &items, bool useRelativePath, bool useUtf8)
{
    // Important note:
    // On Windows \n will produce a <CR><CL> (=\r\n)
    // On Linux and OS X \n is <CR> (which remains \n)
    bool urlEncodingUsed = false;
    QDir baseDirectory(QFileInfo(file_str).canonicalPath());
    QString fileContents(QStringLiteral("#EXTM3U\n"));
    for (const QString& item : items) {
        fileContents += QStringLiteral("#EXTINF\n");
        if (useUtf8) {
            if (useRelativePath) {
                fileContents += baseDirectory.relativeFilePath(item) + QStringLiteral("\n");
            } else {
                fileContents += item + QStringLiteral("\n");
            }
        } else {
            const QString trackName = fromWindows1250(toWindows1250(item));
            if (trackName == item) {
                if (useRelativePath) { //Issue: URL Location is not working properly for Relative Paths
                    fileContents += baseDirectory.relativeFilePath(item) + QStringLiteral("\n");
                } else {
                    fileContents += item + QStringLiteral("\n");
                }
            } else {
                QUrl itemUrl = QUrl::fromLocalFile(item);
                QString itemUrlEncoded = itemUrl.toEncoded();
                fileContents += itemUrlEncoded + QStringLiteral("\n");
                urlEncodingUsed = true;
            }
        }
    }

    QByteArray outputByteArray;
    if (useUtf8) {
        outputByteArray = fileContents.toUtf8();
    } else {
        outputByteArray = toWindows1250(fileContents);
    }

    QFile file(file_str);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        ErrorDialogHandler* pDialogHandler = ErrorDialogHandler::instance();
        ErrorDialogProperties* props = pDialogHandler->newDialogProperties();
        props->setType(DLG_WARNING);
        props->setTitle(QObject::tr("Playlist Export Failed"));
        props->setText(QObject::tr("Could not create file") + " " + file_str);
        props->setDetails(file.errorString());
        pDialogHandler->requestErrorDialog(props);
        return false;
    }
    if (urlEncodingUsed) {
        QMessageBox::information(nullptr,
                QObject::tr("Playlist Export Has Special Characters"),
                QObject::tr("Some file paths in the playlist have special characters. "
                            "These file paths will be encoded as absolute path URLs. "
                            "Please select the m3u8 format for better and lossless exporting."));
    }
    file.write(outputByteArray);
    file.close();

    return true;
}
