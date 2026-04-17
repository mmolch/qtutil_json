# **mmolch_qtutil_json**
### *Little helpers to load, merge, diff, and validate JSON objects with Qt*

This library provides a small collection of qtutilities for working with JSON in Qt applications.
It focuses on:

- **Exception‑free error handling** using `std::expected`
- **Deep and schema‑aware JSON merging**
- **JSON diffing**
- **JSON Schema–like validation** (Draft‑7 subset)
- **Qt‑native data structures** (`QJsonObject`, `QJsonArray`, …)

It is designed to be lightweight, dependency‑free (besides Qt), and easy to integrate into existing Qt6 projects.

## Features

### **JSON loading with detailed error reporting**
- Detects missing files, open failures, parse errors
- Reports line/column for parse errors
- Ensures top‑level JSON is an object
- Returns `std::expected<QJsonObject, JsonError>`

### **JSON merging**
- Shallow or deep merge (`JsonMergeOption::Recursive`)
- Optional null‑override semantics (`JsonMergeOption::OverrideNull`)
- Schema‑aware merge strategies:
  - `"mergeStrategy": "replace"`
  - `"mergeStrategy": "deep"`
  - `"mergeStrategy": "appendUnique"`

### **JSON diff**
- Computes differences between two objects
- Optional recursive diffing
- Optional explicit nulls for removed keys

### **JSON Schema–like validation**
Supports a practical subset of JSON Schema Draft‑7:

- `type`, including `"integer"`
- `enum`, `const`
- Numeric constraints (`minimum`, `maximum`, `exclusiveMinimum`, …)
- String constraints (`minLength`, `maxLength`, `pattern`)
- Array constraints (`items`, tuple validation, `uniqueItems`, …)
- Object constraints (`properties`, `required`, `additionalProperties`)
- Logical combiners: `allOf`, `anyOf`, `oneOf`, `not`
- Local `$ref` resolution using JSON Pointers

## Requirements

- **Qt ≥ 6.4**
  Required for `qt_standard_project_setup()` and stable C++23 integration.

- **C++23 compiler**
  Required for `std::expected` and `std::format`.

Tested with:

- GCC ≥ 13
- Clang ≥ 16

## Building

```bash
cmake -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

## License

MIT License — free to use in commercial and open‑source projects.

## Contributing

Issues and pull requests are welcome.
