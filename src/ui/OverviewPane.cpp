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
  tree_->setUniformRowHeights(true);
  tree_->setAlternatingRowColors(true);
  tree_->setRootIsDecorated(true);
  tree_->setSortingEnabled(true);
  tree_->sortByColumn(3, Qt::DescendingOrder);
  tree_->setItemDelegateForColumn(1, new ShareDelegate(tree_));
  tree_->header()->setStretchLastSection(false);
  tree_->header()->setSectionsClickable(true);
  tree_->header()->setSortIndicatorShown(true);
  tree_->header()->setDefaultAlignment(Qt::AlignHCenter);
  // The item column owns the remaining width; the numeric columns keep their
  // width and the last one ends exactly at the viewport edge.
  tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  for (int column = 1; column <= 3; ++column) {
    tree_->header()->setSectionResizeMode(column, QHeaderView::Fixed);
  }
  tree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  tree_->header()->setFixedHeight(qMax(16, tree_->fontMetrics().height() + 2));
  tree_->header()->resizeSection(1, 56);
  tree_->header()->resizeSection(2, 64);
  tree_->header()->resizeSection(3, 84);
  layout->addWidget(tree_);
  layout->addWidget(stats_);
}

void OverviewPane::setActionColours(const QVector<QString>& colours) {
  stats_->setActionColours(colours);
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

  // The overview must describe the rows the compare view is showing, after
  // action filters, unchanged gating and exclusions.
  model_->build(store, true);

  if (firstBuild_) {
    // Expand only the synthetic Total row; every real directory starts
    // collapsed, matching qpy's overview.
    tree_->expandToDepth(0);
    firstBuild_ = false;
  } else {
    for (const QString& path : expandedPaths_) {
      const QModelIndex index = model_->index_for_path(path.toStdString());
      if (index.isValid()) tree_->setExpanded(index, true);
    }
  }

  int count = 0, creates = 0, updates = 0, metadata = 0, deletes = 0, kept = 0;
  long long size = 0, deletedSize = 0;
  for (int id : store.visible) {
    if (id < 0 || id >= static_cast<int>(store.paths.size())) continue;
    if (id < static_cast<int>(store.is_dir.size()) && store.is_dir[id]) continue;
    int action = id < static_cast<int>(store.actions.size())
                     ? store.actions[id] : model::NoChange;
    if (id < static_cast<int>(store.decisions.size()) &&
        store.decisions[id] == model::Exclude) {
      continue;
    }
    ++count;
    if (id < store.sizes.size()) size += store.sizes[id];
    if (id < store.decisions.size() && store.decisions[id] == model::DeleteRequest) {
      action = model::Delete;
    }
    if (action == model::Create) ++creates;
    else if (action == model::Update) ++updates;
    else if (action == model::Metadata) ++metadata;
    else if (action == model::Delete) {
      ++deletes;
      if (id < store.dest_sizes.size()) deletedSize += store.dest_sizes[id];
    } else if (action == model::DestOnly) ++kept;
  }
  const auto formatSize = [](long long bytes) {
    if (bytes < 1024) return QString::number(bytes) + QStringLiteral(" B");
    return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KiB");
  };
  stats_->setEntries({
      {QStringLiteral("%1 item(s), %2").arg(count).arg(formatSize(size)),
       QStringLiteral("#9CA3AF")},
      {QString::number(creates), QStringLiteral("#208020")},
      {QString::number(updates), QStringLiteral("#eab308")},
      {QString::number(metadata), QStringLiteral("#7c3aed")},
      {QStringLiteral("%1 (%2)").arg(deletes).arg(formatSize(deletedSize)),
       QStringLiteral("#ef4444")},
      {QString::number(kept), QStringLiteral("#a05020")},
  });
}

}  // namespace qt_rsync::ui
