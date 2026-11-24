#include "ui/ComparisonView.h"

#include "core/Filter.h"
#include "ui/Icons.h"
#include "ui/TreeDelegate.h"

#include <QApplication>
#include <QClipboard>
#include <QHash>
#include <QKeyEvent>
#include <QHeaderView>
#include <QMenu>
#include <QScrollBar>
#include <QHBoxLayout>
#include <QTableView>

namespace qt_rsync::ui {
namespace {

class FoldKeyFilter : public QObject {
 public:
  FoldKeyFilter(ComparisonView* view, QObject* parent)
      : QObject(parent), view_(view) {}

  void watch(QObject* object, int pane) { panes_.insert(object, pane); }

  bool eventFilter(QObject* watched, QEvent* event) override {
    auto* key = dynamic_cast<QKeyEvent*>(event);
    if (key == nullptr || key->type() != QEvent::KeyPress) return false;
    if (key->key() != Qt::Key_Left && key->key() != Qt::Key_Right) return false;
    auto it = panes_.constFind(watched);
    if (it == panes_.cend()) return false;
    view_->foldCurrent(it.value(), key->key() == Qt::Key_Left);
    return true;
  }

 private:
  ComparisonView* view_;
  QHash<QObject*, int> panes_;
};

}  // namespace

ComparisonView::ComparisonView(QWidget* parent) : QWidget(parent) {
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  auto* tableLayout = new QHBoxLayout();
  tableLayout->setContentsMargins(0, 0, 0, 0);
  tableLayout->setSpacing(0);

  for (int pane = 0; pane < 3; ++pane) {
    auto* view = new QTableView(this);
    view->verticalHeader()->hide();
    if (pane != 2) {
      view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
    view->horizontalHeader()->setHighlightSections(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setAlternatingRowColors(true);
    auto* model = new model::TableModel(&store_, pane, this);
    view->setModel(model);
    views_.append(view);
    models_.append(model);
    tableLayout->addWidget(view, pane == 1 ? 0 : 1);
  }
  layout->addLayout(tableLayout, 1);

  // Same visible columns as the proven hybrid view: source columns on the
  // left, the decision column in the middle, destination columns on the right.
  for (int column : {3, 4, 5, 6, 7, 8, 9, 10}) views_[0]->setColumnHidden(column, true);
  for (int column = 0; column < 11; ++column) {
    views_[1]->setColumnHidden(column, column != 4);
  }
  for (int column : {0, 1, 2, 3, 4, 8, 9, 10}) views_[2]->setColumnHidden(column, true);
  views_[1]->setFixedWidth(92);
  for (QTableView* view : views_) {
    view->verticalHeader()->setDefaultSectionSize(22);
    view->horizontalHeader()->setFixedHeight(
        qMax(16, view->fontMetrics().height() + 2));
  }

  store_.action_colours = {
      QStringLiteral("#16A34A"),  // create
      QStringLiteral("#EA580C"),  // update
      QStringLiteral("#0891B2"),  // metadata
      QStringLiteral("#DC2626"),  // delete
      QStringLiteral("#B45309"),  // destination only
      QStringLiteral("#64748B"),  // unchanged
  };
  store_.folder_icon = icons::folderIcon();
  store_.root_icon = icons::rootFolderIcon();
  for (const char* extension : {".txt", ".md", ".json", ".py", ".log", ".zip"}) {
    store_.file_icons[extension] = icons::fileIcon(
        QString::fromLatin1("file") + QLatin1String(extension));
  }

  auto* foldFilter = new FoldKeyFilter(this, this);
  for (int i = 0; i < 3; ++i) {
    views_[i]->installEventFilter(foldFilter);
    foldFilter->watch(views_[i], i);
  }

  auto* treeDelegate = new TreeDelegate(this);
  views_[0]->setItemDelegateForColumn(0, treeDelegate);
  views_[2]->setItemDelegateForColumn(5, treeDelegate);
  delegate_ = new DecisionDelegate(this);
  views_[1]->setItemDelegateForColumn(4, delegate_);
  keyFilter_ = new DecisionKeyFilter(this);
  connect(delegate_, &DecisionDelegate::decisionToggled,
          this, &ComparisonView::applyDecision);
  connect(keyFilter_, &DecisionKeyFilter::decisionToggled,
          this, &ComparisonView::applyDecision);
  connect(keyFilter_, &DecisionKeyFilter::batchStarted,
          this, &ComparisonView::startDecisionBatch);
  connect(keyFilter_, &DecisionKeyFilter::batchFinished,
          this, &ComparisonView::endDecisionBatch);
  // Column widths in the ballpark of qpy-rsync: a wide name column, narrow
  // Items and Size columns, and a compact decision pane.
  for (int pane : {0, 2}) {
    QHeaderView* header = views_[pane]->horizontalHeader();
    const int nameColumn = pane == 0 ? 0 : 5;
    const int itemsColumn = pane == 0 ? 1 : 6;
    const int sizeColumn = pane == 0 ? 2 : 7;
    header->setStretchLastSection(false);
    header->setSectionResizeMode(nameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(itemsColumn, QHeaderView::Fixed);
    header->setSectionResizeMode(sizeColumn, QHeaderView::Fixed);
    header->resizeSection(itemsColumn, 70);
    header->resizeSection(sizeColumn, 140);
  }
  QHeaderView* decisionHeader = views_[1]->horizontalHeader();
  decisionHeader->setStretchLastSection(false);
  decisionHeader->setSectionResizeMode(4, QHeaderView::Fixed);
  decisionHeader->resizeSection(4, 140);

  for (QTableView* view : views_) {
    view->installEventFilter(keyFilter_);
    view->viewport()->installEventFilter(keyFilter_);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    const int pane = views_.indexOf(view);
    connect(view, &QTableView::customContextMenuRequested, this,
            [this, pane](const QPoint& position) { showContextMenu(pane, position); });
    view->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(view->horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, [this, pane](const QPoint& position) { showHeaderMenu(pane, position); });
  }
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      if (i == j) continue;
      connect(views_[i]->verticalScrollBar(), &QScrollBar::valueChanged,
              views_[j]->verticalScrollBar(), &QScrollBar::setValue);
    }
    connect(views_[i]->selectionModel(), &QItemSelectionModel::currentChanged,
            this, [this, i](const QModelIndex& index) { onCurrentChanged(i, index); });
    connect(views_[i]->horizontalHeader(), &QHeaderView::sortIndicatorChanged,
            this, [this, i](int section, Qt::SortOrder order) {
              onSortIndicatorChanged(i, section, order);
            });
  }
}

void ComparisonView::setFilterBar(ActionFilterBar* bar) {
  filterBar_ = bar;
  if (filterBar_ != nullptr) {
    connect(filterBar_, &ActionFilterBar::filterChanged,
            this, &ComparisonView::applyFilters);
  }
}

void ComparisonView::applyFilters() {
  if (filterBar_ == nullptr) return;
  int mask = 0;
  for (int action = 0; action < 5; ++action) {
    if (filterBar_->actionVisible(action)) mask |= (1 << action);
  }
  store_.action_mask = (mask == 0x1F) ? 0 : mask;
  store_.show_excluded = filterBar_->showExcluded();
  store_.dirs_only = filterBar_->filterDirectories();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
}

void ComparisonView::setRootNames(const QString& source,
                                  const QString& destination) {
  store_.root_source = source.toStdString();
  store_.root_dest = destination.toStdString();
}

void ComparisonView::setChanges(const QVector<core::Change>& changes) {
  decisions_.reconcile(changes, true);
  loadChanges(store_, changes);
  syncDecisionsToStore();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
}

void ComparisonView::applyDestinationMetadata(
    const QHash<QString, QPair<qlonglong, QString>>& metadata) {
  std::unordered_map<std::string, std::pair<long long, std::string>> converted;
  for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
    converted.emplace(it.key().toStdString(),
                      std::make_pair(static_cast<long long>(it.value().first),
                                     it.value().second.toStdString()));
  }
  store_.apply_destination_metadata(converted, false);
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
}

void ComparisonView::resetDecision(const QString& path) {
  decisions_.reset(path);
  decisionHistory_.append(decisions_.explicitItemsAsHash());
  syncDecisionsToStore();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
  emit undoAvailable(true);
}

void ComparisonView::resetSubtreeDecisions(const QString& path) {
  decisions_.resetSubtree(path);
  syncDecisionsToStore();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
  emit undoAvailable(!decisionHistory_.isEmpty());
}

void ComparisonView::resetAllDecisions() {
  decisions_.clear();
  syncDecisionsToStore();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
  emit undoAvailable(!decisionHistory_.isEmpty());
}

void ComparisonView::markDelete(const QString& path) {
  applyDecision(path, static_cast<int>(core::Decision::Delete));
}

void ComparisonView::startDecisionBatch() {
  batching_ = true;
  batchBefore_ = decisions_.explicitItemsAsHash();
}

void ComparisonView::endDecisionBatch() {
  batching_ = false;
  if (decisions_.explicitItemsAsHash() != batchBefore_) {
    decisionHistory_.append(batchBefore_);
    if (decisionHistory_.size() > 100) decisionHistory_.removeFirst();
    emit undoAvailable(true);
  }
  batchBefore_.clear();
}

bool ComparisonView::undoDecision() {
  if (decisionHistory_.isEmpty()) return false;
  decisions_.replace(decisionHistory_.takeLast());
  syncDecisionsToStore();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
  emit undoAvailable(!decisionHistory_.isEmpty());
  return true;
}

void ComparisonView::applyDecision(const QString& path, int decision) {
  const core::Decision value = static_cast<core::Decision>(decision);
  if (decisions_.explicitDecision(path) == value) return;
  if (!batching_) {
    decisionHistory_.append(decisions_.explicitItemsAsHash());
    if (decisionHistory_.size() > 100) decisionHistory_.removeFirst();
  }
  decisions_.set(path, value);
  syncDecisionsToStore();
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  emit decisionChanged();
  emit undoAvailable(!decisionHistory_.isEmpty());
}

void ComparisonView::syncDecisionsToStore() {
  // Clear the mirrored decisions first: an undo removes entries, and stale
  // values would otherwise keep the rows hidden.
  for (int& value : store_.decisions) value = model::Inherit;
  store_.decision_log.clear();
  store_.compute_aggregates();
  const auto items = decisions_.explicitItems();
  for (const auto& item : items) {
    const std::string key = item.first.toStdString();
    for (std::size_t id = 0; id < store_.paths.size(); ++id) {
      const std::string& path = store_.paths[id];
      if (path != key && path.rfind(key + '/', 0) != 0) continue;
      if (id < store_.decisions.size()) {
        store_.decisions[id] = static_cast<int>(item.second);
      }
    }
  }
}

void ComparisonView::onCurrentChanged(int origin, const QModelIndex& index) {
  if (selecting_ || !index.isValid()) return;
  selecting_ = true;
  for (int pane = 0; pane < 3; ++pane) {
    if (pane == origin) continue;
    const QModelIndex target = views_[pane]->model()->index(index.row(), 0);
    views_[pane]->setCurrentIndex(target);
  }
  selecting_ = false;
}

void ComparisonView::onSortIndicatorChanged(int pane, int section,
                                            Qt::SortOrder order) {
  if (sorting_) return;
  sorting_ = true;
  const int logical = (pane == 1) ? 4 : section;
  models_[pane]->sort(logical, order);
  for (int other = 0; other < 3; ++other) {
    QHeaderView* header = views_[other]->horizontalHeader();
    const bool blocked = header->blockSignals(true);
    header->setSortIndicator(other == 1 ? 4 : section, order);
    header->blockSignals(blocked);
  }
  sorting_ = false;
}

void ComparisonView::selectPath(const QString& path) {
  const std::string key = path.toStdString();
  int row = -1;
  for (int r = 0; r < static_cast<int>(store_.visible.size()); ++r) {
    if (store_.paths[store_.visible[r]] == key) {
      row = r;
      break;
    }
  }
  if (row < 0) return;
  for (QTableView* view : views_) {
    view->setCurrentIndex(view->model()->index(row, 0));
  }
}

void ComparisonView::foldCurrent(int pane, bool collapse) {
  if (pane < 0 || pane >= views_.size()) return;
  const QModelIndex current = views_[pane]->currentIndex();
  if (!current.isValid()) return;
  const int row = current.row();
  if (row < 0 || row >= static_cast<int>(store_.visible.size())) return;
  const int id = store_.visible[row];
  if (id < 0 || id >= static_cast<int>(store_.is_dir.size()) || !store_.is_dir[id]) {
    return;
  }
  const QString path = QString::fromStdString(store_.paths[id]);
  store_.set_collapsed(id, collapse);
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
  selectPath(path);
}

void ComparisonView::foldAll(bool collapse) {
  for (std::size_t id = 0; id < store_.is_dir.size(); ++id) {
    if (store_.is_dir[id]) store_.set_collapsed(static_cast<int>(id), collapse);
  }
  store_.rebuild_visible();
  for (model::TableModel* model : models_) model->reset();
}

void ComparisonView::showContextMenu(int pane, const QPoint& position) {
  QTableView* view = views_[pane];
  const QModelIndex index = view->indexAt(position);
  const QString path = index.data(Qt::UserRole).toString();
  QMenu menu(view);
  if (!path.isEmpty()) {
    auto* heading = menu.addAction(path);
    heading->setEnabled(false);
    menu.addSeparator();
  }
  menu.addAction(QStringLiteral("Copy Path"), [path]() {
    QApplication::clipboard()->setText(path);
  });
  menu.addAction(QStringLiteral("Copy Filename"), [path]() {
    QApplication::clipboard()->setText(path.section('/', -1));
  });
  menu.addAction(QStringLiteral("Copy Relative Path"), [path]() {
    QString relative = path;
    while (relative.startsWith('/')) relative.remove(0, 1);
    QApplication::clipboard()->setText(relative);
  });
  menu.addAction(QStringLiteral("Open Directory"), [this, pane, path]() {
    emit openDirectoryRequested(pane, path);
  });
  menu.addSeparator();
  menu.addAction(QStringLiteral("Open in Diff Tool\t1"), [this, path]() {
    emit diffRequested(path);
  });
  menu.addSeparator();

  if (pane == 1 && !path.isEmpty()) {
    menu.addAction(QStringLiteral("Reset This Decision"),
                   [this, path]() { resetDecision(path); });
    menu.addAction(QStringLiteral("Reset Subtree Decisions"),
                   [this, path]() { resetSubtreeDecisions(path); });
    auto* resetAll = menu.addAction(QStringLiteral("Reset All Decisions"),
                                    [this]() { resetAllDecisions(); });
    resetAll->setEnabled(decisions_.hasExplicit());
    menu.addSeparator();
  }

  if (!path.isEmpty()) {
    const auto it = store_.path_to_id.find(path.toStdString());
    const bool isDirectory =
        it != store_.path_to_id.end() &&
        it->second < static_cast<int>(store_.is_dir.size()) &&
        store_.is_dir[it->second] != 0;

    auto* excludeMenu = menu.addMenu(QStringLiteral("Exclude via Filter"));
    for (const core::FilterPhrase& phrase : core::phrasesFor(path, isDirectory)) {
      excludeMenu->addAction(phrase.label, [this, phrase, path]() {
        emit filterRequested(phrase.pattern, path);
      });
    }
    auto* deleteMenu = menu.addMenu(QStringLiteral("Delete on Destination"));
    for (const core::FilterPhrase& phrase :
         core::deletePhrasesFor(path, isDirectory)) {
      deleteMenu->addAction(phrase.label, [this, phrase, path]() {
        emit deleteFilterRequested(phrase.pattern, path);
      });
    }
    deleteMenu->addSeparator();
    deleteMenu->addAction(QStringLiteral("Decision only (apply at sync)"),
                          [this, path]() { emit deleteRequested(path); });
    menu.addSeparator();

    const int decision = static_cast<int>(decisions_.explicitDecision(path));
    if (decision == static_cast<int>(core::Decision::Exclude)) {
      menu.addAction(QStringLiteral("Include Item"),
                     [this, path]() { applyDecision(path, 0); });
    } else if (decision == static_cast<int>(core::Decision::Delete)) {
      menu.addAction(QStringLiteral("Remove Delete Decision"),
                     [this, path]() { applyDecision(path, 0); });
    } else {
      menu.addAction(QStringLiteral("Exclude Item\tSpace"),
                     [this, path]() { applyDecision(path, 2); });
    }

    auto* fold = menu.addMenu(QStringLiteral("Collapse / Expand"));
    if (isDirectory) {
      fold->addAction(store_.is_collapsed(it->second)
                          ? QStringLiteral("Expand Folder")
                          : QStringLiteral("Collapse Folder"),
                      [this, pane, it]() {
                        foldCurrent(pane, !store_.is_collapsed(it->second));
                      });
      fold->addSeparator();
    }
    fold->addAction(QStringLiteral("Collapse All Folders"),
                    [this]() { foldAll(true); });
    fold->addAction(QStringLiteral("Expand All Folders"),
                    [this]() { foldAll(false); });
  }
  menu.exec(view->viewport()->mapToGlobal(position));
}

void ComparisonView::showHeaderMenu(int pane, const QPoint& position) {
  QHeaderView* header = views_[pane]->horizontalHeader();
  QMenu menu(header);
  static const QVector<QPair<QString, int>> names{
      {QStringLiteral("Item"), 0},
      {QStringLiteral("Items"), 1},
      {QStringLiteral("Size"), 2},
      {QStringLiteral("Modified"), 3},
      {QStringLiteral("Size (bytes)"), 9},
  };
  for (const auto& name : names) {
    const int column = (pane == 2 && name.second < 4) ? name.second + 5
                                                      : name.second;
    auto* action = menu.addAction(name.first);
    action->setCheckable(true);
    action->setChecked(!header->isSectionHidden(column));
    action->setEnabled(pane != 1 || column == 4);
    connect(action, &QAction::toggled, header, [header, column](bool checked) {
      header->setSectionHidden(column, !checked);
    });
  }
  menu.exec(header->mapToGlobal(position));
}

}  // namespace qt_rsync::ui
