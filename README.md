# CPU Task Scheduler

A simulator that runs a set of processes on a single CPU under **Round Robin** and **Priority** scheduling. It shows the order of execution as a Gantt chart, computes completion, turnaround, waiting and response times for every process, and reports the averages.

The scheduling logic is written in C++ and exposed through a small HTTP API. A plain HTML, CSS and JavaScript page is the user interface.

## Features

- **Round Robin** with a configurable time quantum.
- **Priority scheduling**, both non-preemptive and preemptive. A higher priority number means more urgent. Ties are broken by earlier arrival, then lower PID.
- **Gantt chart** with time markers and a distinct style for CPU idle gaps.
- **Metrics cards** for average waiting time, average turnaround time, average response time and CPU utilization, plus a per-process results table.
- **Playback**: replay a run with a moving clock, a live ready queue and per-process remaining-time bars.
- **Live additions**: add a process while a simulation is playing. The new process joins the schedule at the current clock, and everything already played stays unchanged.
- **Algorithm comparison**: Round Robin against Priority on the same input, with bar charts and a ranking.
- Light and dark theme.

## Data structures

All three are implemented by hand in `src/`:

| Structure | File | Role |
|---|---|---|
| Linked list | `process.h`, `process.cpp` | The master process table. It owns every process and stores both the input and the results. |
| Circular queue | `circular_queue.h` | The Round Robin ready queue. Front and rear indices wrap around with modulo arithmetic. |
| Binary heap | `heap.h` | The priority scheduler's ready queue, with the comparison rule in one function. |

The queue and the heap hold pointers into the linked list, never copies, so a process's remaining time lives in exactly one place.
```

## Run it locally

You need a C++17 compiler (`g++`). From the project folder:

**Windows (PowerShell)**

```
g++ -std=c++17 -Wall -Wextra -I third_party -o server src/server.cpp src/api.cpp src/serializer.cpp src/json_mini.cpp src/process.cpp src/stats.cpp src/scheduler.cpp -lws2_32
.\server.exe
```

**Linux or macOS**

```
make server
./server
```

Then open `http://localhost:8080`. Press Ctrl+C to stop the server. To use another port, pass it as an argument, for example `.\server.exe 9000`.

## Run the tests

**Windows (PowerShell)**

```
g++ -std=c++17 -Wall -Wextra -I third_party -o tests src/tests.cpp src/process.cpp src/stats.cpp src/scheduler.cpp
.\tests.exe

g++ -std=c++17 -Wall -Wextra -o api_tests src/api_tests.cpp src/api.cpp src/serializer.cpp src/json_mini.cpp src/process.cpp src/stats.cpp src/scheduler.cpp
.\api_tests.exe
```

**Linux or macOS**

```
make check
```

The tests check every scheduler against hand-worked examples, including idle gaps, ties, preemption and the queue and heap behaviour. They also check that adding a process at a later clock never changes the part of the schedule that has already happened.

## API

The server is stateless: each simulation is a pure function of its input.

`POST /run` takes JSON such as:

```json
{
  "algorithm": "rr",
  "quantum": 2,
  "processes": [
    { "pid": 1, "arrival": 0, "burst": 5, "priority": 1 },
    { "pid": 2, "arrival": 1, "burst": 3, "priority": 2 }
  ]
}
```

Use `"algorithm": "priority"` with an optional `"preemptive": true` for priority scheduling. The response contains the Gantt entries (with the ready queue at each slice), one row per process (completion, turnaround, waiting and response times), the averages, the total time and the idle time. Invalid input returns HTTP 400 with an `error` message.

`GET /health` returns `ok`.

## Deployment

The server and the page can be hosted separately.

- **Server (Render):** create a Web Service from this repository, with Build Command `make server` and Start Command `./server`. The server reads the `PORT` variable that Render sets. The optional `ALLOWED_ORIGIN` variable restricts which website may call the API.
- **Page (Vercel):** import the same repository as a static site with Framework Preset **Other** and Root Directory `web`. Set `window.API_BASE` in `web/config.js` to the address of the Render service.

## Built with

- C++17 for the schedulers, data structures and server.
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) for the HTTP server (header-only, MIT licence). The JSON handling and all data structures are our own code.
- HTML, CSS and JavaScript for the interface, with no framework.

## Project Details

**Course:** Data Structures Mini Project (Project 3 - CPU Task Scheduling Simulator)

**Team Members:**

- Daksh Dhanotiya
- Suroj Krishna Chaitanya

**Repository:** https://github.com/SurojKrishnaChaitanya/cpu-task-scheduler
