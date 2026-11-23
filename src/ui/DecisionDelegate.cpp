#include "ui/DecisionDelegate.h"

#include <QApplication>
#include <QKeyEvent>
#include <QSet>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionViewItem>
#include <QTableView>

namespace qt_rsync::ui {
namespace {

int nextDecision(int checkState) {
  // Qt::CheckState: Unchecked=0, PartiallyChecked=1, Checked=2.
  // The model decision values are Inherit=0, Include=1, Exclude=2, Delete=3.
  // A checked box means included, so clicking it excludes; an unchecked box
  // means excluded, so clicking it restores the inherited decision.
  return checkState == Qt::Checked ? 2 : 0;
}

}  // namespace

DecisionDelegate::DecisionDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

void DecisionDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const {
  QStyleOptionViewItem styled(option);
  initStyleOption(&styled, index);
  styled.text.clear();
  styled.icon = QIcon();
  const QWidget* widget = styled.widget;
  QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
  style->drawPrimitive(QStyle::PE_PanelItemViewItem, &styled, painter, widget);

  const int state = index.data(Qt::CheckStateRole).toInt();
  if (state == 0 && index.data(Qt::CheckStateRole).isValid() == false) {
    // A delete decision is represented by the red marker alone.
    painter->save();
    painter->setPen(QPen(QColor("#ef4444"), 2, Qt::SolidLine, Qt::RoundCap));
    const QRect rect = option.rect.adjusted(
        (option.rect.width() - 16) / 2, (option.rect.height() - 16) / 2,
        -(option.rect.width() - 16) / 2, -(option.rect.height() - 16) / 2);
    painter->drawLine(rect.topLeft(), rect.bottomRight());
    painter->drawLine(rect.topRight(), rect.bottomLeft());
    painter->restore();
    return;
  }
  const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
  const int indicator = style->pixelMetric(QStyle::PM_IndicatorWidth, nullptr, widget);
  QRect box(QPoint(), QSize(indicator, indicator));
  box.moveCenter(option.rect.center());
  painter->save();
  painter->setPen(Qt::NoPen);
  painter->setBrush(option.palette.color(QPalette::Highlight));
  painter->drawRoundedRect(box, 3, 3);
  if (checked) {
    painter->setPen(QPen(option.palette.color(QPalette::HighlightedText), 2,
                         Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(box.left() + 3, box.center().y(), box.left() + 6, box.bottom() - 3);
    painter->drawLine(box.left() + 6, box.bottom() - 3, box.right() - 3, box.top() + 3);
  }
  painter->restore();

  // Coloured action glyph, matching the qpy-rsync decision column semantics.
  const QString label =
      index.model()->data(index.siblingAtColumn(4), Qt::DisplayRole).toString();
  QString glyph = QStringLiteral("=");
  QString colour = QStringLiteral("#64748B");
  if (label == QLatin1String("new")) {
    glyph = QStringLiteral("+");
    colour = QStringLiteral("#16A34A");
  } else if (label == QLatin1String("updated")) {
    glyph = QStringLiteral("\u2191");
    colour = QStringLiteral("#EA580C");
  } else if (label == QLatin1String("destination only")) {
    glyph = QStringLiteral("\u2190");
    colour = QStringLiteral("#B45309");
  } else if (label == QLatin1String("delete")) {
    glyph = QStringLiteral("\u00D7");
    colour = QStringLiteral("#DC2626");
  }
  painter->save();
  painter->setPen(QColor(colour));
  QFont font = painter->font();
  font.setBold(true);
  painter->setFont(font);
  painter->drawText(option.rect.adjusted(0, 0, -6, 0),
                    Qt::AlignRight | Qt::AlignVCenter, glyph);
  painter->restore();
}

bool DecisionDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index) {
  auto* mouse = dynamic_cast<QMouseEvent*>(event);
  if (mouse == nullptr || mouse->button() != Qt::LeftButton ||
      mouse->type() != QEvent::MouseButtonPress) {
    return QStyledItemDelegate::editorEvent(event, model, option, index);
  }
  const QString path = index.data(Qt::UserRole).toString();
  if (path.isEmpty()) return true;
  const int current = model->data(index, Qt::CheckStateRole).toInt();
  emit decisionToggled(path, nextDecision(current));
  return true;
}

DecisionKeyFilter::DecisionKeyFilter(QObject* parent) : QObject(parent) {}

bool DecisionKeyFilter::eventFilter(QObject* watched, QEvent* event) {
  auto* key = dynamic_cast<QKeyEvent*>(event);
  auto* view = dynamic_cast<QTableView*>(watched);
  if (key == nullptr || view == nullptr || key->key() != Qt::Key_Space ||
      key->type() != QEvent::KeyPress) {
    return false;
  }
  QSet<QString> paths;
  if (view->selectionModel() != nullptr) {
    for (const QModelIndex& index : view->selectionModel()->selectedRows()) {
      const QString path = index.data(Qt::UserRole).toString();
      if (!path.isEmpty()) paths.insert(path);
    }
  }
  for (const QString& path : paths) {
    const int current = view->model()->data(
        view->model()->index(indexOfPath(view, path), 4), Qt::CheckStateRole).toInt();
    emit decisionToggled(path, nextDecision(current));
  }
  return true;
}

int DecisionKeyFilter::indexOfPath(QTableView* view, const QString& path) {
  for (int row = 0; row < view->model()->rowCount(); ++row) {
    if (view->model()->data(view->model()->index(row, 0), Qt::UserRole).toString() == path) {
      return row;
    }
  }
  return -1;
}

}  // namespace qt_rsync::ui
