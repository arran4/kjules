1. **Audit Criteria 1:** Re-audit all #417 criteria, verify with actual UI tests (e.g. `test_sessionwindow.cpp`, `test_sourcewindow.cpp`), integration (`test_job.cpp`), and exact code lines.
2. **Audit Criteria 2:** Review "Errors Tab". It seems #417 says "Remove the Errors tab once Job/attempt diagnostics and global diagnostics have replacement presentation." But currently, it still exists. The issue is likely #406 or related. I will check whether #406 satisfied this or if it remains a gap for a separate tracking issue. (And I'll record it).
3. **Audit Criteria 3:** Recheck retry, concurrent, winner selection, etc. with integration tests proving state/persistence.
4. **Audit Criteria 4:** Expand the #425 validation checklist.
5. **Audit Criteria 5:** Verify PR #429 is merged. (Checked git log: `49261ff Merge pull request #429`).
6. **Audit Criteria 6:** Run `format`, `lint`, and `build` separately, plus tests.
7. **Audit Criteria 7:** Commit the generated report as a repository document (e.g. `docs/audit_425_417.md`) and submit the PR.
