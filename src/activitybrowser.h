#ifndef ACTIVITYBROWSER_H
#define ACTIVITYBROWSER_H

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTextBrowser>

class ActivityBrowser : public QTextBrowser {
  Q_OBJECT

public:
  explicit ActivityBrowser(QWidget *parent = nullptr);

  void setActivities(const QJsonArray &activities);
  void setPrompt(const QString &prompt);

Q_SIGNALS:
  void duplicateRequested();

private Q_SLOTS:
  void onAnchorClicked(const QUrl &url);
  void onCustomContextMenu(const QPoint &pos);

private:
  void renderHtml();
  QString generatePromptHtml() const;
  QJsonArray deduplicateActivities(QList<int> &repeatCounts) const;
  QString generateHtmlForActivity(const QJsonObject &activity, bool expanded);
  QString generateRawJsonHtml(const QJsonObject &activity, bool expanded);

  QJsonArray m_activities;
  QString m_prompt;
  QSet<QString> m_expandedItems;
  // Cache of parsed activities for O(1) lookups
  QHash<QString, QJsonObject> m_activityObjects;
};

#endif // ACTIVITYBROWSER_H
