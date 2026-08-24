import re

with open('src/apimanager.cpp', 'r') as f:
    content = f.read()

# Replace simple error emissions with ApiError
content = re.sub(
    r'Q_EMIT githubIssueContextFailed\(sourceId, issueNumber,\n\s*QStringLiteral\("No valid GitHub token or rate limit exhausted\."\)\);',
    '''ApiError err; err.setType(ApiError::Type::Authentication); err.setMessage(QStringLiteral("No valid GitHub token or rate limit exhausted."));
    Q_EMIT githubIssueContextFailed(sourceId, issueNumber, err);''',
    content
)

content = re.sub(
    r'Q_EMIT githubIssueContextFailed\(sourceId, issueNumber, QStringLiteral\("Invalid source metadata\."\)\);',
    '''ApiError err; err.setType(ApiError::Type::Validation); err.setMessage(QStringLiteral("Invalid source metadata."));
    Q_EMIT githubIssueContextFailed(sourceId, issueNumber, err);''',
    content
)

content = re.sub(
    r'Q_EMIT githubIssueContextFailed\(sId, iNum, QStringLiteral\("Rate limit exhausted while fetching comments\."\)\);',
    '''ApiError err; err.setType(ApiError::Type::RateLimit); err.setMessage(QStringLiteral("Rate limit exhausted while fetching comments."));
          Q_EMIT githubIssueContextFailed(sId, iNum, err);''',
    content
)

content = re.sub(
    r'Q_EMIT githubIssueContextFailed\(sId, iNum,\n\s*QStringLiteral\("Failed to fetch comments: "\) \+ rep->errorString\(\)\);',
    'Q_EMIT githubIssueContextFailed(sId, iNum, ApiErrorDetector::detect(rep, rep->readAll()));',
    content
)

content = re.sub(
    r'Q_EMIT githubIssueContextFailed\(sourceId, issueNumber,\n\s*QStringLiteral\("Failed to fetch issue context: "\) \+ reply->errorString\(\)\);',
    'Q_EMIT githubIssueContextFailed(sourceId, issueNumber, ApiErrorDetector::detect(reply, reply->readAll()));',
    content
)

with open('src/apimanager.cpp', 'w') as f:
    f.write(content)
