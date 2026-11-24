#pragma once

#include <QWidget>

#include <optional>

#include "core/Filter.h"

class QPlainTextEdit;
class QPushButton;

namespace qt_rsync::ui {

// The qpy-rsync filter editor: numbered editor, rsync syntax highlighting,
// line preview, import/export and help.
class FilterEditorWidget : public QWidget {
  Q_OBJECT
 public:
  explicit FilterEditorWidget(QWidget* parent = nullptr,
                              const QString& text = QString());

  [[nodiscard]] QPlainTextEdit* editor() const { return editor_; }
  [[nodiscard]] QString text() const;
  void setText(const QString& text);
  [[nodiscard]] QVector<core::FilterRule> rules() const;
  [[nodiscard]] QVector<core::FilterRule> getRules() const { return rules(); }
  [[nodiscard]] std::optional<QPair<int, QString>> firstError() const;
  void revealError();

 private:
  void updatePreview();
  void showHelp();

  QPlainTextEdit* editor_ = nullptr;
  QPlainTextEdit* preview_ = nullptr;
};

}  // namespace qt_rsync::ui
