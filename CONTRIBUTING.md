# Contributing to angelscript-jit

**[English](CONTRIBUTING.md)** | **[Español](docs/CONTRIBUTING.es.md)**

Thank you for helping. Bug reports, fixes, tests and documentation are all welcome.

## Report a problem

Open an [issue](https://github.com/Gaftherman/angelscript-jit/issues) with the AngelScript release, your operating system and architecture, the compiler, and the script that fails. If the same script runs correctly on the interpreter, say so: that is a JIT bug. `tests/differential` shows how to compare both.

A problem in MIR itself, or one that also happens in [asumagic/angelsea](https://github.com/asumagic/angelsea), is best reported there too.

## Propose a change

1. Fork the repository and create a branch from `main`.
2. Keep each change small and focused. A change to the generated code needs a script that fails without it.
3. Write commit messages as `type(scope): short imperative description` without a trailing period. Types: `feat`, `fix`, `test`, `perf`, `refactor`, `style`, `docs`, `chore`.
4. Open a pull request that says what changed and how you checked it. A speed change needs the benchmark numbers before and after.

## Build and test

~~~sh
git clone --recurse-submodules https://github.com/Gaftherman/angelscript-jit.git
cd angelscript-jit
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DASEA_ENABLE_TESTING=ON
cmake --build build
(cd tests && ../build/tests/angelsea-tests "~[benchmark]")
ctest --test-dir build/tests -R differential --output-on-failure
~~~

To test another AngelScript release, point `ASEA_ANGELSCRIPT_ROOT` at an SDK folder (the one that holds `angelscript/` and `add_on/`). [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) prepares the older releases with `scripts/prepare-sdk.py`.

Before sending a change to the code generator, run the generated corpus too:

~~~sh
cd tests
python differential/generate.py --seed 1 --count 120 --out build-gen
../build/tests/angelsea-differential build-gen
~~~

More about the suite is in [docs/TESTS.md](docs/TESTS.md).

## Style

C++20. Follow `.clang-format`. The Python helpers use plain Python 3 with no dependencies.

## License

By contributing you agree that your work is released under the [BSD-2-Clause License](LICENSE).

## Related projects

| Project | Use |
| --- | --- |
| [angelscript-cli](https://github.com/Gaftherman/angelscript-cli) | Run, check and debug scripts from the terminal |
| [angelscript-engine](https://github.com/Gaftherman/angelscript-engine) | Patched SDK builds of older AngelScript releases |
| [angelscript-lsp](https://github.com/Gaftherman/angelscript-lsp) | Editor support for VS Code |
