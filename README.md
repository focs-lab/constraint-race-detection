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