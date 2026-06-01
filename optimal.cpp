#include "include/reversed_access_list.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ncurses.h>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ===== CONSTANTS =====
#define PAGE_SIZE 4000
#define RAM_PAGES 100
#define RAM_SIZE (PAGE_SIZE * 100)
#define MISS_TIME 5

// ==== GLOBAL VARIABLES ====
int disk_counter = 1;
bool using_tmp_file = false;
bool tmp_file_saved = false;
std::string current_instruction_file;

// ===== DEFINITIONS =====
enum class Status { SUCCESS, FILE_ERROR, SYNTAX_ERROR };

enum class Algorithms { OPTIMAL, FIFO, SC, LRU, LFU };

enum class RunResult { FINISHED, RESTART };

const char *AlgorithmStrings[] = {"Optimal", "FIFO", "Second Chance", "LRU",
                                  "LFU"};

struct statistics {
    int loaded_pages = 0;
    int unloaded_pages = 0;
};

struct command {
    std::string name;
    std::vector<int> params;
};

struct page {
    int pageId;
    int ptrId;
    int size;

    page(int page = -1, int ptr = -1, int page_size = 0)
        : pageId(page), ptrId(ptr), size(page_size) {}
};

struct symbol_entry {
    int pageId;
    int pId;
    bool loaded;
    int lAddr;
    int mAddr;
    int dAddr;
    int loadedT;
    int mark;
    int size;

    symbol_entry(int page, int process, bool load, int ptr, int page_size)
        : pageId(page), pId(process), loaded(load), lAddr(ptr), mAddr(),
          dAddr(), loadedT(), mark(), size(page_size) {}

    void print() {
        std::cout << pageId << "    | " << pId << " | " << loaded << " \t\t| "
                  << lAddr << "\t\t| " << mAddr << "\t\t| " << dAddr << "\t\t| "
                  << loadedT << "\t\t| " << mark << "\t|" << size << '\n';
    }
};

struct symbol_table {
    std::unordered_map<int, std::vector<symbol_entry>> table;
    int current_size = 0;

    void push(symbol_entry new_entry) {
        table[new_entry.lAddr].push_back(new_entry);
    }

    std::vector<symbol_entry> &lookup_ptr(int ptr) { return table.at(ptr); }

    symbol_entry &lookup_pageId(int pageId, int ptr) {
        std::vector<symbol_entry> &pages = lookup_ptr(ptr);
        for (auto &page : pages) {
            if (page.pageId == pageId)
                return page;
        }
        throw std::runtime_error("pageId no encontrado");
    }

    bool has_page_loaded(int pageId, int ptr) {
        symbol_entry &entry = lookup_pageId(pageId, ptr);
        return entry.loaded;
    }

    int first_mAddr(int ptr) {
        auto &pages = lookup_ptr(ptr);
        for (const auto &page : pages) {
            if (page.loaded)
                return page.mAddr;
        }
        return -1;
    }

    int get_pId(int pageId, int ptr) {
        symbol_entry &page = lookup_pageId(pageId, ptr);
        return page.pId;
    }

    int get_size(int pageId, int ptr) {
        symbol_entry &page = lookup_pageId(pageId, ptr);
        return page.size;
    }

    bool has_ptr_loaded(int ptr) { return first_mAddr(ptr) != -1; }

    int get_mark(int pageId, int ptr) {
        symbol_entry &entry = lookup_pageId(pageId, ptr);
        return entry.mark;
    }

    void update_mark(int pageId, int ptr, int mark) {
        symbol_entry &entry = lookup_pageId(pageId, ptr);
        entry.mark = mark;
    }

    void inserted(int pageId, int ptr, int mAddr, int mark, int current_time) {
        symbol_entry &entry = lookup_pageId(pageId, ptr);
        entry.loaded = true;
        entry.mAddr = mAddr;
        entry.dAddr = 0;
        entry.loadedT = current_time;
        entry.mark = mark;
    }

    void removed(int pageId, int ptr) {
        symbol_entry &entry = lookup_pageId(pageId, ptr);
        entry.loaded = false;
        entry.mAddr = -1;
        entry.dAddr = disk_counter++;
        entry.loadedT = -1;
    }

