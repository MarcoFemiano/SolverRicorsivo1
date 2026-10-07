# Solver CNF ricorsivo

Programma in C che legge una formula in formato DIMACS CNF dallo standard input e cerca un'assegnazione con una funzione ricorsiva.

## Compilazione

Con GCC:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 main.c -o solver.exe
```

## Uso

Avvia `./solver.exe` (in PowerShell: `.\solver.exe`) e scrivi o incolla la formula. Concludi con una riga contenente soltanto `FINE`, eventualmente preceduta o seguita da spazi.

```text
c Esempio
p cnf 3 2
1 -2 0
2 3 0
FINE
```

È possibile anche usare EOF (Ctrl+Z, poi Invio, nel terminale Windows) oppure redirigere un file DIMACS senza aggiungere `FINE`:

```powershell
Get-Content formula.cnf | .\solver.exe
```

L'intestazione `p cnf N M` è obbligatoria: `N` indica le variabili numerate da 1 a `N`, `M` il numero di clausole. Ogni clausola termina con `0`; può occupare più righe e può contenere qualsiasi numero di letterali. Le righe che iniziano con `c` sono commenti. Una clausola vuota (`0`) rende la formula insoddisfacibile.

Il programma stampa `Formula soddisfacibile.` e il valore di ogni variabile, oppure `Formula insoddisfacibile.`. Le variabili non necessarie alla soluzione sono stampate come `falso`. Gli errori di formato sono scritti su stderr e causano un codice d'uscita diverso da zero.

## Ricorsione

La ricerca visita le clausole nell'ordine dato. Se una clausola è già vera, passa alla successiva; se tutti i suoi letterali sono falsi, torna indietro. Altrimenti sceglie una variabile non assegnata, prova il valore che soddisfa un suo letterale e, in caso di fallimento, cambia il valore. Se falliscono entrambi i tentativi, annulla l'assegnazione e ritorna alla decisione precedente. La ricerca è esponenziale nel caso peggiore.
