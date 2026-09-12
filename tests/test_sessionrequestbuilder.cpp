#include "../src/sessionrequestbuilder.h"

#include <QJsonObject>
#include <QtTest>

class SessionRequestBuilderTest : public QObject {
  Q_OBJECT

private Q_SLOTS:
  void createsDocumentedFullRequest();
  void createsMinimalRepolessRequest();
  void createsDefaultBranchRequestWithoutRepoContext();
  void preservesSourceResourceNameVerbatim();
  void includesExplicitBranchForOpaqueSourceName();
  void preservesCanonicalRequestWhenRetryingError();
  void unwrapsRequestFromApplicationMetadata();
  void omitsEmptyOptionalFields();
  void preservesExplicitFalsePlanApproval();
  void excludesApplicationFields();
  void createsDocumentedSendMessageRequest();
  void embedsApiRequestInSessionResponse();
  void buildsCanonicalSessionRequestWithAllFields();
  void normalizesPlanApprovalAndActionFields();
};

void SessionRequestBuilderTest::createsDocumentedFullRequest() {
  const QJsonObject input{{QStringLiteral("prompt"), QStringLiteral("Add tests")},
                          {QStringLiteral("title"), QStringLiteral("Test task")},
                          {QStringLiteral("source"), QStringLiteral("sources/github/example/repo")},
                          {QStringLiteral("startingBranch"), QStringLiteral("develop")},
                          {QStringLiteral("requirePlanApproval"), true},
                          {QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")}};

  const QJsonObject actual = SessionRequestBuilder::createSession(input);
  const QJsonObject expected{
      {QStringLiteral("prompt"), QStringLiteral("Add tests")},
      {QStringLiteral("title"), QStringLiteral("Test task")},
      {QStringLiteral("sourceContext"),
       QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/github/example/repo")},
                   {QStringLiteral("githubRepoContext"),
                    QJsonObject{{QStringLiteral("startingBranch"), QStringLiteral("develop")}}}}},
      {QStringLiteral("requirePlanApproval"), true},
      {QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")}};
  QCOMPARE(actual, expected);
}

void SessionRequestBuilderTest::createsMinimalRepolessRequest() {
  const QJsonObject actual =
      SessionRequestBuilder::createSession({{QStringLiteral("prompt"), QStringLiteral("Explain this code")}});
  QCOMPARE(actual, QJsonObject({{QStringLiteral("prompt"), QStringLiteral("Explain this code")}}));
  QVERIFY(!actual.contains(QStringLiteral("sourceContext")));
}

void SessionRequestBuilderTest::createsDefaultBranchRequestWithoutRepoContext() {
  const QJsonObject actual =
      SessionRequestBuilder::createSession({{QStringLiteral("prompt"), QStringLiteral("Add verification notes")},
                                            {QStringLiteral("source"), QStringLiteral("sources/arran4/goa4web")},
                                            {QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")}});
  const QJsonObject expected{{QStringLiteral("prompt"), QStringLiteral("Add verification notes")},
                             {QStringLiteral("sourceContext"),
                              QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/arran4/goa4web")}}},
                             {QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")}};
  QCOMPARE(actual, expected);
  QVERIFY(!actual.value(QStringLiteral("sourceContext")).toObject().contains(QStringLiteral("githubRepoContext")));
}

void SessionRequestBuilderTest::preservesSourceResourceNameVerbatim() {
  const QJsonObject actual = SessionRequestBuilder::createSession(
      {{QStringLiteral("prompt"), QStringLiteral("Task")},
       {QStringLiteral("source"), QStringLiteral("opaque/provider/example/repo")}});
  QCOMPARE(actual.value(QStringLiteral("sourceContext")).toObject().value(QStringLiteral("source")).toString(),
           QStringLiteral("opaque/provider/example/repo"));
}

void SessionRequestBuilderTest::includesExplicitBranchForOpaqueSourceName() {
  const QJsonObject actual =
      SessionRequestBuilder::createSession({{QStringLiteral("prompt"), QStringLiteral("Task")},
                                            {QStringLiteral("source"), QStringLiteral("sources/arran4/goa4web")},
                                            {QStringLiteral("startingBranch"), QStringLiteral("feature/tests")}});
  const QJsonObject sourceContext = actual.value(QStringLiteral("sourceContext")).toObject();
  QCOMPARE(sourceContext.value(QStringLiteral("source")).toString(), QStringLiteral("sources/arran4/goa4web"));
  QCOMPARE(sourceContext.value(QStringLiteral("githubRepoContext"))
               .toObject()
               .value(QStringLiteral("startingBranch"))
               .toString(),
           QStringLiteral("feature/tests"));
}

void SessionRequestBuilderTest::preservesCanonicalRequestWhenRetryingError() {
  const QJsonObject canonicalRequest{
      {QStringLiteral("prompt"), QStringLiteral("Add verification notes")},
      {QStringLiteral("sourceContext"),
       QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/arran4/goa4web")},
                   {QStringLiteral("githubRepoContext"),
                    QJsonObject{{QStringLiteral("startingBranch"), QStringLiteral("feature/verification")}}}}},
      {QStringLiteral("automationMode"), QStringLiteral("AUTO_CREATE_PR")}};

  QCOMPARE(SessionRequestBuilder::createSession(canonicalRequest), canonicalRequest);
}

void SessionRequestBuilderTest::unwrapsRequestFromApplicationMetadata() {
  const QJsonObject canonicalRequest{
      {QStringLiteral("prompt"), QStringLiteral("Task")},
      {QStringLiteral("sourceContext"),
       QJsonObject{{QStringLiteral("source"), QStringLiteral("sources/arran4/goa4web")},
                   {QStringLiteral("githubRepoContext"),
                    QJsonObject{{QStringLiteral("startingBranch"), QStringLiteral("release")}}}}}};
  const QJsonObject wrapper{{QStringLiteral("request"), canonicalRequest},
                            {QStringLiteral("_kjules_failed_action"), QStringLiteral("send_now")},
                            {QStringLiteral("_kjules_requeue_origin_row"), -1},
                            {QStringLiteral("_kjules_requeue_source_is_queue"), false}};

  QCOMPARE(SessionRequestBuilder::createSession(wrapper), canonicalRequest);
}

void SessionRequestBuilderTest::omitsEmptyOptionalFields() {
  const QJsonObject actual =
      SessionRequestBuilder::createSession({{QStringLiteral("prompt"), QStringLiteral("Task")},
                                            {QStringLiteral("title"), QString()},
                                            {QStringLiteral("automationMode"), QString()},
                                            {QStringLiteral("source"), QStringLiteral("github/example/repo")},
                                            {QStringLiteral("startingBranch"), QString()}});
  QVERIFY(!actual.contains(QStringLiteral("title")));
  QVERIFY(!actual.contains(QStringLiteral("automationMode")));
  QVERIFY(!actual.value(QStringLiteral("sourceContext")).toObject().contains(QStringLiteral("githubRepoContext")));
}

void SessionRequestBuilderTest::preservesExplicitFalsePlanApproval() {
  const QJsonObject actual = SessionRequestBuilder::createSession(
      {{QStringLiteral("prompt"), QStringLiteral("Task")}, {QStringLiteral("requirePlanApproval"), false}});
  QVERIFY(actual.contains(QStringLiteral("requirePlanApproval")));
  QCOMPARE(actual.value(QStringLiteral("requirePlanApproval")).toBool(), false);
}

void SessionRequestBuilderTest::excludesApplicationFields() {
  QJsonObject input{{QStringLiteral("prompt"), QStringLiteral("Task")},
                    {QStringLiteral("priority"), 10},
                    {QStringLiteral("ignoreConcurrency"), true},
                    {QStringLiteral("request"), QJsonObject{}},
                    {QStringLiteral("_kjules_failed_action"), QStringLiteral("send_now")},
                    {QStringLiteral("_kjules_requeue_origin_row"), -1},
                    {QStringLiteral("_kjules_requeue_source_is_queue"), false},
                    {QStringLiteral("_kjules_arbitrary"), QStringLiteral("private")}};
  const QJsonObject actual = SessionRequestBuilder::createSession(input);
  QCOMPARE(actual.keys(), QStringList{QStringLiteral("prompt")});
}

void SessionRequestBuilderTest::createsDocumentedSendMessageRequest() {
  QCOMPARE(SessionRequestBuilder::sendMessage(QStringLiteral("Continue with tests")),
           QJsonObject({{QStringLiteral("prompt"), QStringLiteral("Continue with tests")}}));
}

void SessionRequestBuilderTest::embedsApiRequestInSessionResponse() {
  const QJsonObject request{{QStringLiteral("prompt"), QStringLiteral("Task")}};
  const QJsonObject response{{QStringLiteral("name"), QStringLiteral("sessions/123")}};
  const QJsonObject actual = SessionRequestBuilder::sessionResponseWithRequest(response, request);
  QCOMPARE(actual.value(QStringLiteral("request")).toObject(), request);
  QCOMPARE(actual.value(QStringLiteral("name")).toString(), QStringLiteral("sessions/123"));

  const QJsonObject emptyRequestResult = SessionRequestBuilder::sessionResponseWithRequest(response, {});
  QVERIFY(emptyRequestResult.contains(QStringLiteral("request")));
  QCOMPARE(emptyRequestResult.value(QStringLiteral("request")).toObject(), QJsonObject{});
}

void SessionRequestBuilderTest::buildsCanonicalSessionRequestWithAllFields() {
  const QJsonObject req = SessionRequestBuilder::buildSessionRequest(
      QStringLiteral("sources/github/testorg/testrepo"), QStringLiteral("feature-branch"),
      QStringLiteral("Fix bug in calculation"), QStringLiteral("AUTO_CREATE_PR"), true /* requirePlanApproval */,
      true /* ignoreConcurrency */, 42 /* priority */, QStringLiteral("send_now") /* queueAction */);

  QCOMPARE(req.value(QStringLiteral("source")).toString(), QStringLiteral("sources/github/testorg/testrepo"));
  QCOMPARE(req.value(QStringLiteral("startingBranch")).toString(), QStringLiteral("feature-branch"));
  QCOMPARE(req.value(QStringLiteral("prompt")).toString(), QStringLiteral("Fix bug in calculation"));
  QCOMPARE(req.value(QStringLiteral("automationMode")).toString(), QStringLiteral("AUTO_CREATE_PR"));
  QCOMPARE(req.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QCOMPARE(req.value(QStringLiteral("ignoreConcurrency")).toBool(), true);
  QCOMPARE(req.value(QStringLiteral("priority")).toInt(), 42);
  QCOMPARE(req.value(QStringLiteral("_kjules_action")).toString(), QStringLiteral("send_now"));
}

void SessionRequestBuilderTest::normalizesPlanApprovalAndActionFields() {
  QJsonObject input;
  input[QStringLiteral("prompt")] = QStringLiteral("Variant prompt");
  input[QStringLiteral("planApproval")] = true;
  input[QStringLiteral("queueAction")] = QStringLiteral("send_next");
  input[QStringLiteral("ignoreConcurrency")] = true;
  input[QStringLiteral("priority")] = 99;

  const QJsonObject normalized = SessionRequestBuilder::normalizeSessionRequest(input);
  QCOMPARE(normalized.value(QStringLiteral("prompt")).toString(), QStringLiteral("Variant prompt"));
  QCOMPARE(normalized.value(QStringLiteral("requirePlanApproval")).toBool(), true);
  QCOMPARE(normalized.value(QStringLiteral("_kjules_action")).toString(), QStringLiteral("send_next"));
  QCOMPARE(normalized.value(QStringLiteral("ignoreConcurrency")).toBool(), true);
  QCOMPARE(normalized.value(QStringLiteral("priority")).toInt(), 99);

  // createSession also honors planApproval
  const QJsonObject apiReq = SessionRequestBuilder::createSession(input);
  QCOMPARE(apiReq.value(QStringLiteral("requirePlanApproval")).toBool(), true);
}

QTEST_MAIN(SessionRequestBuilderTest)
#include "test_sessionrequestbuilder.moc"
