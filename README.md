# NITCbase

A relational database engine I built from scratch in C++.

Supports — creating tables, inserting records, querying with conditions, and indexing via B+ trees.
completed all 12 stages
Eventhough it is a course project for NITC CSE,I did this as a personal project to understand how relational databases work by building one from scratch.
## Features

- Create / drop tables and insert records
- Linear search and SELECT with conditions
- Then added better search and SELECT using B+tree indexing
- PROJECT to pick specific columns
- Equi-join across two relations

## Stack

- C++
- Custom block-based disk (XFS)
- B+ Trees for indexing

## Run it

```bash
make
./nitcbase
```

## Limitations

No tuple deletion, no concurrency, equi-join only.
