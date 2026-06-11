#include "translator.h"

#include "launcheroptions.h"

#include <QDebug>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QString>
#include <QTextDocument>

Translator::Translator(QObject *parent)
    : QTranslator(parent) {
    translations = QMap<QString, QString>();
}

bool Translator::loadLanguage(const QString &languageCode)
{
    // Get string and make ready for file loading
    // All translation files should be lowercase with dashes isntead of underscores
    QString languageCodeString = languageCode;
    languageCodeString.replace('_', '-');
    languageCodeString = languageCodeString.toLower();

    // Make sure language code string is not empty
    if (languageCodeString.isEmpty()) {
        return false;
    }

    // Translation filepath in resources
    return Translator::loadPoFile(QString(":/i18n/i18n/translations_%1.po").arg(languageCodeString));
}

bool Translator::loadPoFile(const QString &poFilePath)
{
    // Open translation file
    QFile file(poFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open translation file:" << poFilePath;
        return false;
    }

    // Clear current translations
    translations.clear();

    // Variables
    QTextStream in(&file);
    QString line;
    QString msgctxt;
    QString msgid;
    QString msgstr;
    bool inMsgid = false;
    bool inMsgstr = false;
    int translationsLoaded = 0;

    // Loop trough PO file
    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        if (
            (line.startsWith("msgctxt") || line.startsWith("msgid")) &&
            !msgid.isEmpty() && !msgstr.isEmpty()
        ) {
            if(msgctxt.isEmpty() == false){
                msgid.prepend(msgctxt + (msgctxt.endsWith('|') ? "" : "|"));
            }
            translations[msgid] = msgstr;
            translationsLoaded++;
            msgctxt.clear();
            msgid.clear();
            msgstr.clear();
        }

        if (line.startsWith("msgctxt")) {
            msgctxt = line.mid(9).chopped(1);
        } else if (line.startsWith("msgid")) {
            msgid = line.mid(7).chopped(1);
            inMsgid = true;
            inMsgstr = false;
        } else if (line.startsWith("msgstr")) {
            msgstr = line.mid(8).chopped(1);
            inMsgid = false;
            inMsgstr = true;
        } else if (inMsgid && line.startsWith("\"")) {
            msgid += line.mid(1).chopped(1);
        } else if (inMsgstr && line.startsWith("\"")) {
            msgstr += line.mid(1).chopped(1);
        }
    }

    // Close last open translation
    if (!msgid.isEmpty() && !msgstr.isEmpty()) {

        // Add disambiguation
        if(msgctxt.isEmpty() == false){
            msgid.prepend(msgctxt + (msgctxt.endsWith('|') ? "" : "|"));
        }

        // Remember translation
        translations[msgid] = msgstr;
        translationsLoaded++;
    }

    // Fix translations that contain newlines
    for (const QString &msgIdString : translations.keys()) {
        if (msgIdString.contains("\\n")) {
            QString newMsgId = msgIdString;
            newMsgId.replace("\\n", "\n");
            translations[newMsgId] = translations.value(msgIdString).replace("\\n", "\n");
            translationsLoaded++;
        }
    }

    // Done!
    qInfo() << "Translations loaded:" << translationsLoaded;
    return true;
}

QString Translator::translate(const char *context, const char *sourceText, const char *disambiguation, int n) const {

    // Get strings from chars
    QString contextString(context);
    QString sourceString(sourceText);
    QString disambiguationString(disambiguation);

    // Get source ID
    QString sourceId(sourceString);
    if(disambiguationString.isEmpty() == false){
        sourceId.prepend(disambiguationString + (disambiguationString.endsWith('|') ? "" : "|"));
    }
    if(contextString.isEmpty() == false){
        sourceId.prepend(contextString + (contextString.endsWith('|') ? "" : "|"));
    }

    // Check if this source ID has a translation
    if (translations.contains(sourceId)) {
        QString output(translations.value(sourceId));
        if (output.isEmpty() == false) {
            return output;
        }
    }

    // Check if this source ID has a translation that is HTML escaped
    if (translations.contains(sourceId.toHtmlEscaped())) {
        QString output(translations.value(sourceId.toHtmlEscaped()));
        if (output.isEmpty() == false) {
            QTextDocument outputDoc;
            outputDoc.setHtml(output);
            return outputDoc.toPlainText();
        }
    }

    // Check if the source string itself has a translation
    if (translations.contains(sourceString)) {
        QString output(translations.value(sourceString));
        if (output.isEmpty() == false) {
            return output;
        }
    }

    // Check if the source string itself has a translation that is HTML escaped
    if (translations.contains(sourceString.toHtmlEscaped())) {
        QString output(translations.value(sourceString.toHtmlEscaped()));
        if (output.isEmpty() == false) {
            QTextDocument outputDoc;
            outputDoc.setHtml(output);
            return outputDoc.toPlainText();
        }
    }

    // Log missing translation
    if (LauncherOptions::isSet("log-missing-translations") &&
        QRegularExpression("^Q[A-Z][a-zA-Z]*\\|").match(sourceId).hasMatch() == false
    ) {
        qWarning() << "Translation not found:" << sourceId;
    }

    // Log missing Qt core translation
    if (LauncherOptions::isSet("log-missing-qt-translations") &&
        QRegularExpression("^Q[A-Z][a-zA-Z]*\\|").match(sourceId).hasMatch()
    ) {
        qWarning() << "Qt translation not found:" << sourceId;
    }

    return sourceString;
}