    void print() {
        std::cout << "PageId\t| PId\t| Loaded \t| L-Addr \t| M-Addr \t| D-Addr "
                     "\t| LoadedT \t| Mark \t| Size \n";

        for (int i = 1; i <= table.size(); ++i) {
            for (int j = 0; j < table.at(i).size(); ++j) {
                table.at(i).at(j).print();
            }
        }
        std::cout << std::endl;
    }
};

struct RAM {
    page memory[RAM_PAGES];
    std::set<int> unique_pages;
    int current_size = 0;

    RAM() = default;

    void push(page &new_page, int position) {
        memory[position] = new_page;
        if (current_size < RAM_PAGES)
            current_size++;
    }

    void pop(int position) {
        memory[position].ptrId = -1;
        memory[position].pageId = -1;
        current_size--;
    }

    bool is_full() { return current_size >= RAM_PAGES; }
};

struct MMU {
    RAM MMU_RAM;
    symbol_table table;
    Algorithms algorithm;
    int ptr_counter;
    int miss_counter = 0;
    int process_counter = 0;
    int simulation_time = 0;
    int ram_size = 0;
    float ram_porcentage = 0;
    int vram_size = 0;
    float vram_porcentage = 0;
    int trashing = 0;
    float trashing_porcentage = 0;
    int fragmentation = 0;
    int loaded_pages = 0;
    int unloaded_pages = 0;

    MMU(Algorithms used_algorithm)
        : MMU_RAM(), table(), algorithm(used_algorithm), ptr_counter(1) {}

    void update_statistics() {
        std::unordered_set<int> unique_process;
        fragmentation = 0;

        for (int i = 0; i < RAM_PAGES; ++i) {
            page &new_page = MMU_RAM.memory[i];
            int pageId = new_page.pageId;
            int ptrId = new_page.ptrId;
            if (pageId == 0 || pageId == -1 || ptrId == -1)
                continue;

            unique_process.insert(table.get_pId(pageId, ptrId));
            int size = table.get_size(pageId, ptrId);
            fragmentation += PAGE_SIZE - size;
        }

        process_counter = unique_process.size();
        loaded_pages = MMU_RAM.current_size;
        unloaded_pages = table.current_size - loaded_pages;
        ram_size = MMU_RAM.current_size * 4;
        ram_porcentage = (float)MMU_RAM.current_size / RAM_PAGES * 100.0f;
        vram_size = unloaded_pages * 4;
        if (ram_size != 0)
            vram_porcentage = (float)vram_size / ram_size * 100.0f;
        else
            vram_porcentage = 0;
        trashing = miss_counter * MISS_TIME;
        if (simulation_time != 0)
            trashing_porcentage = (float)trashing / simulation_time * 100.0f;
        else
            trashing_porcentage = 0;
    }

    void print(bool table_flag = true) {
        std::cout << "\nALG - " << AlgorithmStrings[(int)algorithm]
                  << std::endl;
        std::cout << "MISS COUNTER = " << miss_counter << std::endl;
        std::cout << "PROCESS COUNTER = " << process_counter << std::endl;
        std::cout << "VRAM SIZE = " << vram_size << std::endl;
        std::cout << "TRASHING = " << trashing << std::endl;
        std::cout << "LOADED PAGES = " << loaded_pages << std::endl;
        std::cout << "UNLOADED PAGES = " << unloaded_pages << std::endl;
        std::cout << "FRAGMENTATION = " << fragmentation << std::endl;
    }
};

// ==== MMUS ====
int dict_counter = 1;
int access_counter = 0;
std::vector<MMU> MMUs;

// ==== ALGORITHM VARIABLES ====
// OPTIMAL
access_list<int> optimal_index;
std::unordered_map<int, int> dictionary;

// FIFO-SC
int FIFO_pointer = 0;
std::vector<int> FIFO_queue;

// LRU
int LRU_pointer;

// ===== EXPORT LOGIC =====
// Ahora copia el archivo de instrucciones original sin modificar
void export_instructions_file() {
    if (current_instruction_file.empty()) return;
    
    std::ifstream src(current_instruction_file, std::ios::binary);
    std::ofstream dst("exported_instructions.txt", std::ios::binary);
    
    if (src && dst) {
        dst << src.rdbuf();
    }
}

// ===== FILL THE REVERSED ACCESS LIST =====
void access_page(int key, int pages) {
    for (int i = 0; i < pages; ++i)
        optimal_index.push(key, access_counter++);
}

