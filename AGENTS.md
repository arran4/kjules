Benchmarks should be placed in the `/bench` directory.

- Managed sessions are those that are in the main window (e.g. `m_sessionModel` and `m_archiveModel`). They are treated as persistent user data and stored in the AppData directory.
- The "Follow" action means adopting an unmanaged session (e.g. from All Sessions list) as a managed kjules session.
- The "All Sessions" list (`SessionsWindow`) is strictly a cache (`cached_all_sessions.json`) and is stored in the system Cache directory. Refreshing it uses a progressive upsert logic (updating existing entries rather than clearing the list) and syncs those updates directly to any followed/managed sessions.
