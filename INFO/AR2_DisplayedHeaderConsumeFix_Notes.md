# AR2 Displayed Header Consume Fix

Fixes an AR2v2 input edge case where a visible encrypted key header such as:

```text
0E3C7DF2 1853E59E
```

could be treated as a normal code row when the same input AR2 key was already selected in the UI.

## Correct behavior

For AR2v2 encrypted input, a leading visible key header is now consumed as metadata before decrypting the remaining block:

- `0E3C7DF2 XXXXXXXX` selects the AR2v2 input seed for that block and is removed from the cheat body.
- `0E3C7DF2 1456E7A5` remains the RAW-enable / no-decryption marker.
- The header is not translated or written as a normal output code line.

This keeps AR2 -> AR2 conversions from producing an extra converted/header-derived row before the real master hook.

## Regression covered

Input:

```text
!Master Codes:
(M) Must Be On by Codejunkies
0E3C7DF2 1853E59E
EE8CDC1A BCBBBD2A
!!
```

Output AR2 with `1456E7A5` selected now produces only:

```text
0E3C7DF2 1456E7A5
F01222A4 001222A7
```

instead of carrying an extra header-derived code row.