void fill_optimal(command &cmd) {
    if (cmd.name == "new") {
        int pages = std::ceil(cmd.params[1] / (float)PAGE_SIZE);
        dictionary[dict_counter] = pages;
        access_page(dict_counter, pages);
        dict_counter++;
    }
    if (cmd.name == "use") {
        int pages = dictionary[cmd.params[0]];
        access_page(cmd.params[0], pages);
    }
}

// ===== ALGORITHM EXECUTION  =====

void inserted_RAM(int pageId, int ptr, int mAddr, MMU &selected, int mark = 0) {
    selected.table.inserted(pageId, ptr, mAddr, mark, selected.simulation_time);
}

void removed_RAM(int pageId, int ptr, int mAddr, MMU &selected) {
    selected.table.removed(pageId, ptr);
    if (selected.table.has_ptr_loaded(ptr) == false)
        selected.MMU_RAM.unique_pages.erase(ptr);
    selected.MMU_RAM.pop(mAddr);

    if (selected.algorithm == Algorithms::FIFO ||
        selected.algorithm == Algorithms::SC) {
        FIFO_queue.erase(
            std::remove(FIFO_queue.begin(), FIFO_queue.end(), mAddr),
            FIFO_queue.end());
    }
}

int FIFO_replacement(page &new_page, MMU &selected) {
    int index = FIFO_queue.front();

    page page_to_remove = selected.MMU_RAM.memory[index];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, index, selected);

    return index;
}

int SC_replacement(page &new_page, MMU &selected) {
    while (true) {
        int index = FIFO_queue.front();

        page page_to_remove = selected.MMU_RAM.memory[index];

        int pageId = page_to_remove.pageId;
        int ptrId = page_to_remove.ptrId;
        int mark = selected.table.get_mark(pageId, ptrId);

        if (mark == 0) {
            removed_RAM(pageId, ptrId, index, selected);
            return index;
        }

        selected.table.update_mark(pageId, ptrId, 0);
        FIFO_queue.erase(FIFO_queue.begin());
        FIFO_queue.push_back(index);
    }
}

int LRU_replacement(page &new_page, MMU &selected) {
    int smallest_mark = INT_MAX;
    int mAddr = -1;

    for (int i = 0; i < RAM_PAGES; i++) {
        page &candidate = selected.MMU_RAM.memory[i];
        int mark = selected.table.get_mark(candidate.pageId, candidate.ptrId);
        if (mark < smallest_mark) {
            smallest_mark = mark;
            mAddr = i;
        }
    }
    page &page_to_remove = selected.MMU_RAM.memory[mAddr];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    return mAddr;
}

int LFU_replacement(page &new_page, MMU &selected) {
    return LRU_replacement(new_page, selected);
}

int optimal_replacement(page &new_page, MMU &selected) {

    std::set<int> &s = selected.MMU_RAM.unique_pages;
    int ptr = new_page.ptrId;
    int replacement = optimal_index.farthest_element(s);
    int mAddr = selected.table.first_mAddr(replacement);

    page page_to_remove = selected.MMU_RAM.memory[mAddr];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    if (selected.table.has_ptr_loaded(page_to_remove.ptrId) == false)
        s.erase(page_to_remove.ptrId);

    return mAddr;
}

int algorithm_router(page &new_page, MMU &selected) {
    switch (selected.algorithm) {
    case Algorithms::FIFO:
        return FIFO_replacement(new_page, selected);
    case Algorithms::SC:
        return SC_replacement(new_page, selected);
    case Algorithms::LRU:
        return LRU_replacement(new_page, selected);
    case Algorithms::LFU:
        return LFU_replacement(new_page, selected);
    default:
        return optimal_replacement(new_page, selected);
    }
}

bool check_optimal(MMU &selected) {
    return selected.algorithm == Algorithms::OPTIMAL;
}

int basic_miss(page &new_page, MMU &selected) {
    for (int i = 0; i < RAM_PAGES; ++i) {
        if (selected.MMU_RAM.memory[i].ptrId == -1)
            return i;
    }

    return -1;
}

