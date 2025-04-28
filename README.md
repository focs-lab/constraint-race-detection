# Constraint-Based Race Detection

A tool for detecting race conditions in concurrent programs using constraint solving with Z3 theorem prover.

## Overview

This tool analyzes execution traces of concurrent programs to detect potential race conditions. It uses a constraint-based approach with the Z3 theorem prover to identify and verify race conditions.

## Usage

### Building
```bash
cmake -B build
```

```bash
cd build && make
```

### Running
The predictor executable will be in the `build` directory
```bash
./predictor -f <trace_file> [options]
```

Options:
- `-f <trace_file>`: Input trace file (required)
- `--witness-dir <dir>`: Directory for witness files (default: "witness/")
- `--model <type>`: Model type to use (default: "naive").
- `--log-witness`: Generate human-readable witness files
- `--log-binary-witness`: Generate binary witness files
- `--human`: Use human-readable trace format (default: binary)
- `-c <number>`: Maximum number of COP events to check
- `-r <number>`: Maximum number of races to detect

## Trace Format

The tool supports both binary and text trace formats. The text format supports the following events:

Format:
- `Fork <Thread> <Child Thread> 0`
- `Begin <Thread> 0 0` 
- `End <Thread> 0 0`
- `Join <Thread> <Child Thread> 0`
- `Read <Thread> <Var> <Var Value>`
- `Write <Thread> <Var> <Var Value>`
- `Acq <Thread> <Lock> 0`
- `Rel <Thread> <Lock> 0`

Example
```
Fork 1 2 0
Begin 2 0 0
Acq 1 l_0 0
Write 1 X_0 0
Rel 1 l_0 0
Acq 2 l_0 0
Rel 2 l_0 0
Write 2 X_0 1
End 2 0 0
Join 1 2 0
```