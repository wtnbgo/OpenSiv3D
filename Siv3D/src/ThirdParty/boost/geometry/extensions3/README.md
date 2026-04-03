# Boost.Geometry extensions3

## Overview

This directory contains a modified version of the Boost.Geometry dissolve extension, adapted for **Boost 1.89+** compatibility.

## Version Support

| Boost Version | Extension Directory |
|---------------|---------------------|
| ≤ 1.74        | extensions/         |
| 1.75 - 1.86   | extensions2/        |
| 1.87 - 1.88   | **Not supported** (compile error) |
| ≥ 1.89        | extensions3/        |

## Key Differences from extensions2

### 1. Rescale Policy Removal

Boost 1.87 removed all deprecated rescaling code. The following were removed:

- `rescale_policy_type` type alias
- `get_rescale_policy()` function
- `RescalePolicy` / `RobustPolicy` template parameters from all functions

**Files affected:**
- `dissolve.hpp`
- `dissolver.hpp`
- `dissolve_traverse.hpp`
- `split_rings.hpp`

### 2. Traversal System Rewrite

Boost 1.89 completely rewrote the traversal system to be graph-based. The following headers no longer exist:

- `backtrack_check_si.hpp` - **Deleted**
- `traversal_ring_creator.hpp` - **Deleted**

**Changes in dissolve_traverse.hpp:**
- Removed `Backtrack` template parameter
- Now delegates to the standard `detail::overlay::traverse` class
- Removed custom `traversal_ring_creator` usage

**Changes in dissolve.hpp:**
- Removed `backtrack_for_dissolve` class (no longer needed)

### 3. Template Signature Changes

#### `traversal_turn_info`

```cpp
// Boost 1.88 and earlier
template <typename Point>
struct traversal_turn_info;

// Boost 1.89+
template <typename Point, typename SegmentRatio>
struct traversal_turn_info;
```

**Fix:** Added explicit `segment_ratio_type` parameter:
```cpp
using coord_type = typename geometry::coordinate_type<point_type>::type;
using segment_ratio_type = geometry::segment_ratio<coord_type>;
using turn_info = detail::overlay::traversal_turn_info<point_type, segment_ratio_type>;
```

#### `enrichment_info`

The `prefer_start` member was removed and replaced with `startable`:

```cpp
// Boost 1.88 and earlier
op.enriched.prefer_start = false;

// Boost 1.89+
op.enriched.startable = false;
```

### 4. Additional Include Required

Added include for `segment_ratio.hpp`:
```cpp
#include <boost/geometry/policies/robustness/segment_ratio.hpp>
```

## References

- [Boost.Geometry 1.87 Release Notes](https://www.boost.org/doc/libs/1_87_0/libs/geometry/doc/html/geometry/release_notes.html) - "Remove deprecated rescaling code"
- [Boost.Geometry GitHub](https://github.com/boostorg/geometry) - "feat: major rewrite of traversal" commit
