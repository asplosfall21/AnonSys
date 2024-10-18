from z3 import *

# Define symbolic variables
x, y = Ints('x y')

# Create a solver
s = Solver()

# Add constraints
s.add(x > 0)
s.add(y > 0)
s.add(x + y == 10)
s.add(x * 2 == y)

# Check satisfiability and get model
if s.check() == sat:
    m = s.model()
    print(f"Solution: x = {m[x]}, y = {m[y]}")
else:
    print("No solution")
