Benchmarks should be placed in the `/bench` directory.

## Terminology

*   **Managed Sessions**: Sessions explicitly watched by the user, stored in `cached_watching_sessions.json`. These are shown in the main "Watching" tab and automatically refresh on a timer.
*   **Archived Sessions**: Sessions the user has moved to the "Archive" tab (`cached_archive_sessions.json`), essentially stopping updates but keeping the history.
*   **Unmanaged Sessions / All Sessions**: Raw sessions fetched directly from the API that the user is not explicitly tracking. They reside in `cached_all_sessions.json` (a cache file, not persistent app data) and are viewed via the `SessionsWindow`. Users can adopt an unmanaged session via the "Watch" action.

* To build the project, ensure all required KF6 dependencies are installed, particularly `libkf6wallet-dev` and `libkf6notifications-dev`.
