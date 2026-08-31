1. **Apply Code Review Feedback - Header**
   - Use `replace_with_git_merge_diff` on `src/legacydatarepair.h` to add `followingSkippedInvalid` and `queueSkippedInvalid` integer counters to `LegacyDataRepairResult`. Add `fatalError` string to separate fatal backup errors from non-fatal component warnings (which remain in `error`).
   - Verify with `git diff`.

2. **Apply Code Review Feedback - Source Implementation**
   - Use `replace_with_git_merge_diff` on `src/legacydatarepair.cpp` to update `parseLegacySessions` and `parseLegacyQueue`: if `doc.isObject()` but lacks the exact `"sessions"` or `"items"` key, set `outIsMalformed = true` instead of silently returning empty arrays.
   - Use `replace_with_git_merge_diff` on `src/legacydatarepair.cpp` in `analyze()` to increment `followingSkippedInvalid` if `id.isEmpty()` instead of silently skipping, and similarly increment `queueSkippedInvalid` for missing/empty `requestData` rather than using fallback reconstruction logic.
   - Use `replace_with_git_merge_diff` on `src/legacydatarepair.cpp` in `performMerge()` to assign backup failures to `result.fatalError` instead of `result.error`. Add `sessionModel->saveSessions();` after adding sessions.
   - Verify with `git diff`.

3. **Apply Code Review Feedback - UI Implementation**
   - Use `replace_with_git_merge_diff` on `src/mainwindow.cpp` to correctly consume the new `LegacyDataRepairResult` properties: display the `skipped` counts in the prompt, and check `result.fatalError` instead of `result.error` for the critical abort dialog.
   - Verify with `git diff`.

4. **Update Tests**
   - Use `replace_with_git_merge_diff` on `tests/test_legacydatarepair.cpp` to modify the malformed JSON and mock failure tests to check the updated strict schema requirements and newly decoupled `fatalError`/`skipped` properties.
   - Verify with `git diff`.

5. **Format codebase**
   - Run `find src tests bench -name "*.h" -o -name "*.cpp" | xargs -r clang-format -i` via `run_in_bash_session` to maintain formatting constraints.

6. **Cleanup**
   - Run `rm plan.md` via `run_in_bash_session` to remove execution artifacts from the commit.

7. **Run Tests**
   - Use `run_in_bash_session` to execute `.jules/run.sh cmake --build build --parallel` and `.jules/run.sh ctest --test-dir build --output-on-failure`.

8. **Pre-commit Instructions**
   - Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
