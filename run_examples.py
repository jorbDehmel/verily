'''
Assuming that 'verily' and 'lualatex' are commands that exist,
runs all the examples and compiles the latex.
'''

from subprocess import run
from os import listdir


to_latex = []
for fp in listdir('examples'):
  if fp.endswith('.verily'):
    fp = f'examples/{fp}'
    run(['verily', fp, '--latex'])
    print(fp)

    to_latex.append(fp)

for fp in to_latex:
  run(['lualatex', fp])

run([
  'find', '.', '-type', 'f', '(', '-iname', '*.aux', '-or',
  '-iname', '*.log', '-or', '-iname', '*.tex', ')',
  '-exec', 'rm', '{}', ';'
])
