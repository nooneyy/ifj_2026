# IFJ 2026

## Závislosti

- make: build systém
- clang: compiler
- clang-format: formátování kódu
- clang-tidy: lintování kódu
- bear: generování `compile_commands.json` pro clang-tidy

## Make příkazy

- `all` / nic: zkompiluje celý program do souboru `ifj26`
- `clean`: smaže zkompilovaný program a zkompilované objektové soubory
- `format`: naformátuje veškerý zdrojový kód v repu
- `tidy`: lintne veškerý zdrojový kód v repu

## Autoformátování a lintování

Stáhni clangd extension do Visual Studio Codu a do user configu si přidej:

```json
"editor.formatOnSave": true,
"[c]": {
    "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd"
},
```

## Pravidla repa

- vše co není dokumentace anglicky
- Proměnné ve `snake_case`, konstanty v `SCREAMING_SNAKE_CASE`, custom structy v `PascalCase`
- názvy commitů jsou podle [Conventional Commits](https://www.conventionalcommits.org)
- každá feature/nová funkcionalita bude mít svůj vlastní branch, z které pak hotové commity půjdou přes PR do main branche
