# Systems Programming

- [BSUIR LMS](https://lms.bsuir.by/course/view.php?id=9282)
- Term 5

## How to run

1. Ensure that you have nix package manager
2. Just exec, even not clone repo:
   ```bash
   nix run github:nadevko/bsuir-SP-table#bsuir-sp
   ```

## How to build

```bash
cmake -B build
cd ./build
make
./bsuir-sp
```

## Tasks

1. [Установить linux](docs/task-1.md)
2. [Таблица m на n](docs/task-2.md)
