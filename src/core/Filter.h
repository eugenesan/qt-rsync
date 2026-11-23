#pragma once

#include <QHash>
#include <QString>
#include <QVector>

namespace qt_rsync::core {

enum class RuleType { Include, Exclude, Advanced };

struct FilterRule {
  QString rule;
  RuleType type = RuleType::Advanced;
  bool enabled = true;
  QString comment;
};

// Patterns offered by the item context menu (extension, name, relative path).
struct FilterPhrase {
  QString label;
  QString pattern;
  RuleType type = RuleType::Exclude;
};

QVector<FilterRule> filterTextToRules(const QString& text);
QString filterArguments(const QVector<FilterRule>& rules);
bool isValidRule(const QString& rule);
QVector<FilterPhrase> phrasesFor(const QString& path, bool isDirectory);
QVector<FilterPhrase> deletePhrasesFor(const QString& path, bool isDirectory);
QString cleanQtNoise(const QString& text);

}  // namespace qt_rsync::core
