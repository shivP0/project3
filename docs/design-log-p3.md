# Project 3 Design Log

**Author:** Shiv Patel

**P2 code used:** My own Project 2 `Conversation` and `SentinelScanner`.

**Tools used:** Codex helped set up the repository, write the registry, workspace checks, calculator, and tests, debug the build, and draft this log. The project was built and checked using CMake, Apple Clang, AddressSanitizer, and UndefinedBehaviorSanitizer. Git was used to save versions of the code.

## 1. Hash table

The registry uses separate chaining. It has a vector of buckets, and each bucket holds a vector of `unique_ptr<Tool>`. If two names map to the same bucket, both tools go into that bucket. To find a tool, `find()` calculates the bucket index and checks the names stored there. This makes collisions fairly easy to handle.

The hash uses the XOR and multiply loop from Appendix A, with the starting value given there. It processes every character in order. The hash function returns the whole number; the remainder operation selects a bucket afterward.

The registry starts with 16 buckets. Its maximum load factor is 1.0, so there is at most one tool per bucket on average, although individual buckets can hold more. When the table is full by that measure, the next insertion doubles the bucket count and moves the pointers into their new buckets. A null pointer or duplicate name is rejected first. For a duplicate, the original tool stays in place and the new one is destroyed.

These were the results for `tool_0` through `tool_9999`:

| Measurement | Registry hash | Weak hash: first byte + name length |
|---|---:|---:|
| Bucket count | 16,384 | 16,384 |
| Load factor | 0.610352 | 0.610352 |
| Longest chain | 6 | 9,000 |

The weak hash comparison counts how many names would land in each bucket. Every name starts with `t`, so the hash mainly groups names by length. That explains the chain of 9,000 tools. Having plenty of buckets does not help if the hash sends most names to the same one.

Resizing takes O(n) work, but doubling makes space for many more insertions. That cost is spread over the insertions between resizes. With a good distribution and bounded name lengths, `add()` is expected amortized O(1). Reading a name to hash it still takes time proportional to its length.

## 2. Pointer stability

Moving a `unique_ptr` changes which pointer owns a tool, but the tool itself stays at the same memory address. This is why a pointer from `find()` still works after the buckets resize. The scale test saves the first tool's pointer and checks it again after all 10,000 insertions. The registry owns the tools, so callers must not delete the pointers they receive.

## 3. Calculator trace

The calculator uses one stack for numbers and one for operators. For `2 * (3 + 4) - 5`, the stack tops are shown on the right:

1. `2`: push the number. Values `[2]`, operators `[]`.
2. `*`: push the operator. Values `[2]`, operators `[*]`.
3. `(`: push it. Values `[2]`, operators `[*, (]`.
4. `3`: push the number. Values `[2, 3]`, operators `[*, (]`.
5. `+`: push it above `(`. Values `[2, 3]`, operators `[*, (, +]`.
6. `4`: push the number. Values `[2, 3, 4]`, operators `[*, (, +]`.
7. `)`: calculate `3 + 4` and remove `(`. Values `[2, 7]`, operators `[*]`.
8. `-`: multiplication comes first, so calculate `2 * 7`, then push `-`. Values `[14]`, operators `[-]`.
9. `5`: push the number. Values `[14, 5]`, operators `[-]`.
10. End: calculate `14 - 5`. Values `[9]`, operators `[]`.

The right operand is popped first. Operators with equal precedence are handled left to right, which matters for subtraction and division. The evaluator uses loops rather than recursion. Tests cover 20,000 terms and 5,000 nested parentheses.

## 4. Errors as values

A model can ask for a missing file, an unsafe path, or division by zero. Those are normal failures the model should be able to read and respond to. Returning `false` and an error message lets the tool wrapper send back that information. Exceptions are better suited to mistakes in the program, such as trying to read a JSON number as a string. The dispatcher also catches exceptions as a final backup.

## 5. What I would design differently

One improvement would be moving the resizing loop into a private `rehash()` helper. Then `add()` would be shorter, and the code that moves tools between buckets would be easier to check separately.

On macOS, all 35 P3 tests, 14 P2 tests, nine tool-loop tests, and the end-to-end group passed with warnings treated as errors and ASan/UBSan enabled. The final Linux run, including leak detection, is still pending.
