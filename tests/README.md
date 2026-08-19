# Library tests

The `rasta/` and `sci/` directories contain the existing CUnit tests for the
transport and SCI libraries. They are separate from the optional manual and
stress programs under `extra/diagnostics/`.

Most historical CUnit tests still belong to the legacy Gradle workflow. The
standalone regression tests registered in `CMakeLists.txt` can be run with
`ctest --test-dir build --output-on-failure`.
