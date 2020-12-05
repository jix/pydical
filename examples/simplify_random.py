import random
from pydical import Solver, SATISFIABLE

s = Solver()

N = 30
k = 3

print("c generating random instance")

clauses = []

while s.solve() == SATISFIABLE:
    vars = random.choices(range(1, N + 1), k=k)
    lits = [var * random.choice((-1, 1)) for var in vars]
    s.add_clause(lits)
    clauses.append(lits)

clauses.pop()

print(f"c {len(clauses)} original clauses")

s = Solver()
s.add_clauses(clauses)

s.simplify()

simplified_clauses = []


def handle_clause(clause, witness=None):
    simplified_clauses.append(clause)
    return True


s.traverse_clauses(handle_clause)
s.traverse_witnesses_backward(handle_clause)

print(f"c {len(simplified_clauses)} simplified clauses")

for clause in simplified_clauses:
    print(*clause, 0)
