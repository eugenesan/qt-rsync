#include "core/Storage.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QThread>

#include <cerrno>
#include <signal.h>
#include <unistd.h>

#include "core/Filter.h"
#include "core/Paths.h"

namespace qt_rsync::core::storage {
namespace {

QString endpointKindName(EndpointKind kind) {
  return kind == EndpointKind::Local ? QStringLiteral("local")
                                     : QStringLiteral("ssh");
}

EndpointKind endpointKindFrom(const QString& value) {
  return value == QLatin1String("ssh") ? EndpointKind::Ssh : EndpointKind::Local;
}

QString modeName(SyncMode mode) {
  switch (mode) {
    case SyncMode::Mirror: return QStringLiteral("mirror");
    case SyncMode::NewOnly: return QStringLiteral("new-only");
    default: return QStringLiteral("update");
  }
}

SyncMode modeFrom(const QString& value) {
  if (value == QLatin1String("mirror")) return SyncMode::Mirror;
  if (value == QLatin1String("new-only") || value == QLatin1String("new_only")) {
    return SyncMode::NewOnly;
  }
  return SyncMode::Update;
}

QString timingName(DeleteTiming timing) {
  switch (timing) {
    case DeleteTiming::Before: return QStringLiteral("before");
    case DeleteTiming::Delay: return QStringLiteral("delay");
    case DeleteTiming::After: return QStringLiteral("after");
    default: return QStringLiteral("during");
  }
}

DeleteTiming timingFrom(const QString& value) {
  if (value == QLatin1String("before")) return DeleteTiming::Before;
  if (value == QLatin1String("delay")) return DeleteTiming::Delay;
  if (value == QLatin1String("after")) return DeleteTiming::After;
  return DeleteTiming::During;
}

QString conflictName(ConflictPolicy policy) {
  switch (policy) {
    case ConflictPolicy::Skip: return QStringLiteral("skip");
    case ConflictPolicy::Unselect: return QStringLiteral("unselect");
    default: return QStringLiteral("override");
  }
}

ConflictPolicy conflictFrom(const QString& value) {
  if (value == QLatin1String("skip")) return ConflictPolicy::Skip;
  if (value == QLatin1String("unselect")) return ConflictPolicy::Unselect;
  return ConflictPolicy::Override;
}

QJsonObject endpointJson(const Endpoint& endpoint) {
  QJsonObject object;
  object.insert(QStringLiteral("kind"), endpointKindName(endpoint.kind));
  object.insert(QStringLiteral("path"), endpoint.path);
  if (!endpoint.host.isEmpty()) object.insert(QStringLiteral("host"), endpoint.host);
  if (!endpoint.user.isEmpty()) object.insert(QStringLiteral("user"), endpoint.user);
  return object;
}

Endpoint endpointFrom(const QJsonObject& object) {
  Endpoint endpoint;
  endpoint.kind = endpointKindFrom(object.value(QStringLiteral("kind")).toString());
  endpoint.path = object.value(QStringLiteral("path")).toString();
  endpoint.host = object.value(QStringLiteral("host")).toString();
  endpoint.user = object.value(QStringLiteral("user")).toString();
  return endpoint;
}

}  // namespace

FileLock::FileLock(QString path, int timeoutMs)
    : path_(std::move(path)), timeoutMs_(timeoutMs) {}

QString FileLock::lockPath() const { return path_ + QStringLiteral(".lock"); }

qint64 readLockPid(const QString& path) {
  QFile file(path + QStringLiteral(".lock"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return 0;
  return file.readAll().trimmed().toLongLong();
}

bool pidAlive(qint64 pid) {
  if (pid <= 0 || pid == static_cast<qint64>(::getpid())) return false;
  if (::kill(static_cast<pid_t>(pid), 0) == 0) return true;
  return errno == EPERM;
}

bool FileLock::acquire() {
  if (owned_) return true;
  QFileInfo(path_).dir().mkpath(QStringLiteral("."));
  const QFileInfo info(path_);
  info.absoluteDir().mkpath(QStringLiteral("."));
  const QString name = lockPath();
  qint64 deadline = 0;
  if (timeoutMs_ > 0) {
    deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs_;
  }
  while (true) {
    QFile file(name);
    if (file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
      file.write(QByteArray::number(static_cast<qint64>(::getpid())));
      file.flush();
      file.close();
      owned_ = true;
      return true;
    }
    const qint64 pid = readLockPid(path_);
    if (pid <= 0 || !pidAlive(pid)) {
      QFile::remove(name);
      continue;
    }
    if (timeoutMs_ <= 0 ||
        (deadline > 0 && QDateTime::currentMSecsSinceEpoch() >= deadline)) {
      return false;
    }
    QThread::msleep(50);
  }
}

void FileLock::release() {
  if (!owned_) return;
  if (readLockPid(path_) == static_cast<qint64>(::getpid())) {
    QFile::remove(lockPath());
  }
  owned_ = false;
}

void removeObsoleteEncodedSettings() {
  Settings settings;
  for (const QString& key : {QStringLiteral("tools/external"),
                             QStringLiteral("colours/actions"),
                             QStringLiteral("main-window/state"),
                             QStringLiteral("comparison/flat-splitter"),
                             QStringLiteral("comparison/flat-source-header"),
                             QStringLiteral("comparison/flat-destination-header"),
                             QStringLiteral("command-preview/splitter")}) {
    settings.remove(key);
  }
  settings.sync();
}

QStringList actionColourKeys() {
  return {QStringLiteral("create"), QStringLiteral("update_content"),
          QStringLiteral("update_metadata"), QStringLiteral("delete"),
          QStringLiteral("destination_only")};
}

QString encodeIntList(const QList<int>& values) {
  QStringList parts;
  for (int value : values) parts.append(QString::number(value));
  return parts.join(QLatin1Char(','));
}

QList<int> decodeIntList(const QString& text) {
  QList<int> values;
  for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
    bool ok = false;
    const int value = part.trimmed().toInt(&ok);
    if (ok) values.append(value);
  }
  return values;
}

Settings::Settings()
    : state_(paths::settingsPath(), QSettings::IniFormat),
      config_(paths::configSettingsPath(), QSettings::IniFormat) {}

bool Settings::isConfigKey(const QString& key) {
  static const QStringList prefixes{
      QStringLiteral("logging/"), QStringLiteral("sound/"),
      QStringLiteral("verification/"), QStringLiteral("decisions/"),
      QStringLiteral("advanced/"), QStringLiteral("tools/"),
      QStringLiteral("colours/"), QStringLiteral("comparison/backup"),
      QStringLiteral("comparison/filter-directories"),
      QStringLiteral("profile/current_path"),
  };
  for (const QString& prefix : prefixes) {
    if (key.startsWith(prefix)) return true;
  }
  return false;
}

QVariant Settings::value(const QString& key, const QVariant& fallback) const {
  if (isConfigKey(key)) return config_.value(key, fallback);
  return state_.value(key, fallback);
}

void Settings::setValue(const QString& key, const QVariant& value) {
  if (isConfigKey(key)) {
    config_.setValue(key, value);
  } else {
    state_.setValue(key, value);
  }
}

void Settings::remove(const QString& key) {
  if (isConfigKey(key)) {
    config_.remove(key);
  } else {
    state_.remove(key);
  }
}

void Settings::sync() {
  state_.sync();
  config_.sync();
}

void Settings::clear() {
  state_.clear();
  config_.clear();
}

QStringList Settings::allKeys() const {
  QStringList keys = state_.allKeys();
  keys.append(config_.allKeys());
  keys.removeDuplicates();
  return keys;
}

QString Settings::statePath() const { return state_.fileName(); }
QString Settings::configPath() const { return config_.fileName(); }

QString profileToJson(const SyncProfile& profile) {
  QJsonObject root;
  root.insert(QStringLiteral("name"), profile.name);
  root.insert(QStringLiteral("description"), profile.description);
  root.insert(QStringLiteral("profile_id"), profile.id);
  root.insert(QStringLiteral("schema_version"), profile.schemaVersion);
  root.insert(QStringLiteral("conflict_policy"), conflictName(profile.conflict));
  root.insert(QStringLiteral("source"), endpointJson(profile.source));
  root.insert(QStringLiteral("destination"), endpointJson(profile.destination));

  QJsonObject preservation;
  preservation.insert(QStringLiteral("archive"), profile.preservation.archive);
  preservation.insert(QStringLiteral("hard_links"), profile.preservation.hardLinks);
  preservation.insert(QStringLiteral("acls"), profile.preservation.acls);
  preservation.insert(QStringLiteral("xattrs"), profile.preservation.xattrs);
  preservation.insert(QStringLiteral("numeric_ids"), profile.preservation.numericIds);
  root.insert(QStringLiteral("preservation"), preservation);

  QJsonObject transfer;
  transfer.insert(QStringLiteral("sync_mode"), modeName(profile.transfer.mode));
  transfer.insert(QStringLiteral("delete_timing"),
                  timingName(profile.transfer.deleteTiming));
  transfer.insert(QStringLiteral("delete"), false);
  transfer.insert(QStringLiteral("fuzzy"), profile.transfer.fuzzy);
  transfer.insert(QStringLiteral("copy_links"), profile.transfer.copyLinks);
  transfer.insert(QStringLiteral("whole_file"), profile.transfer.wholeFile);
  transfer.insert(QStringLiteral("relative"), profile.transfer.relative);
  transfer.insert(QStringLiteral("compress"), profile.transfer.compress);
  transfer.insert(QStringLiteral("checksum"), profile.transfer.checksum);
  transfer.insert(QStringLiteral("partial_dir"), profile.transfer.partialDir);
  transfer.insert(QStringLiteral("inplace"), profile.transfer.inplace);
  transfer.insert(QStringLiteral("one_file_system"), profile.transfer.oneFileSystem);
  transfer.insert(QStringLiteral("sparse"), profile.transfer.sparse);
  transfer.insert(QStringLiteral("ignore_times"), profile.transfer.ignoreTimes);
  root.insert(QStringLiteral("transfer"), transfer);

  QJsonObject backup;
  backup.insert(QStringLiteral("directory"), profile.backup.directory);
  backup.insert(QStringLiteral("protect_nested"), profile.backup.protectNested);
  root.insert(QStringLiteral("backup"), backup);

  QJsonArray filterLines;
  for (const QString& line : profile.filterText.split('\n')) {
    filterLines.append(line);
  }
  root.insert(QStringLiteral("filter_lines"), filterLines);
  root.insert(QStringLiteral("inventory_after_compare"),
              profile.inventoryAfterCompare);

  QJsonDocument document(root);
  return QString::fromUtf8(document.toJson(QJsonDocument::Indented));
}

SyncProfile profileFromJson(const QString& payload) {
  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(payload.toUtf8(), &error);
  SyncProfile profile;
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return profile;
  }
  const QJsonObject root = document.object();
  profile.name = root.value(QStringLiteral("name")).toString();
  profile.description = root.value(QStringLiteral("description")).toString();
  profile.id = root.value(QStringLiteral("profile_id")).toString();
  profile.schemaVersion = root.value(QStringLiteral("schema_version")).toInt(3);
  profile.conflict = conflictFrom(
      root.value(QStringLiteral("conflict_policy")).toString());
  profile.source = endpointFrom(root.value(QStringLiteral("source")).toObject());
  profile.destination =
      endpointFrom(root.value(QStringLiteral("destination")).toObject());

  const QJsonObject preservation = root.value(QStringLiteral("preservation")).toObject();
  profile.preservation.archive =
      preservation.value(QStringLiteral("archive")).toBool(true);
  profile.preservation.hardLinks =
      preservation.value(QStringLiteral("hard_links")).toBool();
  profile.preservation.acls = preservation.value(QStringLiteral("acls")).toBool();
  profile.preservation.xattrs =
      preservation.value(QStringLiteral("xattrs")).toBool();
  profile.preservation.numericIds =
      preservation.value(QStringLiteral("numeric_ids")).toBool();

  const QJsonObject transfer = root.value(QStringLiteral("transfer")).toObject();
  const bool legacyDelete = transfer.value(QStringLiteral("delete")).toBool();
  profile.transfer.mode = legacyDelete
                              ? SyncMode::Mirror
                              : modeFrom(transfer.value(QStringLiteral("sync_mode")).toString());
  profile.transfer.deleteTiming =
      timingFrom(transfer.value(QStringLiteral("delete_timing")).toString());
  profile.transfer.fuzzy = transfer.value(QStringLiteral("fuzzy")).toBool();
  profile.transfer.copyLinks = transfer.value(QStringLiteral("copy_links")).toBool();
  profile.transfer.wholeFile = transfer.value(QStringLiteral("whole_file")).toBool();
  profile.transfer.relative = transfer.value(QStringLiteral("relative")).toBool();
  profile.transfer.compress = transfer.value(QStringLiteral("compress")).toBool();
  profile.transfer.checksum = transfer.value(QStringLiteral("checksum")).toBool();
  profile.transfer.partialDir =
      transfer.value(QStringLiteral("partial_dir")).toString();
  profile.transfer.inplace = transfer.value(QStringLiteral("inplace")).toBool();
  profile.transfer.oneFileSystem =
      transfer.value(QStringLiteral("one_file_system")).toBool();
  profile.transfer.sparse = transfer.value(QStringLiteral("sparse")).toBool();
  profile.transfer.ignoreTimes =
      transfer.value(QStringLiteral("ignore_times")).toBool();

  const QJsonObject backup = root.value(QStringLiteral("backup")).toObject();
  profile.backup.directory = backup.value(QStringLiteral("directory")).toString();
  if (backup.contains(QStringLiteral("mode")) &&
      backup.value(QStringLiteral("mode")).toString() == QLatin1String("none")) {
    profile.backup.directory.clear();
  }
  profile.backup.protectNested =
      backup.value(QStringLiteral("protect_nested")).toBool(true);

  QStringList lines;
  const QJsonArray filterLines = root.value(QStringLiteral("filter_lines")).toArray();
  for (const QJsonValue& value : filterLines) lines << value.toString();
  profile.filterText = lines.join('\n');
  profile.filters = filterTextToRules(profile.filterText);
  profile.inventoryAfterCompare =
      root.value(QStringLiteral("inventory_after_compare")).toBool();
  return profile;
}

void saveProfile(const QString& path, const SyncProfile& profile) {
  const QFileInfo info(path);
  info.absoluteDir().mkpath(QStringLiteral("."));
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  file.write(profileToJson(profile).toUtf8());
  file.commit();
}

SyncProfile loadProfile(const QString& path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
  return profileFromJson(QString::fromUtf8(file.readAll()));
}

QStringList listProfiles() {
  QDir directory(paths::profilesDirectory());
  QStringList names = directory.entryList({QStringLiteral("*.json")},
                                          QDir::Files, QDir::Name);
  return names;
}

QString currentProfilePath() {
  Settings settings;
  return settings.value(QStringLiteral("profile/current_path")).toString();
}

void setCurrentProfilePath(const QString& path) {
  Settings settings;
  settings.setValue(QStringLiteral("profile/current_path"), path);
  settings.sync();
}

}  // namespace qt_rsync::core::storage
