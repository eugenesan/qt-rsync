#pragma once

#include <QString>

#include <cstdint>
#include <optional>

namespace qt_rsync::core {

enum class Action {
  Create,
  UpdateContent,
  UpdateMetadata,
  Delete,
  DestinationOnly,
  NoChange,
  Message,
};

enum class Kind { File, Directory, Symlink, Device, Special, Unknown };

struct Change {
  QString path;
  Action action = Action::NoChange;
  Kind kind = Kind::File;
  QString itemizedCode;
  std::optional<std::int64_t> size;
  QString modifiedTime;
  QString linkTarget;
  QString reason;
  std::optional<std::int64_t> destinationSize;
  QString destinationTime;

  bool isDeletion() const { return action == Action::Delete; }
  bool isDestinationOnly() const { return action == Action::DestinationOnly; }
};

}  // namespace qt_rsync::core
