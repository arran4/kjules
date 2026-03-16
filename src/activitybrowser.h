#ifndef ACTIVITYBROWSER_H
#define ACTIVITYBROWSER_H

#include <QTextBrowser>
#include <QJsonArray>
#include <QSet>
#include <QJsonObject>
#include <QMap>

class ActivityBrowser : public QTextBrowser {
  Q_OBJECT

public:
  explicit ActivityBrowser(QWidget *parent = nullptr);

  void setActivities(const QJsonArray &activities);
  void setPrompt(const QString &prompt);

private Q_SLOTS:
  void onAnchorClicked(const QUrl &url);
  void onCustomContextMenu(const QPoint &pos);

private:
  void renderHtml();
  QString generateHtmlForActivity(const QJsonObject &activity, bool expanded);
  QString generateRawJsonHtml(const QJsonObject &activity, bool expanded);

  QJsonArray m_activities;
  QString m_prompt;
  QSet<QString> m_expandedItems;
  // Map element id to the string representation of its JSON for context menu
  QMap<QString, QString> m_activityJsons;
};

#endif // ACTIVITYBROWSER_H
