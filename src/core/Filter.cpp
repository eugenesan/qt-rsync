#include "core/Filter.h"

#include <QRegularExpression>

namespace qt_rsync::core {

QVector<FilterRule> filterTextToRules(const QString&) { return {}; }
QString filterArguments(const QVector<FilterRule>&) { return {}; }
bool isValidRule(const QString&) { return false; }

QVector<FilterPhrase> phrasesFor(const QString&, bool) { return {}; }
QVector<FilterPhrase> deletePhrasesFor(const QString&, bool) { return {}; }

QString cleanQtNoise(const QString& text) { return text; }

}  // namespace qt_rsync::core
