#include "queuemanager.h"
#include "apimanager.h"
#include <QTimer>

QueueManager::QueueManager(APIManager *apiManager, QObject *parent)
    : QAbstractListModel(parent), m_apiManager(apiManager),
      m_isProcessing(false), m_jobsCompletedSinceLastWait(0), m_tier(Free) {
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &QueueManager::onTick);
}

int QueueManager::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_queue.size();
}

QVariant QueueManager::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_queue.size())
    return QVariant();

  const QueueItem &item = m_queue[index.row()];

  switch (role) {
  case TypeRole:
    return item.type;
  case DescriptionRole:
    if (item.type == QueueItem::SessionJob) {
      return QStringLiteral("Create session for ") + item.source;
    } else {
      return QStringLiteral("Waiting to respect rate limits...");
    }
  case StatusRole:
    if (index.row() == 0 && m_isProcessing) {
      if (item.type == QueueItem::Wait) {
        return QStringLiteral("Waiting (%1m %2s remaining)")
            .arg(item.remainingSeconds / 60)
            .arg(item.remainingSeconds % 60);
      } else {
        return QStringLiteral("Processing...");
      }
    }
    return QStringLiteral("Queued");
  case TimeRemainingRole:
    if (item.type == QueueItem::Wait) {
      return item.remainingSeconds;
    }
    return 0;
  case Qt::DisplayRole:
    if (item.type == QueueItem::SessionJob) {
      return QStringLiteral("Job: ") + item.source;
    } else {
      return QStringLiteral("Wait 1 Hour");
    }
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> QueueManager::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[TypeRole] = "type";
  roles[DescriptionRole] = "description";
  roles[StatusRole] = "status";
  roles[TimeRemainingRole] = "timeRemaining";
  return roles;
}

void QueueManager::setTier(Tier tier) { m_tier = tier; }

int QueueManager::jobsBeforeWait() const {
  switch (m_tier) {
  case Free:
    return 3;
  case Pro:
    return 15;
  case Max:
    return 30;
  default:
    return 3;
  }
}

void QueueManager::addJobs(const QStringList &sources, const QString &prompt,
                           const QString &automationMode) {
  if (sources.isEmpty())
    return;

  beginInsertRows(QModelIndex(), m_queue.size(),
                  m_queue.size() + sources.size() - 1);

  for (const QString &source : sources) {
    QueueItem job;
    job.type = QueueItem::SessionJob;
    job.source = source;
    job.prompt = prompt;
    job.automationMode = automationMode;
    m_queue.append(job);
  }

  endInsertRows();

  if (!m_isProcessing && !m_queue.isEmpty()) {
    processNext();
  }
}

void QueueManager::processNext() {
  if (m_queue.isEmpty()) {
    m_isProcessing = false;
    return;
  }

  m_isProcessing = true;
  Q_EMIT dataChanged(index(0, 0), index(0, 0), {StatusRole});

  if (m_queue.first().type == QueueItem::SessionJob &&
      m_jobsCompletedSinceLastWait >= jobsBeforeWait()) {
    beginInsertRows(QModelIndex(), 0, 0);
    QueueItem waitItem;
    waitItem.type = QueueItem::Wait;
    waitItem.remainingSeconds = 3600; // 1 hour
    m_queue.insert(0, waitItem);
    endInsertRows();
    m_jobsCompletedSinceLastWait = 0;
  }

  QueueItem &item = m_queue.first();

  if (item.type == QueueItem::Wait) {
    m_timer->start(1000); // Tick every second
  } else if (item.type == QueueItem::SessionJob) {
    // Create a local context object that will be destroyed once the callbacks
    // run, avoiding the need for manual connection pointers.
    QObject *context = new QObject(this);

    // In case the API hangs indefinitely, we can add a fallback timer to cancel
    // this context.
    QTimer *fallbackTimer = new QTimer(context);
    fallbackTimer->setSingleShot(true);
    // e.g. 60 seconds timeout
    fallbackTimer->start(60000);

    auto finishJob = [this, context]() {
      m_jobsCompletedSinceLastWait++;

      beginRemoveRows(QModelIndex(), 0, 0);
      m_queue.removeFirst();
      endRemoveRows();

      // Cleanup the context which breaks all connections attached to it
      context->deleteLater();

      QTimer::singleShot(0, this, &QueueManager::processNext);
    };

    connect(m_apiManager, &APIManager::sessionCreated, context,
            [finishJob]() { finishJob(); });

    connect(m_apiManager, &APIManager::errorOccurred, context,
            [finishJob](const QString &) { finishJob(); });

    connect(fallbackTimer, &QTimer::timeout, context, [finishJob]() {
      // API timed out
      finishJob();
    });

    m_apiManager->createSession(item.source, item.prompt, item.automationMode);
  }
}

void QueueManager::onTick() {
  if (m_queue.isEmpty() || m_queue.first().type != QueueItem::Wait) {
    m_timer->stop();
    return;
  }

  m_queue.first().remainingSeconds--;
  Q_EMIT dataChanged(index(0, 0), index(0, 0), {StatusRole, TimeRemainingRole});

  if (m_queue.first().remainingSeconds <= 0) {
    m_timer->stop();
    beginRemoveRows(QModelIndex(), 0, 0);
    m_queue.removeFirst();
    endRemoveRows();

    processNext();
  }
}
