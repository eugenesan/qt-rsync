#include "core/Command.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace qt_rsync::core {
namespace {

const char* outFormat() {
  return "%i|qpy-rsync|%l|qpy-rsync|%M|qpy-rsync|%n%L";
}

bool hasRemoteEndpoint(const SyncProfile& profile) {
  return profile.source.kind == EndpointKind::Ssh ||
         profile.destination.kind == EndpointKind::Ssh;
}

QStringList commonArguments(const SyncProfile& profile, const QString& filterFile) {
  QStringList args;
  const PreservationOptions& preserve = profile.preservation;
  const TransferOptions& transfer = profile.transfer;

  if (preserve.archive) args << QStringLiteral("--archive");
  if (preserve.hardLinks) args << QStringLiteral("--hard-links");
  if (preserve.acls) args << QStringLiteral("--acls");
  if (preserve.xattrs) args << QStringLiteral("--xattrs");
  if (preserve.numericIds) args << QStringLiteral("--numeric-ids");

  if (transfer.copyLinks) args << QStringLiteral("--copy-links");
  if (transfer.wholeFile) args << QStringLiteral("--whole-file");
  if (transfer.relative) args << QStringLiteral("--relative");
  args << (transfer.compress ? QStringLiteral("--compress")
                             : QStringLiteral("--no-compress"));
  if (transfer.checksum) args << QStringLiteral("--checksum");
  if (!transfer.partialDir.isEmpty()) {
    args << (QStringLiteral("--partial-dir=") + transfer.partialDir);
    args << QStringLiteral("--partial");
  }
  if (transfer.inplace) args << QStringLiteral("--inplace");
  if (transfer.oneFileSystem) args << QStringLiteral("--one-file-system");
  if (transfer.sparse) args << QStringLiteral("--sparse");
  if (transfer.ignoreTimes) args << QStringLiteral("--ignore-times");
  if (transfer.fuzzy) args << QStringLiteral("--fuzzy");

  if (transfer.mirrorEnabled()) {
    const DeleteTiming timing = transfer.fuzzy ? DeleteTiming::After
                                                : transfer.deleteTiming;
    const char* value = "during";
    switch (timing) {
      case DeleteTiming::Before: value = "before"; break;
      case DeleteTiming::During: value = "during"; break;
      case DeleteTiming::Delay: value = "delay"; break;
      case DeleteTiming::After: value = "after"; break;
    }
    args << (QStringLiteral("--delete-") + QLatin1String(value));
  }
  if (transfer.newOnlyEnabled()) args << QStringLiteral("--ignore-existing");

  if (profile.backup.enabled()) {
    args << QStringLiteral("--backup");
    args << (QStringLiteral("--backup-dir=") +
             destinationBackupDirectory(profile));
  }
  args << backupProtectionArguments(profile);
  if (!filterFile.isEmpty()) {
    args << (QStringLiteral("--filter=merge ") + filterFile);
  }
  args << filterArguments(profile.filters);
  return args;
}

QHash<QString, QString> processEnvironment(const SyncProfile& profile) {
  QHash<QString, QString> environment;
  if (!hasRemoteEndpoint(profile)) return environment;

  const QProcessEnvironment host = QProcessEnvironment::systemEnvironment();
  QString askpass = host.value(QStringLiteral("SSH_ASKPASS"));
  if (askpass.isEmpty()) {
    for (const char* candidate : {"ksshaskpass", "ssh-askpass",
                                  "ssh-askpass-gnome", "gnome-ssh-askpass"}) {
      const QString path = QStandardPaths::findExecutable(
          QLatin1String(candidate));
      if (!path.isEmpty()) {
        askpass = path;
        break;
      }
    }
  }
  if (!askpass.isEmpty()) {
    environment.insert(QStringLiteral("SSH_ASKPASS"), askpass);
    environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
  }
  for (const char* name : {"DISPLAY", "XAUTHORITY", "WAYLAND_DISPLAY",
                           "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS",
                           "DESKTOP_STARTUP_ID"}) {
    const QString key = QLatin1String(name);
    const QString value = host.value(key).trimmed();
    if (!value.isEmpty()) environment.insert(key, value);
  }
  return environment;
}

QString remoteShellArgument() {
  const QString display = QString::fromLocal8Bit(qgetenv("DISPLAY")).trimmed();
  if (display.isEmpty()) return QStringLiteral("ssh");
  return QStringLiteral("env DISPLAY=") + display + QStringLiteral(" ssh");
}

ProcessSpec processSpec(const SyncProfile& profile, QStringList arguments) {
  ProcessSpec spec;
  spec.environment = processEnvironment(profile);
  if (hasRemoteEndpoint(profile)) {
    spec.program = QStringLiteral("setsid");
    spec.arguments << QStringLiteral("--wait") << QStringLiteral("rsync")
                   << arguments;
  } else {
    spec.program = QStringLiteral("rsync");
    spec.arguments = arguments;
  }
  return spec;
}

QStringList syncArguments(const SyncProfile& profile, const QString& filterFile) {
  QStringList args{QStringLiteral("--itemize-changes"),
                   QStringLiteral("--info=progress2"),
                   QStringLiteral("--stats"),
                   QStringLiteral("--out-format=") + QLatin1String(outFormat())};
  if (hasRemoteEndpoint(profile)) {
    args << QStringLiteral("-e") << remoteShellArgument();
  }
  args << commonArguments(profile, filterFile);
  args << QStringLiteral("--") << profile.source.rsyncOperand()
       << profile.destination.rsyncOperand();
  return args;
}

}  // namespace

