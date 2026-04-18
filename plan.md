1. **Update `NewSessionDialog` Header**:
   - In `src/newsessiondialog.h`, add an override for `closeEvent(QCloseEvent *event)` so we can save the geometry when the dialog is closed.

2. **Restore Window Geometry on Creation**:
   - In `src/newsessiondialog.cpp`, include `<KConfigGroup>` and `<KSharedConfig>`.
   - In the constructor of `NewSessionDialog`, open a `KConfigGroup` named `"NewSessionDialog"`.
   - Read the `"Geometry"` entry and call `restoreGeometry()` to apply the previous window position and size. (Qt's `QWidget` class, which `QDialog` inherits from, natively supports `saveGeometry()` and `restoreGeometry()`).
   - If `restoreGeometry()` returns false (i.e. no previous geometry was saved), ensure a sane fallback using `resize(700, 600)`.

3. **Save Window Geometry on Close**:
   - Implement `closeEvent(QCloseEvent *event)` in `src/newsessiondialog.cpp`.
   - In `closeEvent`, open the `"NewSessionDialog"` `KConfigGroup` and write `saveGeometry()` to the `"Geometry"` entry.
   - Sync the config to ensure it saves.

4. **Build and Test**:
   - Run `cmake -B build -DQT_MAJOR_VERSION=5 && cmake --build build && cd build && QT_QPA_PLATFORM=offscreen ctest` to ensure nothing is broken.

5. **Pre-commit Steps**:
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.

6. **Submit Change**:
   - Submit the change with a descriptive commit message.
