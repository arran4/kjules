#include "activitybrowser.h"

#include <KLocalizedString>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDialog>
#include <QGuiApplication>
#include <QImageReader>
#include <QJsonDocument>
#include <QLabel>
#include <QMenu>
#include <QScreen>
#include <QVBoxLayout>

ActivityBrowser::ActivityBrowser(QWidget *parent) : QTextBrowser(parent) {
  setOpenLinks(false);
  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(this, &QTextBrowser::anchorClicked, this,
          &ActivityBrowser::onAnchorClicked);
  connect(this, &QWidget::customContextMenuRequested, this,
          &ActivityBrowser::onCustomContextMenu);
}

void ActivityBrowser::setActivities(const QJsonArray &activities) {
  m_activities = activities;
  renderHtml();
}

void ActivityBrowser::setPrompt(const QString &prompt) {
  m_prompt = prompt;
  renderHtml();
}

void ActivityBrowser::renderHtml() {
  QString html =
      QStringLiteral("<html><head><style>") +
      QStringLiteral("body { font-family: sans-serif; font-size: 13px; margin: "
                     "10px; background-color: #fafafa; }") +
      QStringLiteral(
          ".turn { margin-bottom: 25px; padding: 15px; border-radius: 8px; "
          "box-shadow: 0 2px 4px rgba(0,0,0,0.05); }") +
      QStringLiteral(
          ".user-turn { margin-left: 15%; margin-right: 5%; background-color: "
          "#e3f2fd; border: 1px solid #bbdefb; }") +
      QStringLiteral(
          ".agent-turn { margin-left: 5%; margin-right: 15%; background-color: "
          "#ffffff; border: 1px solid #e0e0e0; }") +
      QStringLiteral(
          ".system-turn { margin-left: 10%; margin-right: 10%; "
          "background-color: #fff9c4; border: 1px solid #fff59d; }") +
      QStringLiteral(
          ".role { font-weight: bold; text-transform: capitalize; "
          "margin-bottom: 8px; color: #333; font-size: 1.0em; border-bottom: "
          "1px solid rgba(0,0,0,0.05); padding-bottom: 5px; }") +
      QStringLiteral(
          ".content { white-space: pre-wrap; word-wrap: break-word; }") +
      QStringLiteral(".box { background: rgba(255,255,255,0.7); border: 1px "
                     "solid rgba(0,0,0,0.1); padding: 10px; border-radius: "
                     "4px; margin-top: 5px; }") +
      QStringLiteral(".code { font-family: monospace; background: "
                     "rgba(0,0,0,0.05); padding: 8px; border-radius: 4px; "
                     "display: block; white-space: pre-wrap; }") +
      QStringLiteral(
          ".btn { display: inline-block; padding: 5px 10px; background: "
          "#3498db; color: white; text-decoration: none; border-radius: 3px; "
          "font-weight: bold; margin-top: 5px; }") +
      QStringLiteral(".prompt { font-weight: bold; font-size: 1.1em; color: "
                     "#2c3e50; margin-bottom: 10px; }") +
      QStringLiteral("</style></head><body>");

  if (!m_prompt.isEmpty()) {
    html +=
        QStringLiteral("<div class='prompt'>") + i18n("Prompt") +
        QStringLiteral("</div><div style='margin-bottom: 10px; padding: 10px; "
                       "background-color: #f0f0f0; border-radius: 5px;'>");

    bool promptExpanded =
        m_expandedItems.contains(QStringLiteral("prompt_view"));
    QStringList lines = m_prompt.split(QLatin1Char('\n'));

    if (!promptExpanded && lines.size() > 5) {
      html +=
          QStringLiteral(
              "<div style='white-space: pre-wrap; font-family: monospace;'>") +
          lines.mid(0, 5).join(QLatin1Char('\n')).toHtmlEscaped() +
          QStringLiteral("\n...</div>");
      html +=
          QStringLiteral("<div style='margin-top: 10px;'><a "
                         "href='expand:prompt_view' style='font-size: 0.9em; "
                         "color: #3498db; text-decoration: none;'>") +
          i18n("Click to expand prompt") + QStringLiteral("</a></div>");
    } else if (promptExpanded && lines.size() > 5) {
      html +=
          QStringLiteral(
              "<div style='white-space: pre-wrap; font-family: monospace;'>") +
          m_prompt.toHtmlEscaped() + QStringLiteral("</div>");
      html +=
          QStringLiteral("<div style='margin-top: 10px;'><a "
                         "href='collapse:prompt_view' style='font-size: 0.9em; "
                         "color: #3498db; text-decoration: none;'>") +
          i18n("Click to collapse prompt") + QStringLiteral("</a></div>");
    } else {
      html +=
          QStringLiteral(
              "<div style='white-space: pre-wrap; font-family: monospace;'>") +
          m_prompt.toHtmlEscaped() + QStringLiteral("</div>");
    }

    html += QStringLiteral("</div><hr>");
  }

  if (m_activities.isEmpty()) {
    html += QStringLiteral("<p><i>") + i18n("No activity feed available.") +
            QStringLiteral("</i></p>");
  } else {
    QDateTime lastTime;

    // Deduplication logic
    QJsonArray dedupedActivities;
    QList<int> repeatCounts;

    for (int i = 0; i < m_activities.size(); ++i) {
      QJsonObject current = m_activities[i].toObject();
      if (dedupedActivities.isEmpty()) {
        dedupedActivities.append(current);
        repeatCounts.append(1);
      } else {
        QJsonObject previous = dedupedActivities.last().toObject();

        // Compare without dynamic fields
        QJsonObject currComp = current;
        currComp.remove(QStringLiteral("id"));
        currComp.remove(QStringLiteral("name"));
        currComp.remove(QStringLiteral("createTime"));

        QJsonObject prevComp = previous;
        prevComp.remove(QStringLiteral("id"));
        prevComp.remove(QStringLiteral("name"));
        prevComp.remove(QStringLiteral("createTime"));

        if (currComp == prevComp) {
          repeatCounts.last()++;
        } else {
          dedupedActivities.append(current);
          repeatCounts.append(1);
        }
      }
    }

    m_activityJsons.clear();

    for (int i = 0; i < dedupedActivities.size(); ++i) {
      QJsonObject activity = dedupedActivities[i].toObject();
      QString id = activity.value(QStringLiteral("id")).toString();
      if (id.isEmpty())
        id = QString::number(i);

      int count = repeatCounts[i];

      QJsonDocument doc(activity);
      m_activityJsons.insert(
          id, QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));

      QString createTimeStr =
          activity.value(QStringLiteral("createTime")).toString();
      QDateTime createTime =
          QDateTime::fromString(createTimeStr, Qt::ISODateWithMs);
      if (!createTime.isValid())
        createTime = QDateTime::fromString(createTimeStr, Qt::ISODate);

      if (lastTime.isValid() && createTime.isValid()) {
        qint64 secs = lastTime.secsTo(createTime);
        if (secs > 300) { // 5 minutes
          html += QStringLiteral("<div style='text-align:center; color:#999; "
                                 "margin: 10px 0;'>&mdash; ") +
                  i18n("%1 minutes passed", secs / 60) +
                  QStringLiteral(" &mdash;</div>");
        }
      }
      if (createTime.isValid())
        lastTime = createTime;

      bool expanded = m_expandedItems.contains(id);

      QString timeTooltip =
          createTime.isValid()
              ? QLocale::system().toString(createTime, QLocale::LongFormat)
              : i18n("Unknown time");

      QString turnClass = QStringLiteral("turn ");
      if (activity.contains(QStringLiteral("userMessaged"))) {
        turnClass += QStringLiteral("user-turn");
      } else if (activity.contains(QStringLiteral("agentMessaged")) ||
                 activity.contains(QStringLiteral("progressUpdated")) ||
                 activity.contains(QStringLiteral("artifacts")) ||
                 activity.contains(QStringLiteral("planGenerated"))) {
        turnClass += QStringLiteral("agent-turn");
      } else {
        turnClass += QStringLiteral("system-turn");
      }

      html += QStringLiteral("<div class='") + turnClass +
              QStringLiteral("' title='") + timeTooltip.toHtmlEscaped() +
              QStringLiteral("'>");

      // Add a small generic header with a link to open an action menu for this
      // specific block
      html += QStringLiteral("<div style='text-align: right; float: right;'>") +
              QStringLiteral("<a href='action:") + id +
              QStringLiteral("' style='color: #aaa; text-decoration: none; "
                             "font-weight: bold;'>...</a>") +
              QStringLiteral("</div>");

      if (count > 1) {
        if (!expanded) {
          html += QStringLiteral("<div style='color: #888;'><i>") +
                  i18n("Repeated %1 times", count) +
                  QStringLiteral("</i></div>");
          html += QStringLiteral(
                      "<div style='margin-top: 10px;'><a href='expand:") +
                  id +
                  QStringLiteral(
                      "' style='color: #3498db; text-decoration: none;'>") +
                  i18n("Show All") + QStringLiteral("</a></div>");
        } else {
          html += QStringLiteral(
                      "<div style='color: #888; margin-bottom: 10px;'><i>") +
                  i18n("Repeated %1 times", count) +
                  QStringLiteral("</i></div>");
          html += generateHtmlForActivity(activity, expanded);
          html += QStringLiteral(
                      "<div style='margin-top: 15px; border-top: 1px dashed "
                      "#ccc; padding-top: 10px;'><a href='collapse:") +
                  id +
                  QStringLiteral(
                      "' style='color: #3498db; text-decoration: none;'>") +
                  i18n("Hide Repeated Items") + QStringLiteral("</a></div>");
        }
      } else {
        html += generateHtmlForActivity(activity, expanded);
      }

      html += QStringLiteral("<div style='clear: both;'></div>");
      html += QStringLiteral("</div>");
    }
  }

  html += QStringLiteral("</body></html>");
  setHtml(html);
}

