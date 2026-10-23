# File Analysis 0.7.0 development checkpoint

Batch search works across the retained fields, numeric/raw values, recognized generation settings, errors, preview descriptions/hashes and filenames of all scanned files. Check Search all files, enter a query, and press Find. Category filters narrow the results. Open match or double-click a row navigates to its original file and field without rescanning. The full Export Results report remains complete.

Matching uses Windows invariant Unicode case-insensitive collation. A query is limited to 512 UTF-16 code units; results stop at 5000 rows with a visible limit notice. Long strings are searched in overlapping chunks on the cancelable background worker. Search is a convenience for retained evidence, not a byte-identical comparison or a search of unscanned disk files, image pixels, or parser-only fields beyond the display retention limit. Cancellation preserves partial matches and labels them incomplete.

Choose a category before pressing Find; changing the query or category clears the previous search results. Local row filtering uses displayed summaries; Search all files searches full retained values and works with a one-file batch too.

Large structured values use compact table summaries; selecting the row retains full evidence in the detail pane and exports. Seven suites pass (318 assertions, including 17 batch-search checks). Final interactive search/navigation validation remains pending.
