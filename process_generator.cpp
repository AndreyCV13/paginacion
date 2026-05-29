#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define NEW_OP 0
#define NEW_PROB 15
#define USE_OP 1
#define USE_PROB 60
#define DELETE_OP 2
#define DELETE_PROB 10
#define KILL_OP 3
#define KILL_PROB 5

using namespace std;

struct Pointer {
  int ptrId;
  int pId;
  bool alive;
};

int main() {
  int P, N;

  cout << "Insert number of processes: ";
  cin >> P;

  cout << "Insert number of operation: ";
  cin >> N;

  ofstream outputFile("instructions.txt");

  if (!outputFile.is_open()) {
    cout << "Failed to open file" << endl;
    return 1;
  }

  mt19937 rng(time(nullptr));

  int nextPTR = 1;
  int nextPID = 1;

  unordered_set<int> aliveProcesses;

  unordered_map<int, Pointer> pointers;

  vector<int> alivePointers;

  int totalInstructions = 0;

  // // Propociones para las operaciones
  // int targetNew = (N * 10) / 100;
  // int targetUse = (N * 60) / 100;
  // int targetDelete = (N * 20) / 100;
  // int TargetKill = (N * 10) / 100;

  int newCount = 0;
  int useCount = 0;
  int deleteCount = 0;
  int killCount = 0;

  // Para tamaños random entre 1 y 10000
  uniform_int_distribution<int> sizeDist(1, 10000);

  auto randomAliveProcess = [&]() {
    vector<int> pids(aliveProcesses.begin(), aliveProcesses.end());

    uniform_int_distribution<int> pidDist(0, pids.size() - 1);

    return pids[pidDist(rng)];
  };

  auto randomAlivePointer = [&]() {
    uniform_int_distribution<int> ptrDist(0, alivePointers.size() - 1);

    return alivePointers[ptrDist(rng)];
  };

  while (totalInstructions < N) {
    // 0 = new
    // 1 = use
    // 2 = delete
    // 3 = kill
    vector<int> possibleOps;

    // if (newCount < targetNew) {
    //   possibleOps.push_back(0);
    // }
    //
    // if (useCount < targetUse && !alivePointers.empty()) {
    //   possibleOps.push_back(1);
    // }
    //
    // if (deleteCount < targetDelete && !alivePointers.empty()) {
    //   possibleOps.push_back(2);
    // }
    //
    // if (killCount < TargetKill && !aliveProcesses.empty()) {
    //   possibleOps.push_back(3);
    // }
    //
    // if (possibleOps.empty()) {
    //   break;
    // }

    if (nextPID <= P || !aliveProcesses.empty()) {
      possibleOps.insert(possibleOps.end(), NEW_PROB, NEW_OP);
    }

    if (!alivePointers.empty()) {
      possibleOps.insert(possibleOps.end(), USE_PROB, USE_OP);
    }

    if (!alivePointers.empty()) {
      possibleOps.insert(possibleOps.end(), DELETE_PROB, DELETE_OP);
    }

    if (!aliveProcesses.empty() &&
        !(totalInstructions < N - 1 && aliveProcesses.size() == 1)) {
      possibleOps.insert(possibleOps.end(), KILL_PROB, KILL_OP);
    }

    uniform_int_distribution<int> opDist(0, possibleOps.size() - 1);
    int op = possibleOps[opDist(rng)];

    if (op == NEW_OP) {
      int pid;

      // 50% de crear un nuevo proceso en vez de usar uno ya existente
      bool createNew =
          (nextPID <= P) &&
          (aliveProcesses.empty() || uniform_int_distribution<int>(0, 1)(rng));

      if (createNew) {
        pid = nextPID++;
        aliveProcesses.insert(pid);
      } else {
        pid = randomAliveProcess();
      }

      int size = sizeDist(rng);

      int ptr = nextPTR++;

      pointers[ptr] = {ptr, pid, true};

      alivePointers.push_back(ptr);

      outputFile << "new(" << pid << ", " << size << ")" << endl;

      newCount++;
    } else if (op == USE_OP) {
      int ptr = randomAlivePointer();

      outputFile << "use(" << ptr << ")" << endl;

      useCount++;
    } else if (op == DELETE_OP) {
      int ptr = randomAlivePointer();

      outputFile << "delete(" << ptr << ")" << endl;

      pointers[ptr].alive = false;

      alivePointers.erase(
          remove(alivePointers.begin(), alivePointers.end(), ptr),
          alivePointers.end());

      deleteCount++;
    } else if (op == KILL_OP) {
      int pid = randomAliveProcess();

      outputFile << "kill(" << pid << ")" << endl;

      aliveProcesses.erase(pid);

      vector<int> remainingPtrs;

      for (int ptr : alivePointers) {
        if (pointers[ptr].pId == pid) {
          pointers[ptr].alive = false;
        } else {
          remainingPtrs.push_back(ptr);
        }
      }

      alivePointers = remainingPtrs;

      killCount++;
    }

    totalInstructions++;
  }

  outputFile.close();

  cout << "Generated " << totalInstructions
       << " instructions in instructions.txt" << endl;
  cout << "Proportions:" << endl
       << "- new: " << newCount << endl
       << "- use: " << useCount << endl
       << "- delete: " << deleteCount << endl
       << "- kill: " << killCount << endl;

  return 0;
}
