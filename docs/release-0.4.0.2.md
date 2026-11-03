# MCM2:Reborn 0.4.0.2 — track discovery stability

- Duplicate tracks in local/shared folders no longer remove unrelated tracks from the list.
- Windows filenames are merged without regard to case; the local copy wins a duplicate.
- Directory scans now use one enumeration with a growing buffer, avoiding an overrun if files change between separate counting and copying passes.
- Empty folders, long search paths, interrupted enumeration and allocation failures are handled without publishing a partial list or leaking the temporary lists.
- Merged catalogs are consistently sorted and retain correct local/shared source tags.

These changes improve the shared discovery code used by track selection and dynamic track counts.

## Reference

Compared the reconstructed DirectoryList and CombinedDirectoryList behavior in [andrewboudreau/motocross-madness-2-decomp](https://github.com/andrewboudreau/motocross-madness-2-decomp/blob/7d8fe635b45c64f3d219d146b2fb7081ce06d253/src/reconstructed/DirectoryList.cpp) with Reborn's existing retail ports in code/mcm2/dirlist.c. The upstream project describes itself as ongoing function/class reconstruction, not a complete playable source rebuild. Its addresses are not assumed interchangeable with Reborn's retail bindings. The fixes are implemented in Reborn's existing C ports.

## Validation

1,768 regression and asset checks passed, including 157 directory checks. Directory cases cover actual Windows folders, case-only and exact duplicates, growth to 160 entries, local/shared precedence, enumeration failure and injected allocation failures. Full executable rebuild, fixed retail section verification, declaration and calling-convention checks passed. Game and launcher identities, all modern menu archives and ZIP integrity are verified during packaging.

Physical gameplay testing has not been performed for this patch. Existing unfinished track-editor changes were preserved in their original checkout.
