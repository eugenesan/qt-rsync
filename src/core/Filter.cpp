#include "core/Filter.h"

#include <QRegularExpression>
#include <QSet>

namespace qt_rsync::core {
namespace {

const QSet<QChar> kShortRules{QChar('+'), QChar('-'), QChar('.'), QChar(':'),
                              QChar('H'), QChar('S'), QChar('P'), QChar('R'),
                              QChar('!')};
const QSet<QString> kLongRules{"exclude", "include", "merge", "dir-merge",
                               "hide", "show", "protect", "risk", "clear"};
const QSet<QChar> kModifiers{'/', '!', 's', 'r', 'p', 'x',
                             'C', 'n', 'e', '-', '+'};

bool allModifiersValid(const QString& modifiers) {
  if (modifiers.isEmpty()) return false;
  for (const QChar character : modifiers) {
    if (!kModifiers.contains(character)) return false;
  }
  return true;
}

bool wildcardMatch(const QString& pattern, const QString& value) {
  const QString regex = QRegularExpression::wildcardToRegularExpression(pattern);
  return QRegularExpression(regex).match(value).hasMatch();
}

bool matchPart(const QString& pattern, const QString& value) {
  if (!pattern.contains('*') && !pattern.contains('?') && !pattern.contains('[')) {
    return pattern == value;
  }
  return wildcardMatch(pattern, value);
}

bool matchSegments(const QStringList& patternParts, const QStringList& pathParts) {
  if (patternParts.isEmpty()) return pathParts.isEmpty();
  const QString head = patternParts.first();
  if (head == QLatin1String("**")) {
    for (int index = 0; index <= pathParts.size(); ++index) {
      if (matchSegments(patternParts.mid(1), pathParts.mid(index))) return true;
    }
    return false;
  }
  if (pathParts.isEmpty()) return false;
  if (!matchPart(head, pathParts.first())) return false;
  return matchSegments(patternParts.mid(1), pathParts.mid(1));
}

QString trimmedPath(const QString& path) {
  QString result = path;
  while (result.startsWith('/')) result.remove(0, 1);
  while (result.endsWith('/')) result.chop(1);
  return result;
}

}  // namespace

std::optional<ParsedAdvancedRule> parseAdvancedRule(const QString& value) {
  const QString text = value.trimmed();
  static const QRegularExpression tokenPattern(
      QStringLiteral("^([^\\s_]+)(?:[ \\t_]+(.*))?$"));
  const auto match = tokenPattern.match(text);
  if (!match.hasMatch()) return std::nullopt;
  const QString token = match.captured(1);
  const QString pattern = match.captured(2);

  QString name;
  if (token.contains(',')) {
    const QStringList parts = token.split(',');
    if (parts.size() != 2 || !allModifiersValid(parts.at(1))) return std::nullopt;
    name = parts.first();
  } else if (kLongRules.contains(token)) {
    name = token;
  } else if (!token.isEmpty() && kShortRules.contains(token.at(0))) {
    const QString modifiers = token.mid(1);
    if (!modifiers.isEmpty() && !allModifiersValid(modifiers)) return std::nullopt;
    name = token.left(1);
  } else {
    return std::nullopt;
  }

  const bool knownShort = name.size() == 1 && kShortRules.contains(name.at(0));
  if (!kLongRules.contains(name) && !knownShort) return std::nullopt;
  const bool isClear = name == QLatin1String("clear") || name == QLatin1String("!");
  if (isClear) {
    if (!pattern.isNull()) return std::nullopt;
    return ParsedAdvancedRule{token, QString()};
  }
  if (pattern.isEmpty()) return std::nullopt;
  return ParsedAdvancedRule{token, pattern};
}

bool isAdvancedRule(const QString& value) {
  return parseAdvancedRule(value).has_value();
}

bool validateRule(const FilterRule& rule) {
  if (rule.rule.isEmpty() || rule.rule.contains(QChar('\0')) ||
      rule.rule.contains('\n') || rule.rule.contains('\r')) {
    return false;
  }
  if (rule.type == RuleType::Advanced) return isAdvancedRule(rule.rule);
  return true;
}

QVector<FilterRule> filterTextToRules(const QString& text) {
  QVector<FilterRule> rules;
  const QStringList lines = text.split('\n');
  for (QString line : lines) {
    const QString value = line.trimmed();
    if (value.isEmpty() || value.startsWith('#')) continue;
    FilterRule rule;
    rule.rule = isAdvancedRule(value) ? value : QStringLiteral("- ") + value;
    rule.type = RuleType::Advanced;
    rules.append(rule);
  }
  return rules;
}

QStringList filterArguments(const QVector<FilterRule>& rules) {
  QStringList arguments;
  for (const FilterRule& rule : rules) {
    if (!rule.enabled) continue;
    switch (rule.type) {
      case RuleType::Include:
        arguments << (QStringLiteral("--include=") + rule.rule);
        break;
      case RuleType::Exclude:
        arguments << (QStringLiteral("--exclude=") + rule.rule);
        break;
      case RuleType::Advanced:
        arguments << QStringLiteral("--filter") << rule.rule;
        break;
    }
  }
  return arguments;
}

QStringList filterFileLines(const QVector<FilterRule>& rules) {
  QStringList lines;
  for (const FilterRule& rule : rules) {
    if (rule.type == RuleType::Include) {
      lines << (QStringLiteral("+ ") + rule.rule);
    } else if (rule.type == RuleType::Exclude) {
      lines << (QStringLiteral("- ") + rule.rule);
    } else if (!rule.enabled && !rule.comment.isEmpty()) {
      lines << QStringLiteral("# disabled: %1: %2")
                   .arg(rule.comment, rule.rule);
    }
  }
  return lines;
}

QVector<FilterRule> importFilterFile(const QString& text) {
  QVector<FilterRule> rules;
  for (QString line : text.split('\n')) {
    const QString value = line.trimmed();
    if (value.isEmpty() || value.startsWith('#')) continue;
    rules.append(FilterRule{value, RuleType::Advanced, true, QString()});
  }
  return rules;
}

QString exportFilterFile(const QVector<FilterRule>& rules) {
  return filterFileLines(rules).join('\n');
}

QString extensionOf(const QString& path) {
  const QString name = path.trimmed().section('/', -1);
  const int dot = name.lastIndexOf('.');
  return dot < 0 ? QString() : name.mid(dot + 1);
}

QVector<FilterPhrase> phrasesFor(const QString& path, bool isDirectory) {
  QVector<FilterPhrase> phrases;
  const QString clean = trimmedPath(path);
  const QString name = clean.section('/', -1);
  const QString extension = extensionOf(clean);
  if (!extension.isEmpty()) {
    const QString pattern = "*." + extension;
    phrases.append({pattern, pattern, RuleType::Exclude});
  }
  const QString suffix = isDirectory ? QStringLiteral("/") : QString();
  if (!name.isEmpty()) {
    phrases.append({name + suffix, name + suffix, RuleType::Exclude});
  }
  if (!clean.isEmpty()) {
    const QString pattern = "/" + clean + suffix;
    phrases.append({pattern, pattern, RuleType::Exclude});
  }
  return phrases;
}

QVector<FilterPhrase> deletePhrasesFor(const QString& path, bool isDirectory) {
  QVector<FilterPhrase> result;
  for (const FilterPhrase& phrase : phrasesFor(path, isDirectory)) {
    result.append({QStringLiteral("Delete on destination: ") + phrase.label,
                   QStringLiteral("-s ") + phrase.pattern,
                   RuleType::Advanced});
  }
  return result;
}

bool patternMatches(const QString& pattern, const QString& path) {
  if (pattern.isEmpty()) return false;
  const QString clean = trimmedPath(path);
  const bool anchored = pattern.startsWith('/');
  QString body = anchored ? pattern.mid(1) : pattern;
  const bool trailing = body.endsWith('/');
  if (trailing) {
    while (body.endsWith('/')) body.chop(1);
  }
  body = trimmedPath(body);
  if (body.isEmpty()) return false;

  const QStringList patternParts = body.split('/');
  const QStringList pathParts = clean.split('/');
  if (anchored) {
    if (trailing) {
      return matchSegments(patternParts, pathParts.mid(0, patternParts.size()));
    }
    return matchSegments(patternParts, pathParts);
  }
  for (int start = 0; start < pathParts.size(); ++start) {
    const QStringList rest = pathParts.mid(start);
    if (trailing) {
      if (matchSegments(patternParts, rest.mid(0, patternParts.size()))) return true;
    } else if (matchSegments(patternParts, rest)) {
      return true;
    }
  }
  return false;
}

QString cleanQtNoise(const QString& text) {
  static const QStringList benign{
      QStringLiteral("Cannot create platform OpenGL context"),
      QStringLiteral("fallback to QtQuick software backend"),
      QStringLiteral("Socket not connected"),
      QStringLiteral("QLocalSocket::PeerClosedError"),
      QStringLiteral("Connection::send() called with connection not inited"),
  };
  QStringList kept;
  for (const QString& line : text.split('\n')) {
    bool keep = true;
    for (const QString& pattern : benign) {
      if (line.contains(pattern)) {
        keep = false;
        break;
      }
    }
    if (keep) kept << line;
  }
  return kept.join('\n');
}

}  // namespace qt_rsync::core
