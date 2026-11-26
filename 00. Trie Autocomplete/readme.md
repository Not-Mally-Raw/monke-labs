# 00. Trie Autocomplete

First lab in `monke-labs`. Taking a basic textbook Trie and turning it into something closer to a real search bar.

## What this mimics

**Google Suggest** (Google Search Autocomplete). You type `"ap"` and get the top-K ranked words back in microseconds, not milliseconds. Tested against `/usr/share/dict/words` (~235k words).

## What this project means

Textbook trees are pointer soup. They leak, thrash cache, and crawl on short prefixes like `"a"`.
The goal here was to take a naive `new`/`delete` Trie and push it toward something production-shaped: flat memory, O(prefix) reads, thread-safe access, and inputs that don't crash it.

## Transition phases

### Phase 1: `trie.cpp` (naive baseline)
- Heap-allocated `TrieNode*` pointers (`new` / `delete`).
- Every `autocomplete()` runs a full DFS + min-heap over the whole subtree.
- Short prefixes like `"a"` walk almost the entire dictionary on every keystroke.

### Phase 2: `trie-v2.cpp` (the 4 upgrades)
1. **Top-K cache per node (`topK`)**: Each node stores its own sorted top 10 words at insert time. Queries just read the vector instead of running DFS.
2. **Arena allocation (`std::vector<TrieNode> arena`)**: Replaced raw pointers with `uint32_t` indices into a single contiguous vector (`NO_NODE` sentinel). No manual `delete`, better cache locality.
3. **Thread safety (`std::shared_mutex`)**: Multiple readers can query at once (`shared_lock`), writes take exclusive access (`unique_lock`).
4. **Input hardening (`charToIdx`, `MAX_LEN`)**: Rejects bad characters, `weight <= 0`, `k <= 0`, and oversized strings upfront before touching the arena.

## Run

Needs C++17 (for `std::shared_mutex`).

```bash
clang++ -std=c++17 -O2 trie-v2.cpp -o trie-v2 && ./trie-v2
```