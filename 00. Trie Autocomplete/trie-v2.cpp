#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class Trie {
private:
  static constexpr int CACHE_K = 10;
  static constexpr size_t MAX_LEN = 128;
  static constexpr uint32_t NO_NODE = UINT32_MAX;
  struct TrieNode {
    uint32_t children[26];
    int weight = 0;
    vector<pair<int, string>> topK;
    TrieNode() {
      for (int i = 0; i < 26; ++i) {
        children[i] = NO_NODE;
      }
    }
  };
  vector<TrieNode> arena;
  uint32_t root = 0;
  mutable shared_mutex mtx_;

  uint32_t createNode() {
    arena.emplace_back();
    return static_cast<uint32_t>(arena.size() - 1);
  }

  static int charToIdx(char ch) {
    unsigned char uch = static_cast<unsigned char>(ch);
    if (uch < 'a' || uch > 'z')
      return -1;
    return uch - 'a';
  }

  static bool betterEntry(const pair<int, string> &a,
                          const pair<int, string> &b) {
    if (a.first != b.first)
      return a.first > b.first;
    return a.second < b.second;
  }

  void updateTopK(uint32_t nodeIdx, const string &word, int weight) {
    if (nodeIdx >= arena.size())
      return;
    auto &top = arena[nodeIdx].topK;
    pair<int, string> candidate{weight, word};
    if (static_cast<int>(top.size()) == CACHE_K &&
        !betterEntry(candidate, top.back())) {
      return;
    }
    for (auto &entry : top) {
      if (entry.second == word) {
        if (weight > entry.first) {
          entry.first = weight;
          sort(top.begin(), top.end(), betterEntry);
        }
        return;
      }
    }
    top.push_back(std::move(candidate));
    sort(top.begin(), top.end(), betterEntry);
    if (static_cast<int>(top.size()) > CACHE_K) {
      top.pop_back();
    }
  }

public:
  Trie() {
    arena.reserve(500000);
    root = createNode();
  }

  bool search(const string &word) const {
    if (word.size() > MAX_LEN)
      return false;

    shared_lock<shared_mutex> lock(mtx_);
    uint32_t curr = root;
    for (char ch : word) {
      int idx = charToIdx(ch);
      if (idx < 0 || arena[curr].children[idx] == NO_NODE) {
        return false;
      }
      curr = arena[curr].children[idx];
    }
    return arena[curr].weight > 0;
  }

  bool startsWith(const string &prefix) const {
    if (prefix.size() > MAX_LEN)
      return false;

    shared_lock<shared_mutex> lock(mtx_);
    uint32_t curr = root;
    for (char ch : prefix) {
      int idx = charToIdx(ch);
      if (idx < 0 || arena[curr].children[idx] == NO_NODE) {
        return false;
      }
      curr = arena[curr].children[idx];
    }
    return true;
  }

  bool insert(const string &word, int weight) {
    if (weight <= 0 || word.empty() || word.size() > MAX_LEN)
      return false;

    for (char ch : word) {
      if (charToIdx(ch) < 0)
        return false;
    }

    unique_lock<shared_mutex> lock(mtx_);
    uint32_t curr = root;
    updateTopK(curr, word, weight);

    for (char ch : word) {
      int idx = charToIdx(ch);
      if (arena[curr].children[idx] == NO_NODE) {
        uint32_t nextIdx = createNode();
        arena[curr].children[idx] = nextIdx;
      }
      curr = arena[curr].children[idx];
      updateTopK(curr, word, weight);
    }
    arena[curr].weight = max(arena[curr].weight, weight);
    return true;
  }

  vector<string> autocomplete(const string &prefix, int k) const {
    if (k <= 0 || prefix.size() > MAX_LEN)
      return {};

    shared_lock<shared_mutex> lock(mtx_);
    uint32_t curr = root;
    for (char ch : prefix) {
      int idx = charToIdx(ch);
      if (idx < 0 || arena[curr].children[idx] == NO_NODE) {
        return {};
      }
      curr = arena[curr].children[idx];
    }

    const auto &top = arena[curr].topK;
    int limit = min({k, CACHE_K, static_cast<int>(top.size())});
    vector<string> results;
    results.reserve(limit);
    for (int i = 0; i < limit; ++i) {
      results.push_back(top[i].second);
    }
    return results;
  }

  size_t nodeCount() const {
    shared_lock<shared_mutex> lock(mtx_);
    return arena.size();
  }
};

void loadDict(Trie &trie, const string &path) {
  ifstream file(path);
  if (!file.is_open()) {
    cerr << "Could not open " << path << "\n";
    return;
  }
  string word;
  int weight = 1000000;
  size_t count = 0;
  while (file >> word) {
    string clean;
    for (char ch : word) {
      if (ch >= 'A' && ch <= 'Z')
        clean += static_cast<char>(ch - 'A' + 'a');
      else if (ch >= 'a' && ch <= 'z')
        clean += ch;
    }
    if (!clean.empty() && trie.insert(clean, weight)) {
      if (weight > 1)
        --weight;
      ++count;
    }
  }
  cout << "Loaded " << count << " entries (" << trie.nodeCount()
       << " arena nodes)\n";
}

void benchQuery(const Trie &trie, const string &prefix, int k) {
  auto start = chrono::steady_clock::now();
  auto res = trie.autocomplete(prefix, k);
  auto end = chrono::steady_clock::now();
  auto us = chrono::duration_cast<chrono::microseconds>(end - start).count();
  cout << "prefix=\"" << prefix << "\" -> " << res.size() << " results in "
       << us << " us\n";
}

int main() {
  Trie trie;
  auto t0 = chrono::steady_clock::now();
  loadDict(trie, "/usr/share/dict/words");
  auto t1 = chrono::steady_clock::now();
  cout << "Load time: "
       << chrono::duration_cast<chrono::milliseconds>(t1 - t0).count()
       << " ms\n";

  benchQuery(trie, "a", 5);
  benchQuery(trie, "ap", 5);
  benchQuery(trie, "app", 5);
  benchQuery(trie, "comp", 5);
  return 0;
}