void replace_page(page &new_page, MMU &selected) {
    int mAddr;
    int ptr = new_page.ptrId;
    int mark = 0;

    if (!selected.MMU_RAM.is_full())
        mAddr = basic_miss(new_page, selected);
    else
        mAddr = algorithm_router(new_page, selected);

    if (mAddr == -1)
        return;

    if (selected.algorithm == Algorithms::LRU)
        mark = LRU_pointer++;
    else if (selected.algorithm == Algorithms::LFU)
        mark = 1;

    selected.MMU_RAM.push(new_page, mAddr);
    inserted_RAM(new_page.pageId, ptr, mAddr, selected, mark);

    if (selected.algorithm == Algorithms::FIFO ||
        selected.algorithm == Algorithms::SC) {
        FIFO_queue.push_back(mAddr);
    }

    if (check_optimal(selected)) {
        optimal_index.pop(ptr);
        selected.MMU_RAM.unique_pages.insert(ptr);
    }
}

void update_hit(page &new_page, MMU &selected) {
    if (check_optimal(selected))
        optimal_index.pop(new_page.ptrId);
    if (selected.algorithm == Algorithms::SC)
        selected.table.update_mark(new_page.pageId, new_page.ptrId, 1);
    if (selected.algorithm == Algorithms::LRU)
        selected.table.update_mark(new_page.pageId, new_page.ptrId,
                                   LRU_pointer++);
    if (selected.algorithm == Algorithms::LFU) {
        int current_mark =
            selected.table.get_mark(new_page.pageId, new_page.ptrId);
        selected.table.update_mark(new_page.pageId, new_page.ptrId,
                                   current_mark + 1);
    }
}

void check_RAM(std::vector<page> &pages, int ptr, MMU &selected) {
    for (size_t i = 0; i < pages.size(); ++i) {
        page &new_page = pages.at(i);
        if (selected.table.has_page_loaded(new_page.pageId, ptr)) {
            selected.simulation_time++;
            update_hit(new_page, selected);
        } else {
            selected.miss_counter++;
            selected.simulation_time += MISS_TIME;
            replace_page(new_page, selected);
        }
    }
}

void delete_pId(command &cmd, MMU &selected) {
    int pId = cmd.params[0];

    for (int i = 0; i < RAM_PAGES; ++i) {
        page &candidate = selected.MMU_RAM.memory[i];
        if (candidate.pageId == -1 || candidate.ptrId == -1)
            continue;
        if (selected.table.get_pId(candidate.pageId, candidate.ptrId) == pId)
            removed_RAM(candidate.pageId, candidate.ptrId, i, selected);
    }
}

void delete_ptr(command &cmd, MMU &selected) {
    int ptr = cmd.params[0];
    for (int i = 0; i < RAM_PAGES; ++i) {
        page &candidate = selected.MMU_RAM.memory[i];
        if (candidate.ptrId == ptr)
            removed_RAM(candidate.pageId, candidate.ptrId, i, selected);
    }
}

void load_ptr(int ptr, MMU &selected) {
    if (selected.table.table.find(ptr) == selected.table.table.end())
        return;
    std::vector<symbol_entry> &entries = selected.table.lookup_ptr(ptr);
    std::vector<page> pages;
    for (size_t i = 0; i < entries.size(); ++i) {
        pages.push_back(page(entries[i].pageId, ptr, entries[i].size));
    }
    check_RAM(pages, ptr, selected);
}

void use_ptr(command &cmd, MMU &selected) { load_ptr(cmd.params[0], selected); }

void create_ptr(command &cmd, MMU &selected) {
    int page_amount = std::ceil(cmd.params[1] / (float)PAGE_SIZE);
    int remaining_size = cmd.params[1];

    for (int i = 0; i < page_amount; ++i) {

        int current_page_size = std::min(remaining_size, PAGE_SIZE);
        symbol_entry new_ptr(++selected.table.current_size, cmd.params[0],
                             false, selected.ptr_counter, current_page_size);
        selected.table.push(new_ptr);
        remaining_size -= current_page_size;
    }
    load_ptr(selected.ptr_counter, selected);
    selected.ptr_counter++;
}

// ===== COMMANDS LOGIC =====
void reset_simulation_state() {
    disk_counter = 1;

    dict_counter = 1;
    access_counter = 0;

    optimal_index = access_list<int>();
    dictionary.clear();

    FIFO_pointer = 0;
    FIFO_queue.clear();

    LRU_pointer = 0;

    MMUs.clear();
}

