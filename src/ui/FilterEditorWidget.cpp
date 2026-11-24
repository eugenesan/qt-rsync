#include "ui/FilterEditorWidget.h"

#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>

#include <functional>

namespace qt_rsync::ui {
namespace {

QString filterHelp() {
  return QStringLiteral(
      "rsync filter rules\n\n"
      "Each non-empty, non-comment line is one rule, applied in order; rsync "
      "stops at the first matching rule.\n\n"
      "Operator prefixes (rsync filter syntax):\n"
      "  + pattern   include the matching path\n"
      "  - pattern   exclude the matching path\n"
      "  P pattern   protect (do not delete) the matching path\n"
      "  R pattern   risk (allow deletion) of the matching path\n"
      "  H pattern   hide (sender-only exclude)\n"
      "  S pattern   show (sender-only include)\n"
      "  . file      merge a rule file once\n"
      "  : file      merge a per-directory rule file\n"
      "  !           clear the current list of rules\n"
      "  (no prefix) treated as an exclusion: \"- pattern\"\n\n"
      "Long names work too: exclude, include, protect, risk, hide, show, "
      "merge, dir-merge and clear.\n\n"
      "Modifiers may follow the operator (with or without a comma), for "
      "example:\n"
      "  -s pattern   sender-side exclude\n"
      "  -p pattern   perishable exclude\n"
      "  -r pattern   receiver-side exclude (protects from deletion)\n"
      "  +r pattern   receiver-side include (allows deletion)\n"
      "  -/ pattern   match against the absolute path\n"
      "  -! pattern   apply the rule when the pattern does NOT match\n"
      "  dir-merge,n- .rsync-filter\n"
      "With Mirror mode, \"-s *.kate-swp\" ignores those files on the source "
      "while letting --delete remove them from the destination.\n\n"
      "Comments:\n"
      "  A line starting with \"#\" is a comment and is ignored. Commenting "
      "or uncommenting a rule is the only way to enable or disable it.\n\n"
      "Patterns:\n"
      "  *  matches any path component, but stops at slashes\n"
      "  ** matches anything, including slashes\n"
      "  ?  matches any single character\n"
      "  [..] character class\n"
      "  A trailing \"/\" matches directories only.\n"
      "  A leading \"/\" anchors the pattern to the transfer root.\n\n"
      "Examples:\n"
      "  __pycache__/        exclude any __pycache__ directory\n"
      "  *.pyc               exclude every .pyc file\n"
      "  + important.txt     include important.txt even if excluded above\n"
      "  - main.py           exclude main.py\n"
      "  P cache/***         protect the cache tree from deletion");
}

struct LineRule {
  core::FilterRule rule;
  bool valid = true;
};

std::optional<LineRule> lineToRule(const QString& line) {
  const QString stripped = line.trimmed();
  if (stripped.isEmpty() || stripped.startsWith('#')) return std::nullopt;
  LineRule result;
  result.rule.rule =
      core::isAdvancedRule(stripped) ? stripped : QStringLiteral("- ") + stripped;
  result.rule.type = core::RuleType::Advanced;
  result.valid = core::validateRule(result.rule);
  return result;
}

class FilterHighlighter : public QSyntaxHighlighter {
 public:
  explicit FilterHighlighter(QTextDocument* document)
      : QSyntaxHighlighter(document) {
    comment_.setForeground(QColor(QStringLiteral("#6a737d")));
    comment_.setFontItalic(true);
    operator_.setForeground(QColor(QStringLiteral("#005cc5")));
    operator_.setFontWeight(QFont::Bold);
    invalid_.setForeground(QColor(QStringLiteral("#d73a49")));
    invalid_.setUnderlineColor(QColor(QStringLiteral("#d73a49")));
    invalid_.setUnderlineStyle(QTextCharFormat::SingleUnderline);
  }

 protected:
  void highlightBlock(const QString& text) override {
    const QString stripped = text.trimmed();
    if (stripped.isEmpty() || stripped.startsWith('#')) {
      setFormat(0, text.size(), comment_);
      return;
    }
    const auto parsed = core::parseAdvancedRule(stripped);
    if (parsed.has_value()) {
      const int leading = text.size() - text.trimmed().size();
      setFormat(leading, parsed->token.size(), operator_);
    }
    const auto rule = lineToRule(text);
    if (rule.has_value() && !rule->valid) {
      setFormat(0, text.size(), invalid_);
    }
  }

 private:
  QTextCharFormat comment_;
  QTextCharFormat operator_;
  QTextCharFormat invalid_;
};

class LineNumberArea : public QWidget {
 public:
  LineNumberArea(QWidget* parent, std::function<int()> width,
                 std::function<void(QPainter&, int)> paint)
      : QWidget(parent), width_(std::move(width)), paint_(std::move(paint)) {
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Base);
  }
  QSize sizeHint() const override { return QSize(width_(), 0); }

 protected:
  void paintEvent(QPaintEvent*) override {
    QPainter painter(this);
    painter.fillRect(rect(), palette().color(backgroundRole()));
    paint_(painter, width_());
  }

 private:
  std::function<int()> width_;
  std::function<void(QPainter&, int)> paint_;
};

class NumberedTextEdit : public QPlainTextEdit {
 public:
  explicit NumberedTextEdit(QWidget* parent = nullptr)
      : QPlainTextEdit(parent),
        numbers_(new LineNumberArea(
            this, [this]() { return lineWidth(); },
            [this](QPainter& painter, int width) {
              paintNumbers(painter, width);
            })) {
    connect(this, &QPlainTextEdit::blockCountChanged, this,
            [this]() { updateMargins(); });
    connect(this, &QPlainTextEdit::updateRequest, this,
            [this](const QRect& rect, int dy) {
              if (dy != 0) {
                numbers_->scroll(0, dy);
              } else {
                numbers_->update(0, rect.y(), numbers_->width(), rect.height());
              }
            });
    updateMargins();
  }

