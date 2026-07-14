1. **Understand the user's request:**
   The user asks: "did you make it cache the value against the hash of the token?"
   Currently, `APIManager` tests the token each time `testGithubConnection` is called (which we call every time the dialog opens). The user wants us to cache the scopes/permissions test result so we don't hit the GitHub API repeatedly if the token hasn't changed.
   Actually, `APIManager` currently calls `Q_EMIT githubConnectionTested(...)` after testing.
   Wait, if we cache the token and scopes, we can just return the cached scopes if the token hasn't changed since the last test.
   In `APIManager::testGithubConnection`:
   We can add `QString m_testedGithubToken;` (or hash of it) to avoid storing it twice, but `m_githubToken` is already stored.
   Wait, `APIManager::testGithubConnection` is called from `CreateRepoDialog::showEvent`.
   We can cache `m_githubScopes` and `m_githubUsername`. If `m_testedGithubToken == token` (or `m_githubToken`), we can just emit `githubConnectionTested(true, ...)` immediately with the cached result instead of hitting the API again.

2. **Implement caching in `APIManager`**:
   - In `apimanager.h`: Add `QString m_testedGithubToken;`
   - In `apimanager.cpp` `testGithubConnection`:
     ```cpp
     QString tk = token.isEmpty() ? m_githubToken : token;
     if (tk == m_testedGithubToken && !m_githubScopes.isEmpty()) {
       Q_EMIT githubUsernameFetched(m_githubUsername);
       Q_EMIT githubConnectionTested(true, QStringLiteral("Cached connection."));
       return;
     }
     ```
     Wait, if we cache it, we should update `m_testedGithubToken` on success.
     ```cpp
     if (reply->error() == QNetworkReply::NoError) {
         m_testedGithubToken = tk;
         // ...
     }
     ```

   Let's check what `testGithubConnection` currently does.