ProcessSpec buildFetchSpec(const SyncProfile& profile, const Endpoint& endpoint,
                           const QString& remotePath, const QString& localPath) {
  QStringList args{QStringLiteral("--archive"), QStringLiteral("--no-compress")};
  if (hasRemoteEndpoint(profile)) {
    args << QStringLiteral("-e") << remoteShellArgument();
  }
  QString operand = endpoint.rsyncOperand();
  while (operand.endsWith('/')) operand.chop(1);
  QString relative = remotePath;
  while (relative.startsWith('/')) relative.remove(0, 1);
  args << QStringLiteral("--") << operand + QLatin1Char('/') + relative
       << localPath;
  return processSpec(profile, args);
}

QStringList ProcessSpec::argv() const {
  QStringList result{program};
  result << arguments;
  return result;
}

ProcessSpec buildCompareSpec(const SyncProfile& profile, const QString& filterFile) {
  QStringList args{QStringLiteral("--dry-run"),
                   QStringLiteral("--itemize-changes"),
                   QStringLiteral("--info=progress2"),
                   QStringLiteral("--out-format=") + QLatin1String(outFormat())};
  if (hasRemoteEndpoint(profile)) {
    args << QStringLiteral("-e") << remoteShellArgument();
  }
  args << commonArguments(profile, filterFile);
  // A one-way preview inventories destination-only objects even when the
  // execution profile keeps them. Dry-run makes this safe.
  if (!profile.transfer.mirrorEnabled()) args << QStringLiteral("--delete-during");
  args << QStringLiteral("--") << profile.source.rsyncOperand()
       << profile.destination.rsyncOperand();
  return processSpec(profile, args);
}

ProcessSpec buildVerifySpec(const SyncProfile& profile, const QString& filterFile) {
  return buildCompareSpec(profile, filterFile);
}

ProcessSpec buildSyncSpec(const SyncProfile& profile, const QString& filterFile) {
  return processSpec(profile, syncArguments(profile, filterFile));
}

ProcessSpec buildInventorySpec(const SyncProfile& profile, const Endpoint& endpoint) {
  QString operand = endpoint.rsyncOperand();
  if (!operand.endsWith('/')) operand += '/';
  QStringList args{QStringLiteral("-r"), QStringLiteral("--list-only")};
  if (hasRemoteEndpoint(profile)) {
    args << QStringLiteral("-e") << remoteShellArgument();
  }
  args << filterArguments(profile.filters);
  args << QStringLiteral("--") << operand;
  return processSpec(profile, args);
}

QStringList buildSyncArgv(const SyncProfile& profile, const QString& filterFile) {
  return processSpec(profile, syncArguments(profile, filterFile)).argv();
}

QString explainRsyncExit(int code) {
  switch (code) {
    case 1: return QStringLiteral("rsync reported a syntax or usage error.");
    case 2: return QStringLiteral("rsync reported a protocol incompatibility.");
    case 3: return QStringLiteral("rsync reported file-selection errors.");
    case 5: return QStringLiteral("rsync could not start the client-server protocol.");
    case 10: return QStringLiteral("rsync reported a socket I/O error.");
    case 11: return QStringLiteral("rsync reported a file I/O error.");
    case 12: return QStringLiteral("rsync reported a protocol data-stream error.");
    case 20: return QStringLiteral("rsync received SIGUSR1 or SIGINT.");
    case 23: return QStringLiteral("rsync completed with a partial transfer due to an error.");
    case 24: return QStringLiteral("rsync completed with vanished source files.");
    case 30: return QStringLiteral("rsync timed out while sending or receiving data.");
    default: return QStringLiteral("rsync exited with status %1.").arg(code);
  }
}

}  // namespace qt_rsync::core
