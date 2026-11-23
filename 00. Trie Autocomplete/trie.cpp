#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class Trie {
private:
  struct TrieNode {
    TrieNode *children[26] = {nullptr};
    int weight = 0;
  };
  using MinHeap = priority_queue<pair<int, string>, vector<pair<int, string>>,
                                 greater<pair<int, string>>>;
  TrieNode *root;

  void freeNodes(TrieNode *node) {
    if (node == nullptr)
      return;
    for (int i = 0; i < 26; i++) {
      freeNodes(node->children[i]);
    }
    delete node;
  }
  void dfs(TrieNode *node, string &currWord, MinHeap &minHeap, int k) {
    if (node == nullptr) {
      return;
    }
    if (node->weight > 0) {
      minHeap.push({node->weight, currWord});
      if (static_cast<int>(minHeap.size()) > k) {
        minHeap.pop();
      }
    }
    for (int i = 0; i < 26; ++i) {
      if (node->children[i] != nullptr) {
        currWord.push_back(static_cast<char>('a' + i));
        dfs(node->children[i], currWord, minHeap, k);
        currWord.pop_back();
      }
    }
  }

public:
  Trie() { root = new TrieNode(); }

  ~Trie() { freeNodes(root); }

  void insert(string word, int weight) {
    TrieNode *curr = root;
    for (char ch : word) {
      int idx = ch - 'a';
      if (curr->children[idx] == nullptr) {
        curr->children[idx] = new TrieNode();
      }
      curr = curr->children[idx];
    }
    curr->weight = weight;
  }

  bool search(string word) {
    TrieNode *curr = root;
    for (char ch : word) {
      int idx = ch - 'a';
      if (curr->children[idx] == nullptr) {
        return false;
      }
      curr = curr->children[idx];
    }
    return curr->weight > 0;
  }

  bool startsWith(string prefix) {
    TrieNode *curr = root;
    for (char ch : prefix) {
      int idx = ch - 'a';
      if (curr->children[idx] == nullptr) {
        return false;
      }
      curr = curr->children[idx];
    }
    return true;
  }
  vector<string> autocomplete(const string &prefix, int k) {
    if (k <= 0)
      return {};
    TrieNode *curr = root;
    for (char ch : prefix) {
      int idx = ch - 'a';
      if (idx < 0 || idx >= 26 || curr->children[idx] == nullptr) {
        return {};
      }
      curr = curr->children[idx];
    }
    MinHeap minHeap;
    string currWord = prefix;
    dfs(curr, currWord, minHeap, k);
    vector<string> results;
    while (!minHeap.empty()) {
      results.push_back(minHeap.top().second);
      minHeap.pop();
    }
    reverse(results.begin(), results.end());
    return results;
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
    if (!clean.empty()) {
      trie.insert(clean, max(1, weight--));
      ++count;
    }
  }
  cout << "Loaded " << count << " entries\n";
}

void benchQuery(Trie &trie, const string &prefix, int k) {
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