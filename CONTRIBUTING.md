# Contributing to MiniShell

Thank you for your interest in contributing to MiniShell! MiniShell is designed as an educational, clean, modular Unix shell implemented in C11.

---

## Code of Conduct & Principles

1. **Standard Compliance:** All code must conform to standard C11 and POSIX standards (`-std=c11 -Wall -Wextra -Wpedantic`).
2. **Modularity:** Keep responsibilities separated across headers and source files (`src/parser.*`, `src/executor.*`, `src/builtins.*`, etc.). Do not bloat individual modules.
3. **Safety & Zero Warnings:** Code must compile with zero warnings under `-Wall -Wextra -Wpedantic`. Never suppress compiler diagnostics or ignore dynamic memory leaks.
4. **Automated Verification:** Every new feature, grammar expansion, or bug fix must include corresponding tests in `tests/`.

---

## Development Workflow

### 1. Clone & Build
```bash
git clone https://github.com/<your-username>/minishell.git
cd minishell
make
```

### 2. Run the Test Suite
Before committing any changes, run the automated verification suite:
```bash
bash tests/run_tests.sh
```

### 3. Check for Memory Errors (Linux / ASan)
```bash
make sanitize
bash tests/run_tests.sh
```

---

## Submitting Pull Requests

1. Fork the repository and create your feature branch from `main`:
   ```bash
   git checkout -b feature/my-new-feature
   ```
2. Commit your changes with clear, descriptive commit messages:
   ```bash
   git commit -m "feat(parser): add support for operator sequence XYZ"
   ```
3. Push to your fork:
   ```bash
   git push origin feature/my-new-feature
   ```
4. Open a Pull Request on GitHub detailing what changes were made and how they were tested.
