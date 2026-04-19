# CI Evaluation: Sequential vs Parallel Jobs

## 1. Current Workflow Critical Path Analysis
Based on test executions, the current sequential `cpp-qt-build-test` job has the following approximate time breakdown:
- **Dependency Setup (apt-get update & install)**: ~60-90 seconds (installing heavy Qt5/KF5 dev packages, compilers, and linting tools).
- **Linting (clang-format, cppcheck)**: ~4-5 seconds.
- **CMake Configure & Clang-Tidy**: ~10 seconds.
- **Build (CMake)**: ~35 seconds.
- **Test (CTest)**: < 1 second.
- **Total Critical Path**: ~110-140 seconds.

## 2. Comparison: Sequential vs. Parallel

### Current Sequential Approach
Everything runs sequentially on a single runner. The total execution time is roughly the sum of the above (~110-140s). All dependencies are downloaded and installed once.

### Parallel Approach (Split Lint and Build/Test)
If split into a **Lint Job** and a **Build/Test Job**:
- **Lint Job**: Requires `clang-format` and `cppcheck`. Startup, checkout, and smaller dependency installation takes ~20-30s. Linting takes ~5s. Total: ~25-35s.
- **Build/Test Job**: Still requires the heavy Qt5/KF5 dev dependencies. Setup (~60-90s) + Config/Tidy (~10s) + Build (~35s) + Test (<1s). Total: ~105-135s.

## 3. Tradeoffs

- **Duplicated Dependency Installation & Container Overhead:** Both parallel jobs must provision a runner, checkout code, and run `apt-get update`. This introduces duplicate network operations and container startup overhead (typically 5-10s per runner).
- **CI Cost/Minutes:** Splitting into two jobs increases the total billable execution time due to the overlapping setup phases, without saving much wall-clock time.
- **Critical Path Reduction:** The Build/Test job is the critical path. Parallelizing only shaves off the ~5s linting time from the build critical path. This ~5s saving might be completely negated by runner provisioning jitter.
- **Release-Job Dependency Fan-In:** The `publish-draft` job currently waits for `cpp-qt-build-test`. If split, release jobs would need to depend on both `lint` and `build` jobs (fan-in), complicating the workflow graph unnecessarily.
- **Cache Behavior:** Currently, there's no cross-job caching. Splitting jobs doesn't help with caching, and keeping them together simplifies potential future cache implementations (like `ccache`).
- **Debugging Complexity:** Splitting creates multiple job logs to sift through. If a PR fails, developers have to check multiple runners instead of a single top-to-bottom execution log.

## 4. Recommendation
**Keep the jobs sequential.**

There is no clear structural or measurable benefit to parallelization. The linting step is extremely fast (~5s) compared to the environment setup (~60-90s) and build (~35s) steps. Splitting would only add runner overhead, duplicate setup costs, increase total CI minutes, and complicate the release dependency graph for a negligible change in the critical path.
