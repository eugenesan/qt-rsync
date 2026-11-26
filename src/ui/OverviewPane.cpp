#include "ui/OverviewPane.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTreeView>

#include <functional>
#include <QVBoxLayout>

#include "core/Filter.h"
#include "ui/Icons.h"

namespace qt_rsync::ui {
namespace {

class OverviewTree : public QTreeView {
 public:
  explicit OverviewTree(QWidget* parent = nullptr) : QTreeView(parent) {}

  std::function<void(const QString&)> jumpCallback;
  std::function<void(const QString&)> excludeCallback;
  std::function<void(const QString&, const QPoint&)> contextCallback;

  QRect jumpRect(const QModelIndex& index) const {
    const QRect row = visualRect(index);
    return QRect(2, row.top() + 4, 14, qMax(0, row.height() - 8));
  }

 protected:
  void drawBranches(QPainter* painter, const QRect& rect,
                    const QModelIndex& index) const override {
    QTreeView::drawBranches(painter, rect, index);
    const QString path = index.data(Qt::UserRole).toString();
    if (path.isEmpty()) return;
    const QIcon icon = icons::toolbarIcon(QStringLiteral("go-jump"));
    if (icon.isNull()) return;
    QRect target = jumpRect(index);
    target.moveLeft(rect.left() + 2);
    icon.paint(painter, target, Qt::AlignCenter, QIcon::Normal, QIcon::Off);
  }

  void mousePressEvent(QMouseEvent* event) override {
    const QModelIndex index = indexAt(event->pos());
    if (index.isValid() && !index.data(Qt::UserRole).toString().isEmpty() &&
        jumpRect(index).contains(event->pos())) {
      if (jumpCallback) jumpCallback(index.data(Qt::UserRole).toString());
      event->accept();
      return;
    }
    QTreeView::mousePressEvent(event);
  }

  void mouseMoveEvent(QMouseEvent* event) override {
    const QModelIndex index = indexAt(event->pos());
    const bool overJump =
        index.isValid() && !index.data(Qt::UserRole).toString().isEmpty() &&
        jumpRect(index).contains(event->pos());
    setCursor(overJump ? Qt::PointingHandCursor : Qt::ArrowCursor);
    QTreeView::mouseMoveEvent(event);
  }

  void keyPressEvent(QKeyEvent* event) override {
    if (event->key() == Qt::Key_Space && excludeCallback) {
      const QString path = currentIndex().data(Qt::UserRole).toString();
      if (!path.isEmpty()) {
        excludeCallback(path);
        event->accept();
        return;
      }
    }
    QTreeView::keyPressEvent(event);
  }

  void contextMenuEvent(QContextMenuEvent* event) override {
    const QModelIndex index = indexAt(event->pos());
    const QString path = index.data(Qt::UserRole).toString();
    if (!path.isEmpty() && contextCallback) {
      contextCallback(path, event->globalPos());
      event->accept();
      return;
    }
    QTreeView::contextMenuEvent(event);
  }
};

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
    : QWidget(parent), tree_(new OverviewTree(this)),
      model_(new model::OverviewModel(this)), stats_(new StatsBar(this)) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  auto* overviewTree = static_cast<OverviewTree*>(tree_);
  overviewTree->jumpCallback = [this](const QString& path) {
    emit folderActivated(path);
  };
  overviewTree->excludeCallback = [this](const QString& path) {
    emit excludeRequested(path);
  };
  overviewTree->contextCallback = [this](const QString& path,
                                          const QPoint& globalPosition) {
    showFolderMenu(path, globalPosition);
  };
  tree_->setModel(model_);
  tree_->setUniformRowHeights(true);
  tree_->setAlternatingRowColors(true);
  tree_->setRootIsDecorated(true);
  tree_->setSortingEnabled(true);
  // Share is the default order; its indicator is shown on the Share header.
  tree_->sortByColumn(1, Qt::DescendingOrder);
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
  tree_->header()->resizeSection(1, 96);
  tree_->header()->resizeSection(2, 64);
  tree_->header()->resizeSection(3, 84);
  layout->addWidget(tree_);
  layout->addWidget(stats_);
}

void OverviewPane::setActionColours(const QVector<QString>& colours) {
  stats_->setActionColours(colours);
}

void OverviewPane::showFolderMenu(const QString& path,
                                  const QPoint& globalPosition) {
  QMenu menu(this);
  QAction* heading = menu.addAction(path);
  heading->setEnabled(false);
  menu.addSeparator();
  QAction* exclude = menu.addAction(QStringLiteral("Exclude Folder\tSpace"));
  QAction* restore = menu.addAction(QStringLiteral("Restore Folder"));
  menu.addSeparator();
  QMenu* filterMenu = menu.addMenu(QStringLiteral("Exclude via Filter"));
  QVector<QPair<QAction*, QString>> filterActions;
  for (const core::FilterPhrase& phrase : core::phrasesFor(path, true)) {
    QAction* action = filterMenu->addAction(phrase.label);
    filterActions.append({action, phrase.pattern});
  }
  QAction* chosen = menu.exec(globalPosition);
  if (chosen == exclude) {
    emit excludeRequested(path);
  } else if (chosen == restore) {
    emit restoreRequested(path);
  } else {
    for (const auto& entry : filterActions) {
      if (entry.first == chosen) {
        emit filterRequested(entry.second, path);
        break;
      }
    }
  }
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

  // The overview describes every row the filters leave, after action
  // filters, unchanged gating and exclusions. Folded compare rows stay in
  // the totals, like qpy's overview.
  const int sortSection = tree_->header()->sortIndicatorSection();
  const Qt::SortOrder sortOrder = tree_->header()->sortIndicatorOrder();
  model_->build(store, false);
  if (sortSection >= 0) model_->sort(sortSection, sortOrder);

  if (firstBuild_) {
    // Expand only the synthetic Total row; every real directory starts
    // collapsed, matching qpy's overview.
    const QModelIndex total = model_->index(0, 0);
    if (total.isValid()) tree_->setExpanded(total, true);
    firstBuild_ = false;
  } else {
    for (const QString& path : expandedPaths_) {
      const QModelIndex index = model_->index_for_path(path.toStdString());
      if (index.isValid()) tree_->setExpanded(index, true);
    }
  }

  updateStats(store);
}

void OverviewPane::updateStats(const model::DataStore& store) {
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
    return model::qpy_format_size(bytes);
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

void OverviewPane::refreshStats(const model::DataStore& store) {
  updateStats(store);
}

}  // namespace qt_rsync::ui
