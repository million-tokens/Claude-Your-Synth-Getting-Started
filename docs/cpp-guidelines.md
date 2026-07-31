- **Standard:** Strictly C++20. No legacy C-style workarounds when C++20 abstractions exist.
- **Formatting:** LLVM style guide. 4 spaces for indentation.
- **Safety:** Absolutely no raw `new`/`delete`. Use `std::unique_ptr` or `std::shared_ptr`.
  Prioritize memory safety, explicit resource management, crash prevention.
- **Modern Paradigms:**
  - Use C++20 **Concepts** instead of `std::enable_if` or SFINAE.
  - Use **`std::span`** for contiguous memory views (avoid raw pointer + size pairs).
  - Prefer **`std::ranges`** and standard views over iterator loops where appropriate.
- **Linter Alignment:** Comply with `bugprone-*`, `cppcoreguidelines-*`, `modernize-*` clang-tidy
  rule sets.
