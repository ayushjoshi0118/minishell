# MiniShell Parser & Grammar Specification

## 1. Formal Grammar (EBNF)

The MiniShell command line syntax adheres to the following Extended Backus-Naur Form (EBNF) definition:

```ebnf
CommandLine     ::= [ Pipeline ] [ Whitespace ] [ '\n' | EOF ]

Pipeline        ::= Command { [ Whitespace ] '|' [ Whitespace ] Command } [ [ Whitespace ] '&' ]

Command         ::= ( SimpleWord | Redirection ) { [ Whitespace ] ( SimpleWord | Redirection ) }

Redirection     ::= RedirOp [ Whitespace ] Filename

RedirOp         ::= '<'       (* Input redirection *)
                  | '>'       (* Output redirection, truncate *)
                  | '>>'      (* Output redirection, append *)
                  | '2>'      (* Standard error redirection *)

Filename        ::= Word

SimpleWord      ::= ( UnquotedChar | SingleQuoted | DoubleQuoted | EscapedChar )+

SingleQuoted    ::= "'" { AnyCharExceptSingleQuote } "'"

DoubleQuoted    ::= '"' { ExpChar | EscapedInDoubleQuote | AnyCharExceptDoubleQuote } '"'

EscapedChar     ::= '\' AnyChar

EscapedInDoubleQuote ::= '\' ( '"' | '\' | '$' | '`' )

ExpChar         ::= '$' ( VarName | '?' | '$' | '{' VarName '}' )

VarName         ::= ( Letter | '_' ) { Letter | Digit | '_' }

Whitespace      ::= ( ' ' | '\t' | '\r' )+
```

---

## 2. Lexical & Parsing Rules

### 2.1. Quoting Behavior
- **Single Quotes (`'...'`)**:
  - All enclosed characters are treated as literal text.
  - No escape sequences are processed (e.g. `\'` is not an escaped quote; it terminates or remains literal).
  - Variable expansion is strictly disabled.
  - Missing closing single quote generates a syntax error:
    `minishell: syntax error: unmatched single quote '\''`

- **Double Quotes (`"..."`)**:
  - Enclosed spaces and tabs are preserved as part of the argument.
  - Backslash escaping is enabled for specific characters: `\"`, `\\`, `\$`, and `` \` ``.
  - Parameter expansion (`$VAR`, `${VAR}`, `$?`, `$$`) is evaluated.
  - Missing closing double quote generates a syntax error:
    `minishell: syntax error: unmatched double quote '"'`

- **Unquoted Backslash (`\<char>`)**:
  - Treats the next character literally, including whitespace (`\ `), pipe (`\|`), or redirection symbols (`\<`, `\>`).

### 2.2. Parameter & Variable Expansion
- `$NAME`: Reads from the environment via `getenv()`. If unset, expands to empty string `""`.
- `${NAME}`: Explicitly bounded variable name expansion.
- `$?`: Expands to the exit status of the previous foreground pipeline or command.
- `$$`: Expands to the process ID of the MiniShell process.
- Lone `$`: Followed by whitespace or delimiter is treated as a literal `$` character.

### 2.3. Redirection Operators
- `< filename`: Sets standard input to read from the target file.
- `> filename`: Opens or creates the target file in write-only mode, truncating existing content.
- `>> filename`: Opens or creates the target file in append mode.
- `2> filename`: Redirects file descriptor 2 (standard error) to the target file.
- **Syntax Validation:**
  - A redirection operator must be followed by a valid filename token.
  - A redirection operator followed by a pipe, background operator, newline, or EOF triggers:
    `minishell: syntax error near unexpected token '<token>'` with exit code `2`.

### 2.4. Pipeline & Operator Sequences
- A pipeline consists of one or more commands separated by `|`.
- A leading pipe (`| cmd`) is rejected.
- A trailing pipe without a following command (`cmd |`) is rejected.
- Consecutive pipes (`cmd1 || cmd2` or `cmd1 | | cmd2`) are rejected.
- Background execution is requested when `&` is placed at the end of the pipeline.
