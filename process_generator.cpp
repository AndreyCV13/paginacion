#include <fstream>
#include <iostream>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

struct Pointer {
    int ptrId;
    int pId;
    bool alive;
};

int main() {
    int N;

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

    // Propociones para las operaciones
    int targetNew = (N * 10) / 100;
    int targetUse = (N * 60) / 100;
    int targetDelete = (N * 20) / 100;
    int TargetKill = (N * 10) / 100;

    int newCount = 0;
    int useCount = 0;
    int deleteCount = 0;
    int killCount = 0;

    // Para tamaños random entre 1 y 10000
    uniform_int_distribution<int> sizeDist(1, 10000);

    while (totalInstructions < N) {
        // 0 = new
        // 1 = use
        // 2 = delete
        // 3 = kill
        vector<int> possibleOps;

        if (newCount < targetNew) {
            possibleOps.push_back(0);
        }

        if (useCount < targetUse && !alivePointers.empty()) {
            possibleOps.push_back(1);
        }

        if (deleteCount < targetDelete && !alivePointers.empty()) {
            possibleOps.push_back(2);
        }

        if (killCount < TargetKill && !aliveProcesses.empty()) {
            possibleOps.push_back(3);
        }

        if (possibleOps.empty()) {
            break;
        }

        uniform_int_distribution<int> opDist(0, possibleOps.size() - 1);
        int op = possibleOps[opDist(rng)];

        if (op == 0) {
            int pid;

            // 50% de crear un nuevo proceso en vez de usar uno ya existente
            bool createNew = aliveProcesses.empty() ||
                             uniform_int_distribution<int>(0, 1)(rng);

            if (createNew) {
                pid = nextPID++;
                aliveProcesses.insert(pid);
            } else {
                vector<int> pids(aliveProcesses.begin(), aliveProcesses.end());

                uniform_int_distribution<int> pidDist(0, pids.size() - 1);

                pid = pids[pidDist(rng)];
            }

            int size = sizeDist(rng);

            int ptr = nextPTR++;

            pointers[ptr] = {ptr, pid, true};

            alivePointers.push_back(ptr);

            outputFile << "new(" << pid << ", " << size << ")" << endl;

            newCount++;
        } else if (op == 1) {
            uniform_int_distribution<int> ptrDist(0, alivePointers.size() - 1);

            int ptr = alivePointers[ptrDist(rng)];

            outputFile << "use(" << ptr << ")" << endl;

            useCount++;
        } else if (op == 2) {
            uniform_int_distribution<int> ptdDist(0, alivePointers.size() - 1);

            int index = ptdDist(rng);

            int ptr = alivePointers[index];

            outputFile << "delete(" << ptr << ")" << endl;

            pointers[ptr].alive = false;

            alivePointers.erase(alivePointers.begin() + index);

            deleteCount++;
        } else if (op == 3) {
            vector<int> pids(aliveProcesses.begin(), aliveProcesses.end());

            uniform_int_distribution<int> pidDist(0, pids.size() - 1);

            int pid = pids[pidDist(rng)];

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
