#include "core/ExternalTools.h"

#include "core/Storage.h"

#include <QDir>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace qt_rsync::core {
namespace {

const QSet<QString> kKnown{QStringLiteral("a"), QStringLiteral("f"),
                           QStringLiteral("x"), QStringLiteral("b"),
                           QStringLiteral("d"), QStringLiteral("A"),
                           QStringLiteral("F"), QStringLiteral("X"),
                           QStringLiteral("B"), QStringLiteral("D"),
                           QStringLiteral("R"), QStringLiteral("U"),
                           QStringLiteral("V"), QStringLiteral("W"),
                           QStringLiteral("P"), QStringLiteral("T"),
                           QStringLiteral("O")};

void splitName(const QString& name, QString* stem, QString* extension) {
  const int dot = name.lastIndexOf(QLatin1Char('.'));
  if (dot <= 0) {
    *stem = name;
    extension->clear();
    return;
  }
  *stem = name.left(dot);
  *extension = name.mid(dot + 1);
}

}  // namespace

QVector<ExternalTool> loadExternalTools() {
  QVector<ExternalTool> tools;
  core::storage::Settings settings;
  const QByteArray payload =
      settings.value(QStringLiteral("tools/external")).toByteArray();
  if (payload.isEmpty()) return tools;
  const QJsonArray array = QJsonDocument::fromJson(payload).array();
  for (const QJsonValue& value : array) {
    const QJsonObject object = value.toObject();
    ExternalTool tool;
    tool.name = object.value(QStringLiteral("name")).toString();
    tool.command = object.value(QStringLiteral("command")).toString();
    tool.options = object.value(QStringLiteral("options")).toString();
    tool.enabled = object.value(QStringLiteral("enabled")).toBool(true);
    if (!tool.name.isEmpty() && !tool.command.isEmpty()) tools.append(tool);
  }
  return tools;
}

void saveExternalTools(const QVector<ExternalTool>& tools) {
  QJsonArray array;
  for (const ExternalTool& tool : tools) {
    QJsonObject object;
    object.insert(QStringLiteral("name"), tool.name);
    object.insert(QStringLiteral("command"), tool.command);
    object.insert(QStringLiteral("options"), tool.options);
    object.insert(QStringLiteral("enabled"), tool.enabled);
    array.append(object);
  }
  core::storage::Settings settings;
  settings.setValue(QStringLiteral("tools/external"),
                    QJsonDocument(array).toJson(QJsonDocument::Compact));
  settings.sync();
}

QString joinToolPath(const QString& root, const QString& path) {
  const QString cleanRoot = root;
  const QString cleanPath = path;
  if (cleanRoot.isEmpty()) return cleanPath;
  if (cleanPath.isEmpty()) return cleanRoot;
  const QString base = cleanRoot.endsWith(QLatin1Char('/'))
                            ? cleanRoot.chopped(1)
                            : cleanRoot;
  const QString item = cleanPath.startsWith(QLatin1Char('/'))
                           ? cleanPath.mid(1)
                           : cleanPath;
  return base + QLatin1Char('/') + item;
}

ToolContext itemContext(const QString& path, bool isDirectory,
                        const QString& pickedRoot, const QString& otherRoot,
                        const Endpoint& pickedEndpoint,
                        const Endpoint& otherEndpoint,
                        const QString& toolName, const QString& options) {
  ToolContext context;
  context.pickedFullPath = joinToolPath(pickedRoot, path);
  context.otherFullPath = joinToolPath(otherRoot, path);
  const QString name = path.split(QLatin1Char('/')).last();
  context.pickedName = name;
  context.otherName = name;
  if (isDirectory) {
    context.pickedDirectory = context.pickedFullPath;
    context.otherDirectory = context.otherFullPath;
  } else {
    context.pickedDirectory = context.pickedFullPath.section(
        QLatin1Char('/'), 0, -2);
    context.otherDirectory = context.otherFullPath.section(
        QLatin1Char('/'), 0, -2);
  }
  context.pickedEndpoint = pickedEndpoint;
  context.otherEndpoint = otherEndpoint;
  context.toolName = toolName;
  context.options = options;
  return context;
}

QHash<QString, QString> placeholderValues(const ToolContext& context) {
  QString pickedStem, pickedExtension, otherStem, otherExtension;
  splitName(context.pickedName, &pickedStem, &pickedExtension);
  splitName(context.otherName, &otherStem, &otherExtension);
  const Endpoint& endpoint = context.otherEndpoint;
  const QString host = endpoint.host;
  const QString user = endpoint.user;
  const bool remote = endpoint.kind == EndpointKind::Ssh;
  QHash<QString, QString> values;
  values.insert(QStringLiteral("a"), context.pickedFullPath);
  values.insert(QStringLiteral("f"), context.pickedName);
  values.insert(QStringLiteral("x"), pickedExtension);
  values.insert(QStringLiteral("b"), pickedStem);
  values.insert(QStringLiteral("d"), context.pickedDirectory);
  values.insert(QStringLiteral("A"), context.otherFullPath);
  values.insert(QStringLiteral("F"), context.otherName);
  values.insert(QStringLiteral("X"), otherExtension);
  values.insert(QStringLiteral("B"), otherStem);
  values.insert(QStringLiteral("D"), context.otherDirectory);
  values.insert(QStringLiteral("R"), host);
  values.insert(QStringLiteral("U"), user);
  values.insert(QStringLiteral("V"), user.isEmpty() ? QString() : QStringLiteral("@"));
  values.insert(QStringLiteral("W"), host.isEmpty() ? QString() : QStringLiteral(":"));
  values.insert(QStringLiteral("P"), remote ? QStringLiteral("://") : QString());
  values.insert(QStringLiteral("T"), context.toolName);
  values.insert(QStringLiteral("O"), context.options);
  return values;
}

QString expandPlaceholders(const QString& text,
                           const QHash<QString, QString>& values) {
  static const QRegularExpression placeholder(
      QStringLiteral("%(%|[A-Za-z])"));
  QString result;
  int position = 0;
  auto it = placeholder.globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    result += text.mid(position, match.capturedStart() - position);
    const QString key = match.captured(1);
    if (key == QStringLiteral("%")) {
      result += QStringLiteral("%");
    } else {
      result += values.value(key, match.captured(0));
    }
    position = match.capturedEnd();
  }
  result += text.mid(position);
  return result;
}

QStringList unknownPlaceholders(const QString& text) {
  static const QRegularExpression placeholder(
      QStringLiteral("%(%|[A-Za-z])"));
  QStringList unknown;
  auto it = placeholder.globalMatch(text);
  while (it.hasNext()) {
    const QRegularExpressionMatch match = it.next();
    const QString key = match.captured(1);
    if (key != QStringLiteral("%") && !kKnown.contains(key) &&
        !unknown.contains(match.captured(0))) {
      unknown.append(match.captured(0));
    }
  }
  return unknown;
}

QStringList buildToolCommand(const QString& command,
                             const QHash<QString, QString>& values,
                             QString* error) {
  const QStringList parts = QProcess::splitCommand(command);
  if (parts.isEmpty()) {
    if (error != nullptr) *error = QStringLiteral("The tool command is empty.");
    return {};
  }
  QStringList argv;
  for (const QString& part : parts) {
    const QString expanded = expandPlaceholders(part, values);
    if (expanded.isEmpty() && !part.isEmpty()) continue;
    if (part.contains(QStringLiteral("%O"))) {
      argv.append(QProcess::splitCommand(expanded));
    } else {
      argv.append(expanded);
    }
  }
  return argv;
}

}  // namespace qt_rsync::core
