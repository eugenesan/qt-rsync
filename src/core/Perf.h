#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QFile>
#include <QTime>

#include <cstdio>

namespace qt_rsync::core {

// Opt-in phase timing controlled by Settings > Advanced > "Include timing
// diagnostics" (logging/timing). "Include rsync debug diagnostics"
// (logging/debug) also enables it. With the flag off a ScopedPerf does no
// work and the normal path is unaffected.
inline bool& perfFlag() {
  static bool enabled = false;
  return enabled;
}
inline QByteArray& perfLogPath() {
  static QByteArray path;
  return path;
}
inline void setPerfEnabled(bool enabled) { perfFlag() = enabled; }
inline bool perfEnabled() { return perfFlag(); }
inline void setPerfLogPath(const QByteArray& path) { perfLogPath() = path; }

inline void perfLog(const char* phase, qint64 ms) {
  if (!perfFlag()) return;
  const QByteArray line = QTime::currentTime()
                              .toString(QStringLiteral("HH:mm:ss.zzz"))
                              .toLocal8Bit() +
                          " " + QByteArray(phase) + " " +
                          QByteArray::number(ms) + " ms\n";
  if (!perfLogPath().isEmpty()) {
    QFile file(QString::fromLocal8Bit(perfLogPath()));
    if (file.open(QIODevice::Append | QIODevice::Text)) file.write(line);
    return;
  }
  std::fprintf(stderr, "%s", line.constData());
}

class ScopedPerf {
 public:
  explicit ScopedPerf(const char* phase) : phase_(phase), enabled_(perfFlag()) {
    if (enabled_) timer_.start();
  }
  ~ScopedPerf() {
    if (enabled_) perfLog(phase_, timer_.elapsed());
  }

  ScopedPerf(const ScopedPerf&) = delete;
  ScopedPerf& operator=(const ScopedPerf&) = delete;

 private:
  const char* phase_;
  bool enabled_;
  QElapsedTimer timer_;
};

}  // namespace qt_rsync::core
