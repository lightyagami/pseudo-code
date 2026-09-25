# Cambridge 9618 Pseudocode Extension for VS Code

Official syntax highlighting and language configuration for **Cambridge International AS & A Level Computer Science (9618 & 9608) Pseudocode**.

---

## Features

- **Full Syntax Highlighting**:
  - Declarations: `DECLARE`, `CONSTANT`, `TYPE`, `CLASS`, `INHERITS`, `EXTENDS`
  - Subroutines: `PROCEDURE`, `FUNCTION`, `RETURNS`, `CALL`, `BYVAL`, `BYREF`
  - Control Flow: `IF ... THEN ... ELSE ... ENDIF`, `FOR ... TO ... NEXT`, `WHILE ... ENDWHILE`, `REPEAT ... UNTIL`, `CASE OF ... OTHERWISE ... ENDCASE`
  - Operators: `<-`, `=`, `<>`, `<=`, `>=`, `DIV`, `MOD`, `AND`, `OR`, `NOT`, `&`
  - Types: `INTEGER`, `REAL`, `BOOLEAN`, `STRING`, `CHAR`, `DATE`, `ARRAY[1:N] OF T`
  - I/O & File Operations: `OUTPUT`, `INPUT`, `OPENFILE`, `CLOSEFILE`, `READFILE`, `WRITEFILE`, `SEEK`, `GETRECORD`, `PUTRECORD`
  - Cambridge Builtins: `LENGTH`, `SUBSTRING`, `MID`, `LEFT`, `RIGHT`, `UCASE`, `LCASE`, `NUM_TO_STR`, `STR_TO_NUM`, `CHR`, `ASC`, `INT`, `ROUND`, `RND`, `EOF`
- **Smart Indentation**:
  - Automatically indents on block keywords (`IF`, `FOR`, `WHILE`, `PROCEDURE`, etc.) and outdents on closing tags (`ENDIF`, `NEXT`, `ENDWHILE`, etc.).
- **Auto-Closing Brackets & Quotes**:
  - Pairs `[]`, `()`, `""`, and `''`.
- **Comment Support**:
  - Line comments with `//` and toggle comment shortcut (`Ctrl+/` or `Cmd+/`).

---

## Installation

### Local Installation
Copy this folder into your local VS Code extensions directory:

```bash
# On Linux / macOS
cp -r vscode-extension ~/.vscode/extensions/pseudocode-cambridge

# On Windows PowerShell
cp -Recurse vscode-extension "$HOME/.vscode/extensions/pseudocode-cambridge"
```

Restart VS Code or run `Developer: Reload Window` (`Ctrl+Shift+P` -> `Reload Window`), and all `.pseudo`, `.psc`, and `.pc` files will be automatically highlighted!
