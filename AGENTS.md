Benchmarks should be placed in the `/bench` directory.
Never downgrade go, kde, or qt.
We are KF6 and Qt 6 only, do not support Qt 5.
To compile and test with Qt 6 locally, you can find a Dockerfile in `.jules/Dockerfile.qt6`.
Always run lint, format, and test before submitting.
If the required changes fall outside the scope of the current task and involve pre-existing failures, the agent may choose not to incorporate them based on the magnitude or consequences of the necessary fixes.
For CLion compatible builds, use: `/home/arran/.local/share/JetBrains/Toolbox/apps/clion/bin/cmake/linux/x64/bin/cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_MAKE_PROGRAM=/home/arran/.local/share/JetBrains/Toolbox/apps/clion/bin/ninja/linux/x64/ninja -G Ninja -S /home/arran/Documents/Projects/kjules -B /home/arran/Documents/Projects/kjules/cmake-build-debug`

