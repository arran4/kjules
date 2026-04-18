Benchmarks should be placed in the `/bench` directory.
Never downgrade go, kde, or qt.
We are KF6 and Qt 6 only, do not support Qt 5.
Always run lint, format, and test before submitting.
If the required changes fall out of the scope of the change and they were pre-existing failures the agent can make a decision based on the size / consequences of the test and lint changes and decide not to incorporate them.