Status execute_command(command &next_cmd, MMU &selected) {
    if (next_cmd.name == "new") {
        create_ptr(next_cmd, selected);
    } else if (next_cmd.name == "use") {
        use_ptr(next_cmd, selected);
    } else if (next_cmd.name == "delete")
        delete_ptr(next_cmd, selected);
    else if (next_cmd.name == "kill")
        delete_pId(next_cmd, selected);
    else {
        std::cerr << "Comando no identificado" << std::endl;
        return Status::SYNTAX_ERROR;
    }
    selected.update_statistics();
    return Status::SUCCESS;
}

std::vector<int> parse_params(const std::string &line) {
    std::vector<int> result;
    std::stringstream stream(line);
    std::string param;
    while (std::getline(stream, param, ','))
        result.push_back(std::stoi(param));
    return result;
}

Status parse_line(const std::string &line, command &new_command) {
    size_t open = line.find('(');
    size_t close = line.find(')');
    if (open == std::string::npos || close == std::string::npos)
        return Status::SYNTAX_ERROR;
    new_command.name = line.substr(0, open);
    std::string params = line.substr(open + 1, close - open - 1);
    new_command.params = parse_params(params);
    return Status::SUCCESS;
}

// ===== INTERFACE LOGIC (NCURSES) =====

