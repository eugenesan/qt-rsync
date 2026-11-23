#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

namespace qt_rsync::core {

enum class RuleType { Include, Exclude, Advanced };

struct FilterRule {
  QString rule;
  RuleType type = RuleType::Advanced;
  bool enabled = true;
  QString comment;
};

struct FilterPhrase {
  QString label;
  QString pattern;
  RuleType type = RuleType::Exclude;
};

struct ParsedAdvancedRule {
  QString token;
  QString pattern;
};

// rsync rule parser (long names, short operators + modifiers, comma modifiers,
// and the pattern-less `clear`/`!` rules).
std::optional<ParsedAdvancedRule> parseAdvancedRule(const QString& value);
bool isAdvancedRule(const QString& value);
bool validateRule(const FilterRule& rule);

QVector<FilterRule> filterTextToRules(const QString& text);
QStringList filterArguments(const QVector<FilterRule>& rules);
QStringList filterFileLines(const QVector<FilterRule>& rules);
QVector<FilterRule> importFilterFile(const QString& text);
QString exportFilterFile(const QVector<FilterRule>& rules);

QString extensionOf(const QString& path);
QVector<FilterPhrase> phrasesFor(const QString& path, bool isDirectory);
QVector<FilterPhrase> deletePhrasesFor(const QString& path, bool isDirectory);
bool patternMatches(const QString& pattern, const QString& path);

QString cleanQtNoise(const QString& text);

}  // namespace qt_rsync::core
