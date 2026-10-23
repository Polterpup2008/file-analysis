# File Analysis 0.8.0 development checkpoint

Compare two is available from Simple and Detailed. With exactly two analyzed files, the button uses that pair automatically; larger batches require two selections with Ctrl+click. The table shows Added, Removed and Changed entries with readable left/right values. Duplicate field occurrences remain grouped, including their separate values, sources, raw/numeric representations, offsets and warnings in details. Hashes, timestamps, recognized parameters/workflows, native structure and provenance remain in the full comparison report.

Native raw blocks are compared at corresponding indices with their type, offset, size and SHA-256. They are positional observations, not inferred moves or a reconstructed timeline. The display is limited to 5000 entries with an explicit notice; complete comparison evidence remains in Export Results. Back to file returns to the file's findings without rescanning.

The toolbar wraps at the supported minimum width instead of hiding actions outside the window. Eight suites pass (328 assertions, including 10 comparison checks for readable values, duplicate/raw evidence, block hashes, empty comparisons, diagnostic exclusion, cancellation and explicit display limits). Interactive comparison and minimum-width validation remain pending.