void draw_mmu_state(MMU &mmu, int start_y, int start_x, std::string title,
                    int scroll_offset) {
    // 1. Título
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(start_y, start_x, "%s", title.c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    // 2. Grilla Dinámica de RAM (10x10)
    mvprintw(start_y + 1, start_x, "--- RAM (100 PAGINAS) ---");
    for (int r = 0; r < 10; ++r) {
        move(start_y + 2 + r, start_x);
        for (int c = 0; c < 10; ++c) {
            int mAddr = r * 10 + c;
            page &p = mmu.MMU_RAM.memory[mAddr];
            if (p.ptrId == -1 || p.pageId == -1) {
                printw("[ ]");
            } else {
                try {
                    symbol_entry &entry =
                        mmu.table.lookup_pageId(p.pageId, p.ptrId);
                    int process_color = 3 + (entry.pId % 6);
                    attron(COLOR_PAIR(process_color));
                    printw("[%d]", entry.pId % 10);
                    attroff(COLOR_PAIR(process_color));
                } catch (...) {
                    printw("[?]");
                }
            }
        }
    }

    // 3. Estadísticas
    int sy = start_y + 13;
    mvprintw(sy, start_x, "--- ESTADISTICAS ---");

    mvprintw(sy + 1, start_x, "Procesos Activos: %d", mmu.process_counter);
    mvprintw(sy + 2, start_x, "Sim-Time: %ds", mmu.simulation_time);
    mvprintw(sy + 3, start_x, "RAM: %d KB (%.1f%%)", mmu.ram_size,
             mmu.ram_porcentage);
    mvprintw(sy + 4, start_x, "V-RAM: %d KB (%.1f%% RAM)", mmu.vram_size,
             mmu.vram_porcentage);

    if (mmu.trashing_porcentage > 50.0) {
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(sy + 5, start_x, "Thrashing: %ds (%.1f%%)", mmu.trashing,
                 mmu.trashing_porcentage);
        attroff(COLOR_PAIR(1) | A_BOLD);
    } else {
        mvprintw(sy + 5, start_x, "Thrashing: %ds (%.1f%%)", mmu.trashing,
                 mmu.trashing_porcentage);
    }
    mvprintw(sy + 6, start_x, "Fragmentacion: %d B", mmu.fragmentation);

    // 4. Tabla de Páginas (Scrollable) Modificada para agregar TIME y MRK
    int ty = sy + 8;
    mvprintw(ty, start_x, "--- TABLA DE PAGINAS (Total: %d) ---",
             mmu.table.current_size);
    mvprintw(ty + 1, start_x, "  ID | PID | LOAD | L-ADR | M-ADR | D-ADR | TIME | MRK");

    std::vector<symbol_entry *> all_entries;
    for (auto &pair : mmu.table.table) {
        for (auto &entry : pair.second) {
            all_entries.push_back(&entry);
        }
    }

    std::sort(
        all_entries.begin(), all_entries.end(),
        [](symbol_entry *a, symbol_entry *b) { return a->pageId < b->pageId; });

    int line = 0;
    int max_lines = 14; // Lineas que se mostraran a la vez en la tabla

    for (size_t i = scroll_offset; i < all_entries.size() && line < max_lines;
         ++i) {
        symbol_entry *entry = all_entries[i];
        int process_color = 3 + (entry->pId % 6);

        attron(COLOR_PAIR(process_color));
        char loadedMark = entry->loaded ? 'X' : ' ';
        
        // Formato ajustado para acomodar las nuevas columnas
        mvprintw(ty + 2 + line, start_x, "%4d | %3d | %4c | %5d | %5d | %5d | %4d | %3d",
                 entry->pageId, entry->pId, loadedMark, entry->lAddr,
                 entry->mAddr, entry->dAddr, entry->loadedT, entry->mark);
        attroff(COLOR_PAIR(process_color));

        line++;
    }

    if (all_entries.size() > scroll_offset + max_lines) {
        mvprintw(ty + 2 + line, start_x, "... (Use Arriba/Abajo para navegar)");
    }
}

RunResult execute_program(std::ifstream &f) {
    nodelay(stdscr, TRUE);

    std::string line;
    bool paused = false;
    int table_scroll = 0;
    bool show_export_msg = false;
    int export_msg_timer = 0;

    while (true) {
        int ch = getch();

        // Controles de teclado
        if (ch == 'q' || ch == 'Q') {
            nodelay(stdscr, FALSE);
            return RunResult::RESTART;
        }
        if (ch == 'p' || ch == 'P')
            paused = !paused;
        if (ch == KEY_DOWN)
            table_scroll++;
        if (ch == KEY_UP && table_scroll > 0)
            table_scroll--;

        // Botón D para descargar archivo de instrucciones
        if (ch == 'd' || ch == 'D') {
            export_instructions_file();
            show_export_msg = true;
            export_msg_timer = 10; // Mostrar el mensaje un par de "frames"
        }

        if (paused && ch != KEY_DOWN && ch != KEY_UP && ch != 'd' &&
            ch != 'D') {
            attron(A_BOLD);
            mvprintw(0, 0,
                     "[ SIMULACION PAUSADA - 'P' Reanudar | Flechas Scrollear "
                     "| 'D' Exportar Inst | 'Q' Reiniciar ]");
            attroff(A_BOLD);
            refresh();
            usleep(100000);
            continue;
        }

        if (!paused) {
            if (getline(f, line)) {
                command new_command;
                if (parse_line(line, new_command) != Status::SYNTAX_ERROR) {
                    for (size_t i = 0; i < MMUs.size(); ++i)
                        execute_command(new_command, MMUs.at(i));
                }
            } else {
                break;
            }
        }

        clear();

        if (show_export_msg && export_msg_timer > 0) {
            attron(COLOR_PAIR(2) | A_BOLD);
            mvprintw(
                0, 0,
                "[ ARCHIVO EXPORTADO A 'exported_instructions.txt' ]");
            attroff(COLOR_PAIR(2) | A_BOLD);
            export_msg_timer--;
        } else {
            mvprintw(0, 0,
                     "[ SIMULACION EN CURSO - 'P' Pausar | Flechas Scrollear | "
                     "'D' Exportar Inst | 'Q' Reiniciar ]");
        }

        draw_mmu_state(MMUs[0], 2, 2, "MMU-OPT", table_scroll);
        draw_mmu_state(MMUs[1], 2, 60,
                       std::string("MMU-") +
                           AlgorithmStrings[(int)MMUs[1].algorithm],
                       table_scroll);

        refresh();
        usleep(
            150000); // 150ms para que la UI se mantenga fluida con las teclas
    }

    nodelay(stdscr, FALSE);
    attron(A_BOLD);
    mvprintw(39, 2,
             "[ FIN DE SIMULACION - Presione 'D' para exportar inst, 'Q' "
             "para reiniciar o "
             "cualquier tecla para salir ]");
    attroff(A_BOLD);

    while (true) {
        int final_ch = getch();
        if (final_ch == 'd' || final_ch == 'D') {
            export_instructions_file();
        }
        if (final_ch == 'q' || final_ch == 'Q') {
            return RunResult::RESTART;
        }
        if (final_ch == ERR) {
            return RunResult::FINISHED;
        }
    }
}

void read_file(std::ifstream &f) {
    std::string line;
    while (getline(f, line)) {
        command new_command;
        if (parse_line(line, new_command) == Status::SYNTAX_ERROR)
            continue;
        fill_optimal(new_command);
    }
}

RunResult open_file(const std::string &input_file) {
    // Almacenamos el nombre del archivo para que la exportación lo use
    current_instruction_file = input_file;

    std::ifstream file(input_file);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open the file." << std::endl;
        return RunResult::FINISHED;
    }
    read_file(file);
    file.clear();
    file.seekg(0, std::ios::beg);
    RunResult result = execute_program(file);
    file.close();
    return result;
}

