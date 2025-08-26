# Parallel Graph Traversal

În acest proiect am implementat un **Thread Pool** generic în C
și l-am folosit pentru a parcurge un graf în paralel.

Structura este inspirată de listele dublu-înlanțuite din **kernel-ul Linux** 
și de clasica problemă **producător-consumator**.

Implementarea este modulară:
- `os_list` => listă dublu-înlanțuită generică
- `os_threadpool` => infrastructura de paralelizare
- `os_graph` => structura și inițializarea grafului
- `parallel` => algoritmul de parcurgere paralelă

## Thread Pool

Acest **design pattern** presupune implementarea a două componente principale:
- Un pool de task-uri de executat, reprezentat de o `coadă`
- Un grup de **workeri** (**thread**-uri)

```c
typedef struct os_threadpool {
	unsigned int num_threads;
	pthread_t *threads;

	/* Head of queue used to store tasks. */
	os_list_node_t head;

	/* Threadpool / queue synchronization data. */
	pthread_mutex_t queue_mtx;
	pthread_cond_t has_tasks;
	int stop_flag;            // 0 -> is running, 1 -> shutdown
} os_threadpool_t;
```

În implementarea Thread Pool-ului meu, am folosit următoarele primitive de sincronizare:
- Un **mutex**:
    care protejează accesurile paralele asupra cozii
- O **variabilă condițională** (`pthread_cond_t`):
    utilizată pentru a bloca thread-urile atunci când coada este goală
    și pentru a le trezi atunci când se inserează noi taskuri.
    Practic, `enqueue_task` face `pthread_cond_signal` (trezește un worker),
    iar `wait_for_completion` face `pthread_cond_broadcast` (trezește toate thread-urile pentru **shutdown**).
- Un **flag** (`stop`):
    care marchează momentul în care Thread Pool-ul trebuie să se oprească (mecanism de **graceful shutdown**).
    Când `stop=1`, workerii nu mai așteaptă taskuri noi și ies din bucla principală după ce coada se golește.


Pașii algoritmului:
  1. `enqueue_task` => inserează un task în coadă și notifică un worker
  2. Worker-ul rulează `dequeue_task` => așteaptă dacă nu sunt taskuri
  3. După ce primește un task, execută funcția (`action`), apoi îl eliberează (`destroy_task`)
  4. `wait_for_completion` => setează `stop = 1`, notifică toți workerii și face `join`

## Graf

- Graf neorientat stocat ca **vector de noduri**, fiecare nod având:
  - `info`: valoarea asociată nodului
  - `neighbours`: vector cu indecșii vecinilor
- `visited`: vector de stări (`NOT_VISITED`, `PROCESSING`, `DONE`)

## Parcurgerea paralelă

- Se pornește de la nodul cu indexul `0`
- Pentru fiecare nod:
  1. Se marchează `PROCESSING` și se adaugă valoarea nodului la **sumă globală**
  2. Se creează taskuri pentru vecinii **nevizitați**
  3. La final, nodul devine `DONE`
- Accesul la `visited` și `sum` este protejat cu un **mutex global**



## 🛡️ Sincronizare și memory management

**Thread safety**: accesul la `graph->visited` și `sum` este protejat cu `pthread_mutex_t graph_lock`

**Memory management**:
- Argumentele taskurilor (`graph_task_arg_t`) sunt **alocate dinamic pe heap**
- Eliberarea se face exclusiv prin `destroy_task` => evită `double free`


## 🔎 Observații

La implementarea functiei `dequeue_task`, de ce nu este o idee bună
sa pun lock pe coadă și să returnez direct `NULL` în cazul în care este vidă?

**Răspuns**:
- Dacă returnez NULL doar pentru că lista e momentan goală, thread-ul iese sau stă în idle loop
- Dacă thread-ul iese => threadpool-ul moare imediat ce coada devine goală (dar poate că în câteva milisecunde vine un nou enqueue_task)
- Dacă thread-ul reapeleza `dequeue_task()` într-un loop => intri în busy waiting (thread-urile consumă inutil CPU)
- Problema `Producător–Consumator` spune că
  - Consumatorii (workerii) trebuie să aștepte dacă nu e nimic de consumat.
  - Dar așteptarea nu trebuie să fie un loop activ, ci un pthread_cond_wait sau sem_wait, adică blocantă
