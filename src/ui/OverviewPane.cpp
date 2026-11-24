#include "ui/OverviewPane.h"

#include <QApplication>
#include <QHeaderView>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeView>

#include <functional>
#include <QVBoxLayout>

namespace qt_rsync::ui {
namespace {

class ShareDelegate : public QStyledItemDelegate {
 public:
  using QStyledItemDelegate::QStyledItemDelegate;

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    if (index.column() != 1) {
      QStyledItemDelegate::paint(painter, option, index);
      return;
    }
    QStyleOptionViewItem blank(option);
    initStyleOption(&blank, index);
    blank.text.clear();
    blank.icon = QIcon();
    const QWidget* widget = blank.widget;
    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &blank, painter, widget);

    const double share = index.data(Qt::UserRole + 20).toDouble();
    if (share > 0.0) {
      QColor colour(QStringLiteral("#0891B2"));
      colour.setAlpha(110);
      painter->fillRect(option.rect.left(), option.rect.top(),
                        static_cast<int>(option.rect.width() * qMin(1.0, share)),
                        option.rect.height(), colour);
    }
    painter->save();
    painter->setPen(option.state & QStyle::State_Selected
                        ? option.palette.color(QPalette::HighlightedText)
                        : option.palette.color(QPalette::Text));
    painter->drawText(option.rect.adjusted(4, 0, -4, 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());
    painter->restore();
  }
};

}  // namespace

OverviewPane::OverviewPane(QWidget* parent)
    : QWidget(parent), tree_(new QTreeView(this)),
      model_(new model::OverviewModel(this)), stats_(new StatsBar(this)) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  tree_->setModel(model_);
  tree_->setSortingEnabled(true);
  tree_->sortByColumn(3, Qt::DescendingOrder);
  tree_->setItemDelegateForColumn(1, new ShareDelegate(tree_));
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionsClickable(true);
  tree_->header()->setSortIndicatorShown(true);
  layout->addWidget(tree_);
  layout->addWidget(stats_);
}

void OverviewPane::rebuild(const model::DataStore& store) {
  // Remember which folders were open; a reset of the model collapses them.
  if (!firstBuild_) {
    expandedPaths_.clear();
    std::function<void(const QModelIndex&)> walk = [&](const QModelIndex& parent) {
      for (int row = 0; row < model_->rowCount(parent); ++row) {
        const QModelIndex index = model_->index(row, 0, parent);
        if (!index.isValid()) continue;
        if (tree_->isExpanded(index)) {
          expandedPaths_.insert(index.data(Qt::UserRole).toString());
        }
        walk(index);
      }
    };
    walk(QModelIndex());
  }

  model_->build(store, false);

  if (firstBuild_) {
    tree_->expandToDepth(1);
    firstBuild_ = false;
  } else {
    for (const QString& path : expandedPaths_) {
      const QModelIndex index = model_->index_for_path(path.toStdString());
      if (index.isValid()) tree_->setExpanded(index, true);
    }
  }

  int included = 0;
  int excluded = 0;
  int deletions = 0;
  for (std::size_t id = 0; id < store.paths.size() && id < store.is_dir.size(); ++id) {
    if (store.is_dir[id]) continue;
    const int decision = id < store.decisions.size() ? store.decisions[id] : model::Inherit;
    if (decision == model::Exclude) {
      ++excluded;
    } else {
      ++included;
    }
    if ((id < store.actions.size() && store.actions[id] == model::Delete) ||
        decision == model::DeleteRequest) {
      ++deletions;
    }
  }
  stats_->setEntries({
      {QStringLiteral("Selected: %1").arg(included), QStringLiteral("#15803D")},
      {QStringLiteral("Excluded: %1").arg(excluded), QStringLiteral("#6B7280")},
      {QStringLiteral("Deletions: %1").arg(deletions), QStringLiteral("#EF4444")},
  });
}

}  // namespace qt_rsync::ui