int ncurses_menu(const std::string &title,
                 const std::vector<std::string> &options) {
    int selected = 0;

    while (true) {
        clear();

        attron(A_BOLD);
        mvprintw(2, 2, "%s", title.c_str());
        attroff(A_BOLD);

        for (size_t i = 0; i < options.size(); i++) {
            if ((int)i == selected)
                attron(A_REVERSE);

            mvprintw(5 + i, 4, "%s", options[i].c_str());

            if ((int)i == selected)
                attroff(A_REVERSE);
        }

        mvprintw(LINES - 2, 2, "ARRIBA/ABAJO = Mover   ENTER = Seleccionar");

        refresh();

        int ch = getch();

        switch (ch) {
        case KEY_UP:
            selected = (selected - 1 + options.size()) % options.size();
            break;

        case KEY_DOWN:
            selected = (selected + 1) % options.size();
            break;

        case '\n':
        case KEY_ENTER:
        case 13:
            return selected;
        }
    }
}

std::string select_file_screen() {
    namespace fs = std::filesystem;

    std::vector<std::string> files;

    for (auto &entry : fs::directory_iterator(".")) {
        if (entry.is_regular_file())
            files.push_back(entry.path().filename().string());
    }

    std::sort(files.begin(), files.end());

    if (files.empty())
        return "";

    int selected = ncurses_menu("Seleccionar Archivo de Instrucciones", files);

    return files[selected];
}

bool choose_generate_file() {
    std::vector<std::string> options = {"Abrir Archivo Existente",
                                        "Generar Nuevo Archivo"};

    int choice = ncurses_menu("Archivo de Instrucciones", options);

    return choice == 1;
}

std::string generator_screen() {
    echo();
    curs_set(1);

    char filename[256];
    char processes[32];
    char instructions[32];

    clear();

    mvprintw(2, 2, "Output file name:");
    move(3, 2);
    getnstr(filename, 255);

    mvprintw(5, 2, "Number of processes:");
    move(6, 2);
    getnstr(processes, 31);

    mvprintw(8, 2, "Number of instructions:");
    move(9, 2);
    getnstr(instructions, 31);

    noecho();
    curs_set(0);

    std::string cmd = "./generator " + std::string(processes) + " " +
                      std::string(instructions) + " \"" +
                      std::string(filename) + "\"";

    int result = system(cmd.c_str());

    clear();

    if (result == 0) {
        mvprintw(2, 2, "File generated successfully.");
    } else {
        mvprintw(2, 2, "Generator failed.");
    }

    mvprintw(4, 2, "Press any key to continue...");
    refresh();
    getch();

    return filename;
}

// ==== MAIN ====
int main() {
    initscr();
    start_color();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    // Paleta de colores de fondo de alto contraste para las filas de procesos
    init_pair(3, COLOR_WHITE, COLOR_RED);
    init_pair(4, COLOR_BLACK, COLOR_GREEN);
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);
    init_pair(6, COLOR_WHITE, COLOR_BLUE);
    init_pair(7, COLOR_WHITE, COLOR_MAGENTA);
    init_pair(8, COLOR_BLACK, COLOR_CYAN);

    while (true) {

        reset_simulation_state();
        clear();
        erase();
        refresh();

        std::vector<std::string> algorithms = {"FIFO", "Second Chance", "LRU",
                                               "LFU"};

        int selection = ncurses_menu("Seleccione Algoritmo", algorithms);

        bool generate_file = choose_generate_file();

        std::string filename;

        if (generate_file)
            filename = generator_screen();
        else
            filename = select_file_screen();

        if (filename.empty())
            continue;

        MMUs.push_back(MMU(Algorithms::OPTIMAL));
        MMUs.push_back(MMU((Algorithms)(selection + 1)));

        RunResult result = open_file(filename);

        if (result == RunResult::FINISHED)
            break;
    }

    endwin();
    return 0;
}