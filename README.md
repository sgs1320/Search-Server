# Search Server

A high-performance document search system that ranks documents by relevance using the TF-IDF algorithm. It supports exclusion of documents containing specific "minus-words" and provides robust filtering and ranking capabilities.

---

## Features

- **TF-IDF Ranking**: Ranks documents by relevance based on Term Frequency-Inverse Document Frequency.
- **Minus-Words Support**: Excludes documents containing specific words from search results.
- **Thread-Safe Execution**: Utilizes Intel TBB for efficient multi-threaded performance.
- **Flexible Query Handling**: Handles multiple queries and efficiently filters relevant results.

---

## Example

### Document Database:
```   
 Doc id_1:   white cat and yellow hat
 Doc id_2:   curly cat curly tail
 Doc id_3:   nasty dog with big eyes
 Doc id_4:   nasty pigeon john
```

### Query:
```
and with -john
```

### Output:
```
ACTUAL:
{ document_id = 2, relevance = 0.866434, rating = 1 }
{ document_id = 1, relevance = 0.173287, rating = 1 }
{ document_id = 3, relevance = 0.173287, rating = 1 }
BANNED:
{ document_id = 4, relevance = 0.231049, rating = 1 }
```


## Deployment Instructions
* Version Standard С++17
* Thread Building Blocks (Intel TBB)