  int lineWidth() const {
    const int digits = QString::number(qMax(1, blockCount())).size();
    return fontMetrics().horizontalAdvance(QLatin1Char('0')) * digits + 12;
  }

 protected:
  void resizeEvent(QResizeEvent* event) override {
    QPlainTextEdit::resizeEvent(event);
    const QRect area = contentsRect();
    numbers_->setGeometry(QRect(area.left(), area.top(), lineWidth(),
                                area.height()));
  }

 private:
  void updateMargins() { setViewportMargins(lineWidth(), 0, 0, 0); }

  void paintNumbers(QPainter& painter, int width) {
    painter.setFont(font());
    painter.setPen(palette().color(QPalette::Text));
    QTextBlock block = firstVisibleBlock();
    int top = static_cast<int>(
        blockBoundingGeometry(block).translated(contentOffset()).top());
    int number = block.blockNumber() + 1;
    while (block.isValid() && top <= height()) {
      const int blockHeight = static_cast<int>(blockBoundingRect(block).height());
      if (block.isVisible() && top + blockHeight >= 0) {
        painter.drawText(0, top, width - 4, blockHeight, Qt::AlignRight,
                         QString::number(number));
      }
      block = block.next();
      top += blockHeight;
      ++number;
    }
  }

  LineNumberArea* numbers_;
};

}  // namespace

FilterEditorWidget::FilterEditorWidget(QWidget* parent, const QString& text)
    : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  auto* editor = new NumberedTextEdit(this);
  editor_ = editor;
  editor_->setPlainText(text);
  new FilterHighlighter(editor_->document());
  preview_ = new QPlainTextEdit(this);
  preview_->setReadOnly(true);
  preview_->setMaximumHeight(preview_->fontMetrics().lineSpacing() * 2 + 12);
  preview_->setPlaceholderText(QStringLiteral("Line preview"));
  auto* buttons = new QHBoxLayout();
  auto* import = new QPushButton(QStringLiteral("Import"), this);
  auto* exportButton = new QPushButton(QStringLiteral("Export"), this);
  auto* help = new QPushButton(QStringLiteral("Help"), this);
  buttons->addWidget(import);
  buttons->addWidget(exportButton);
  buttons->addWidget(help);
  buttons->addStretch(1);
  auto* splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(editor_);
  splitter->addWidget(preview_);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 0);
  splitter->setCollapsible(1, false);
  splitter->setSizes({360, preview_->maximumHeight()});
  layout->addWidget(splitter, 1);
  layout->addLayout(buttons);

  connect(editor_, &QPlainTextEdit::cursorPositionChanged, this,
          [this]() { updatePreview(); });
  connect(import, &QPushButton::clicked, this, [this]() {
    const QString path =
        QFileDialog::getOpenFileName(this, QStringLiteral("Import filters"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    setText(QString::fromUtf8(file.readAll()));
  });
  connect(exportButton, &QPushButton::clicked, this, [this]() {
    const QString path =
        QFileDialog::getSaveFileName(this, QStringLiteral("Export filters"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    file.write(this->text().toUtf8());
  });
  connect(help, &QPushButton::clicked, this, [this]() { showHelp(); });
  updatePreview();
}

QString FilterEditorWidget::text() const { return editor_->toPlainText(); }

void FilterEditorWidget::setText(const QString& text) {
  editor_->setPlainText(text);
  updatePreview();
}

QVector<core::FilterRule> FilterEditorWidget::rules() const {
  QVector<core::FilterRule> result;
  for (const QString& line : text().split('\n')) {
    const auto rule = lineToRule(line);
    if (rule.has_value()) result.append(rule->rule);
  }
  return result;
}

std::optional<QPair<int, QString>> FilterEditorWidget::firstError() const {
  const QStringList lines = text().split('\n');
  for (int index = 0; index < lines.size(); ++index) {
    const auto rule = lineToRule(lines.at(index));
    if (rule.has_value() && !rule->valid) {
      return QPair<int, QString>(
          index + 1, QStringLiteral("invalid rsync rule"));
    }
  }
  return std::nullopt;
}

void FilterEditorWidget::revealError() {
  const auto error = firstError();
  if (!error.has_value()) return;
  QTextCursor cursor(editor_->document()->findBlockByNumber(error->first - 1));
  editor_->setTextCursor(cursor);
  editor_->setFocus();
}

void FilterEditorWidget::updatePreview() {
  const QTextBlock block = editor_->textCursor().block();
  const int number = block.blockNumber() + 1;
  const QString line = block.text().trimmed();
  QString text;
  if (line.isEmpty()) {
    text = QStringLiteral("Line %1: blank, ignored").arg(number);
  } else if (line.startsWith('#')) {
    text = QStringLiteral("Line %1: comment, ignored").arg(number);
  } else if (core::isAdvancedRule(line)) {
    text = QStringLiteral("Line %1: rsync rule: %2").arg(number).arg(line);
  } else {
    text = QStringLiteral("Line %1: exclusion: - %2").arg(number).arg(line);
  }
  const auto rule = lineToRule(block.text());
  if (rule.has_value() && !rule->valid) {
    text = QStringLiteral("Line %1: ERROR: invalid rsync rule").arg(number);
  }
  preview_->setPlainText(text);
  preview_->setToolTip(text);
}

void FilterEditorWidget::showHelp() {
  QMessageBox::information(this, QStringLiteral("rsync filter rules"),
                           filterHelp());
}

}  // namespace qt_rsync::ui