QString ActivityBrowser::generateRawJsonHtml(const QJsonObject &activity,
                                             bool expanded) {
  QString id = activity.value(QStringLiteral("id")).toString();
  QJsonDocument doc(activity);
  QString rawJson = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));

  QString html;
  if (!expanded) {
    QStringList lines = rawJson.split(QLatin1Char('\n'));
    if (lines.size() > 6) {
      rawJson =
          lines.mid(0, 6).join(QLatin1Char('\n')) + QStringLiteral("\n...");
      html += QStringLiteral("<div class='code'>") + rawJson.toHtmlEscaped() +
              QStringLiteral("</div>");
      html += QStringLiteral("<a href='expand:") + id + QStringLiteral("'>") +
              i18n("Show more") + QStringLiteral("</a>");
    } else {
      html += QStringLiteral("<div class='code'>") + rawJson.toHtmlEscaped() +
              QStringLiteral("</div>");
    }
  } else {
    html += QStringLiteral("<div class='code'>") + rawJson.toHtmlEscaped() +
            QStringLiteral("</div>");
    html += QStringLiteral("<a href='collapse:") + id + QStringLiteral("'>") +
            i18n("Show less") + QStringLiteral("</a>");
  }
  return html;
}

QString ActivityBrowser::generateHtmlForActivity(const QJsonObject &activity,
                                                 bool expanded) {
  QString id = activity.value(QStringLiteral("id")).toString();
  QString html;

  QString originator = activity.value(QStringLiteral("originator")).toString();
  if (originator.isEmpty()) {
    originator = activity.value(QStringLiteral("author")).toString();
  }

  QString orgSuffix;
  if (!originator.isEmpty()) {
    orgSuffix =
        QStringLiteral(" (") + originator.toHtmlEscaped() + QStringLiteral(")");
  }

  if (activity.contains(QStringLiteral("planGenerated"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Plan Generated") +
            orgSuffix + QStringLiteral("</div>");
    QJsonObject pg = activity.value(QStringLiteral("planGenerated")).toObject();
    QJsonArray actions = pg.value(QStringLiteral("actions")).toArray();
    html += QStringLiteral("<div class='box'>");
    for (int i = 0; i < actions.size(); ++i) {
      html += QStringLiteral("<div>") +
              actions[i]
                  .toObject()
                  .value(QStringLiteral("description"))
                  .toString()
                  .toHtmlEscaped() +
              QStringLiteral("</div>");
    }
    html += QStringLiteral("</div>");

  } else if (activity.contains(QStringLiteral("planApproved"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Plan Approved") +
            orgSuffix + QStringLiteral("</div>");
    QString org = activity.value(QStringLiteral("originator")).toString();
    html += QStringLiteral("<div class='box'>&#10003; ") +
            i18n("Plan approved by %1", org).toHtmlEscaped() +
            QStringLiteral("</div>");

  } else if (activity.contains(QStringLiteral("progressUpdated"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Progress Updated") +
            orgSuffix + QStringLiteral("</div>");
    QJsonObject pu =
        activity.value(QStringLiteral("progressUpdated")).toObject();
    html += QStringLiteral("<div><b>") +
            pu.value(QStringLiteral("title")).toString().toHtmlEscaped() +
            QStringLiteral("</b></div>");

    QJsonObject bo = pu.value(QStringLiteral("bashOutput")).toObject();
    if (!bo.isEmpty()) {
      html += QStringLiteral("<div class='code' style='color:#555;'>$ ") +
              bo.value(QStringLiteral("command")).toString().toHtmlEscaped() +
              QStringLiteral("</div>");
    }

    QString desc = pu.value(QStringLiteral("description")).toString();
    QString out = bo.value(QStringLiteral("output")).toString();
    bool hasDetails = !desc.isEmpty() || !out.isEmpty();

    if (hasDetails) {
      if (expanded) {
        if (!desc.isEmpty()) {
          html += QStringLiteral("<div>") + desc.toHtmlEscaped() +
                  QStringLiteral("</div>");
        }
        if (!out.isEmpty()) {
          html += QStringLiteral("<div class='code'>") + out.toHtmlEscaped() +
                  QStringLiteral("</div>");
        }
        html += QStringLiteral("<br/><a href='collapse:") + id +
                QStringLiteral("' style='color: #888; text-decoration: none; "
                               "font-size: 0.9em;'>") +
                i18n("Show less") + QStringLiteral("</a>");
      } else {
        html += QStringLiteral("<br/><a href='expand:") + id +
                QStringLiteral("' style='color: #888; text-decoration: none; "
                               "font-size: 0.9em;'>") +
                i18n("Show output & description") + QStringLiteral("</a>");
      }
    }

  } else if (activity.contains(QStringLiteral("userMessaged"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Message") + orgSuffix +
            QStringLiteral("</div>");
    html += QStringLiteral("<div class='content'>") +
            activity.value(QStringLiteral("userMessaged"))
                .toObject()
                .value(QStringLiteral("userMessage"))
                .toString()
                .toHtmlEscaped() +
            QStringLiteral("</div>");

  } else if (activity.contains(QStringLiteral("agentMessaged"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Message") + orgSuffix +
            QStringLiteral("</div>");
    html += QStringLiteral("<div class='content'>") +
            activity.value(QStringLiteral("agentMessaged"))
                .toObject()
                .value(QStringLiteral("agentMessage"))
                .toString()
                .toHtmlEscaped() +
            QStringLiteral("</div>");

  } else if (activity.contains(QStringLiteral("sessionCompleted"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Session Completed") +
            orgSuffix + QStringLiteral("</div>");
    QJsonObject sc =
        activity.value(QStringLiteral("sessionCompleted")).toObject();
    QString prUrl;
    QString prTitle;
    QString prBody;
    QString prHeadBranch;
    QString prBaseBranch;

    if (sc.contains(QStringLiteral("pullRequest"))) {
      QJsonObject pr = sc.value(QStringLiteral("pullRequest")).toObject();
      prUrl = pr.value(QStringLiteral("url")).toString();
      prTitle = pr.value(QStringLiteral("title")).toString();
      prBody = pr.value(QStringLiteral("body")).toString();
      prHeadBranch = pr.value(QStringLiteral("headBranch")).toString();
      prBaseBranch = pr.value(QStringLiteral("baseBranch")).toString();
    } else if (activity.contains(QStringLiteral("outputs"))) {
      QJsonArray outs = activity.value(QStringLiteral("outputs")).toArray();
      for (int i = 0; i < outs.size(); ++i) {
        if (outs[i].toObject().contains(QStringLiteral("pullRequest"))) {
          QJsonObject pr = outs[i]
                               .toObject()
                               .value(QStringLiteral("pullRequest"))
                               .toObject();
          prUrl = pr.value(QStringLiteral("url")).toString();
          prTitle = pr.value(QStringLiteral("title")).toString();
          prBody = pr.value(QStringLiteral("body")).toString();
          prHeadBranch = pr.value(QStringLiteral("headBranch")).toString();
          prBaseBranch = pr.value(QStringLiteral("baseBranch")).toString();
        }
      }
    }

    if (!prTitle.isEmpty()) {
      html += QStringLiteral("<div class='box'>");
      html += QStringLiteral("<div style='margin-bottom: 5px; font-weight: "
                             "bold; font-size: 1.1em;'>") +
              prTitle.toHtmlEscaped() + QStringLiteral("</div>");
      if (!prHeadBranch.isEmpty() && !prBaseBranch.isEmpty()) {
        html += QStringLiteral(
                    "<div style='margin-bottom: 10px; font-family: monospace; "
                    "color: #555; background-color: #f1f1f1; padding: 4px; "
                    "border-radius: 4px; display: inline-block;'>") +
                prBaseBranch.toHtmlEscaped() + QStringLiteral(" &larr; ") +
                prHeadBranch.toHtmlEscaped() + QStringLiteral("</div>");
      }
      if (!prBody.isEmpty()) {
        html += QStringLiteral(
                    "<div style='white-space: pre-wrap; color: #444;'>") +
                prBody.toHtmlEscaped() + QStringLiteral("</div>");
      }
      html += QStringLiteral("</div>");
    }

    html +=
        QStringLiteral("<div style='text-align: center; margin-top: 15px;'>");
    if (!prUrl.isEmpty()) {
      html += QStringLiteral("<a class='btn' style='margin-right: 10px; "
                             "background-color: #2ecc71;' href='") +
              prUrl.toHtmlEscaped() + QStringLiteral("'>") +
              i18n("View Pull Request") + QStringLiteral("</a>");
    }
    html += QStringLiteral(
                "<a class='btn' href='https://jules.google.com/sessions/") +
            activity.value(QStringLiteral("name"))
                .toString()
                .split(QLatin1Char('/'))
                .value(1) +
            QStringLiteral("'>") + i18n("View on Jules") +
            QStringLiteral("</a>");
    html += QStringLiteral("</div>");

  } else if (activity.contains(QStringLiteral("artifacts"))) {
    html += QStringLiteral("<div class='role'>") + i18n("Artifacts") +
            orgSuffix + QStringLiteral("</div>");
    QJsonArray artifacts =
        activity.value(QStringLiteral("artifacts")).toArray();
    for (int i = 0; i < artifacts.size(); ++i) {
      QJsonObject art = artifacts[i].toObject();
      if (art.contains(QStringLiteral("changeSet"))) {
        QJsonObject cs = art.value(QStringLiteral("changeSet")).toObject();
        QString source = cs.value(QStringLiteral("source")).toString();
        source.replace(QStringLiteral("sources/github/"), QStringLiteral(""));

        QJsonObject gp = cs.value(QStringLiteral("gitPatch")).toObject();
        QString commitId = gp.value(QStringLiteral("baseCommitId")).toString();

        html += QStringLiteral("<div class='box'>");
        html += QStringLiteral("<b>") + i18n("Commit:") +
                QStringLiteral("</b> <a href='https://github.com/") +
                source.toHtmlEscaped() + QStringLiteral("/commit/") +
                commitId.toHtmlEscaped() + QStringLiteral("'>") +
                commitId.toHtmlEscaped() + QStringLiteral("</a><br/>");

        QString sugg =
            gp.value(QStringLiteral("suggestedCommitMessage")).toString();
        if (!sugg.isEmpty()) {
          html += QStringLiteral("<div class='code'>") + sugg.toHtmlEscaped() +
                  QStringLiteral("</div>");
        }

        QString patch = gp.value(QStringLiteral("unidiffPatch")).toString();
        if (!patch.isEmpty()) {
          html += QStringLiteral("<a href='diff:") + id + QStringLiteral("'>") +
                  i18n("View Patch") + QStringLiteral("</a>");
        }
        html += QStringLiteral("</div>");
      } else if (art.contains(QStringLiteral("media"))) {
        QJsonObject media = art.value(QStringLiteral("media")).toObject();
        QString mime = media.value(QStringLiteral("mimeType")).toString();
        if (mime.startsWith(QStringLiteral("image/"))) {
          QString b64 = media.value(QStringLiteral("data")).toString();
          html += QStringLiteral("<a href='img:") + id +
                  QStringLiteral("'><img src='data:") + mime +
                  QStringLiteral(";base64,") + b64 +
                  QStringLiteral("' style='max-width:200px; max-height:200px; "
                                 "border: 1px solid #ccc;'/></a>");
        }
      } else {
        html += generateRawJsonHtml(art, expanded);
      }
    }
  } else {
    // Unknown or fallback
    QString role = activity.value(QStringLiteral("role")).toString();
    if (role.isEmpty())
      role = activity.value(QStringLiteral("author")).toString();
    if (role.isEmpty())
      role = QStringLiteral("Unknown");

    html += QStringLiteral("<div class='role'>") + role.toHtmlEscaped() +
            orgSuffix + QStringLiteral("</div>");

    QString content = activity.value(QStringLiteral("content")).toString();
    if (content.isEmpty())
      content = activity.value(QStringLiteral("text")).toString();

    if (!content.isEmpty()) {
      html += QStringLiteral("<div class='content'>") +
              content.toHtmlEscaped() + QStringLiteral("</div>");
    } else {
      html += generateRawJsonHtml(activity, expanded);
    }
  }
  return html;
}

void ActivityBrowser::onAnchorClicked(const QUrl &url) {
  QString scheme = url.scheme();
  QString path = url.path();

  if (scheme == QStringLiteral("expand")) {
    m_expandedItems.insert(path);
    renderHtml();
  } else if (scheme == QStringLiteral("collapse")) {
    m_expandedItems.remove(path);
    renderHtml();
  } else if (scheme == QStringLiteral("diff")) {
    // Find the diff
    QString diffText;
    for (int i = 0; i < m_activities.size(); ++i) {
      QJsonObject activity = m_activities[i].toObject();
      QString id = activity.value(QStringLiteral("id")).toString();
      if (id.isEmpty())
        id = QString::number(i);
      if (id == path) {
        QJsonArray artifacts =
            activity.value(QStringLiteral("artifacts")).toArray();
        for (int j = 0; j < artifacts.size(); ++j) {
          QJsonObject art = artifacts[j].toObject();
          if (art.contains(QStringLiteral("changeSet"))) {
            diffText = art.value(QStringLiteral("changeSet"))
                           .toObject()
                           .value(QStringLiteral("gitPatch"))
                           .toObject()
                           .value(QStringLiteral("unidiffPatch"))
                           .toString();
          }
        }
      }
    }
    if (!diffText.isEmpty()) {
      QDialog *dlg = new QDialog(this);
      dlg->setWindowTitle(i18n("Patch Viewer"));
      dlg->resize(800, 600);
      QVBoxLayout *l = new QVBoxLayout(dlg);
      QTextBrowser *tb = new QTextBrowser(dlg);
      tb->setStyleSheet(QStringLiteral("font-family: monospace;"));
      tb->setPlainText(diffText);
      l->addWidget(tb);
      dlg->setAttribute(Qt::WA_DeleteOnClose);
      dlg->show();
    }
  } else if (scheme == QStringLiteral("action")) {
    QMenu menu(this);
    QAction *copyIdAction = menu.addAction(i18n("Copy ID"));
    connect(copyIdAction, &QAction::triggered, this,
            [path]() { QGuiApplication::clipboard()->setText(path); });

    QAction *showRawAction = menu.addAction(i18n("Show Raw JSON"));
    connect(showRawAction, &QAction::triggered, this, [this, path]() {
      QString rawJson = m_activityJsons.value(path);
      if (!rawJson.isEmpty()) {
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle(i18n("Raw Activity JSON"));
        dlg->resize(600, 400);
        QVBoxLayout *l = new QVBoxLayout(dlg);
        QTextBrowser *tb = new QTextBrowser(dlg);
        tb->setStyleSheet(QStringLiteral("font-family: monospace;"));
        tb->setPlainText(rawJson);
        l->addWidget(tb);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
      }
    });

    menu.exec(QCursor::pos());
  } else if (scheme == QStringLiteral("raw")) {
    QString rawJson = m_activityJsons.value(path);
    if (!rawJson.isEmpty()) {
      QDialog *dlg = new QDialog(this);
      dlg->setWindowTitle(i18n("Raw Activity JSON"));
      dlg->resize(600, 400);
      QVBoxLayout *l = new QVBoxLayout(dlg);
      QTextBrowser *tb = new QTextBrowser(dlg);
      tb->setStyleSheet(QStringLiteral("font-family: monospace;"));
      tb->setPlainText(rawJson);
      l->addWidget(tb);
      dlg->setAttribute(Qt::WA_DeleteOnClose);
      dlg->show();
    }
  } else if (scheme == QStringLiteral("img")) {
    QString b64;
    for (int i = 0; i < m_activities.size(); ++i) {
      QJsonObject activity = m_activities[i].toObject();
      QString id = activity.value(QStringLiteral("id")).toString();
      if (id.isEmpty())
        id = QString::number(i);
      if (id == path) {
        QJsonArray artifacts =
            activity.value(QStringLiteral("artifacts")).toArray();
        for (int j = 0; j < artifacts.size(); ++j) {
          QJsonObject art = artifacts[j].toObject();
          if (art.contains(QStringLiteral("media"))) {
            b64 = art.value(QStringLiteral("media"))
                      .toObject()
                      .value(QStringLiteral("data"))
                      .toString();
          }
        }
      }
    }
    if (!b64.isEmpty()) {
      QByteArray ba = QByteArray::fromBase64(b64.toUtf8());
      QImage img = QImage::fromData(ba);
      if (!img.isNull()) {
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle(i18n("Image Viewer"));
        QVBoxLayout *l = new QVBoxLayout(dlg);
        QLabel *lbl = new QLabel(dlg);
        lbl->setPixmap(QPixmap::fromImage(img));
        l->addWidget(lbl);

        // resize up to screen size
        QSize screenSize = QApplication::primaryScreen()->availableSize();
        QSize imgSize = img.size();
        int w = qMin(imgSize.width() + 40, screenSize.width() - 100);
        int h = qMin(imgSize.height() + 40, screenSize.height() - 100);
        dlg->resize(w, h);

        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
      }
    }
  } else if (scheme == QStringLiteral("http") ||
             scheme == QStringLiteral("https")) {
    QDesktopServices::openUrl(url);
  }
}

void ActivityBrowser::onCustomContextMenu(const QPoint &pos) {
  QMenu menu(this);

  QString clickedId; // In a real browser we might map pos to block, but
                     // QTextBrowser doesn't make this trivial
  // Let's just provide a generic "Copy All JSON" for now, or if they right
  // clicked near text we can't easily extract the ID. However, QTextBrowser
  // cursor gives us block.
  QString anchor = anchorAt(pos);

  QAction *copyAction = menu.addAction(i18n("Copy Selected Text"));
  connect(copyAction, &QAction::triggered, this, [this]() {
    QGuiApplication::clipboard()->setText(textCursor().selectedText());
  });

  // If the user right clicked exactly on the '...' link, add an explicit entry
  if (anchor.startsWith(QStringLiteral("action:"))) {
    QAction *blockRawAction = menu.addAction(i18n("Show Block Raw JSON"));
    QString id = anchor.mid(7);
    connect(blockRawAction, &QAction::triggered, this, [this, id]() {
      onAnchorClicked(QUrl(QStringLiteral("raw:") + id));
    });
  }

  QAction *rawAction = menu.addAction(i18n("Show Complete Raw Feed JSON"));
  connect(rawAction, &QAction::triggered, this, [this]() {
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(i18n("Raw Activities JSON Feed"));
    dlg->resize(800, 600);
    QVBoxLayout *l = new QVBoxLayout(dlg);
    QTextBrowser *tb = new QTextBrowser(dlg);
    tb->setStyleSheet(QStringLiteral("font-family: monospace;"));
    QJsonDocument doc(m_activities);
    tb->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    l->addWidget(tb);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
  });

  menu.exec(mapToGlobal(pos));
}
