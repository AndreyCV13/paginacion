#include <iostream>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <set>
#include <cmath>
#include <string>
#include <climits>
#include <ncurses.h>
#include <unistd.h>
#include "include/reversed_access_list.h"

// ===== CONSTANTS =====
#define PAGE_SIZE 4000
#define RAM_PAGES 100
#define RAM_SIZE (PAGE_SIZE * 100)

// ==== GLOBAL VARIABLES ====
int disk_counter = 1;

// ===== DEFINITIONS =====
enum class Status {
    SUCCESS,
    FILE_ERROR,
    SYNTAX_ERROR
};

enum class Algorithms {
    OPTIMAL,
    FIFO,
    SC,
    LRU,
    LFU
};

const char* AlgorithmStrings[] = { "Optimal", "FIFO", "Second Chance", "LRU", "LFU" };

struct command{
    std::string name;
    std::vector<int> params;
};

struct page{
    int pageId;
    int ptrId;
    int size;

    page(int page = -1, int ptr = -1, int page_size = 0) : pageId(page), ptrId(ptr), size(page_size) {}
};

struct symbol_entry{
    int pageId;
    int pId;
    bool loaded;
    int lAddr;
    int mAddr;
    int dAddr;
    int loadedT;
    int mark;
    int size;

    symbol_entry(int page, int process, bool load, int ptr, int page_size) : pageId(page), pId(process),
			loaded(load), lAddr(ptr), mAddr(), dAddr(), loadedT(), mark(), size(page_size) {}
};

struct symbol_table{
    std::unordered_map<int, std::vector<symbol_entry>> table;
    int current_size = 0;

    void push(symbol_entry new_entry){
    	table[new_entry.lAddr].push_back(new_entry);
    } 

    std::vector<symbol_entry>& lookup_ptr(int ptr){
        return table.at(ptr);
    }

    symbol_entry& lookup_pageId(int pageId, int ptr){
        std::vector<symbol_entry>& pages = lookup_ptr(ptr);
        for(auto& page : pages){
            if(page.pageId == pageId) return page;
        }
        throw std::runtime_error("pageId no encontrado");
    }

    bool has_page_loaded(int pageId, int ptr){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        return entry.loaded;
    }

    int first_mAddr(int ptr){
        auto& pages = lookup_ptr(ptr);
        for(const auto& page : pages){
            if(page.loaded) return page.mAddr;
        }
        return -1;
    }

    bool get_pId(int pageId, int ptr, int pId){
        symbol_entry& page = lookup_pageId(pageId, ptr);
        return page.pId == pId;
    }
    
    bool has_ptr_loaded(int ptr){
        return first_mAddr(ptr) != -1;
    }

    int get_mark(int pageId, int ptr){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        return entry.mark;
    }

    void update_mark(int pageId, int ptr, int mark){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.mark = mark;
    }

    void inserted(int pageId, int ptr, int mAddr, int mark, int current_time){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.loaded = true;
    	entry.mAddr = mAddr;
	    entry.dAddr = 0;
	    entry.loadedT = current_time;
        entry.mark = mark;
    }

    void removed(int pageId, int ptr){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.loaded = false;
        entry.mAddr = -1;
        entry.dAddr = disk_counter++;
        entry.loadedT = -1;
    }
};

struct RAM{
    page memory[RAM_PAGES];
    std::set<int> unique_pages;
    int current_size = 0;

    RAM() = default;

    void push(page& new_page, int position){
	    memory[position] = new_page;
        if (current_size < RAM_PAGES) current_size++;
    }

    void pop(int position){
	    memory[position].ptrId = -1;
        current_size--;
    }

    bool is_full(){
        return current_size >= RAM_PAGES;
    }
};

struct MMU{
    RAM MMU_RAM;
    symbol_table table;
    Algorithms algorithm;
    int ptr_counter;
    int miss_counter;
    
    // Estadísticas
    int sim_time;
    int thrashing_time;
    int fragmentation;
    std::unordered_map<int, int> ptr_frag; 
    std::unordered_map<int, int> ptr_to_pid; 

    MMU(Algorithms used_algorithm) : MMU_RAM(), table(), 
        algorithm(used_algorithm), ptr_counter(1), miss_counter(0),
        sim_time(0), thrashing_time(0), fragmentation(0) {}
};

// ==== MMUS ====
int dict_counter = 1;
int access_counter = 0;
std::vector<MMU> MMUs;

// ==== ALGORITHM VARIABLES ====
access_list<int> optimal_index;
std::unordered_map<int, int> dictionary;
int FIFO_pointer = 0;
int LRU_pointer = 0;

// ===== FILL THE REVERSED ACCESS LIST =====
void access_page(int key, int pages){
    for(int i = 0; i < pages; ++i)
    	optimal_index.push(key, access_counter++);
}

void fill_optimal(command& cmd){
    if(cmd.name == "new"){
        int pages = std::ceil(cmd.params[1] / (float) PAGE_SIZE);
        dictionary[dict_counter] = pages;
        access_page(dict_counter, pages);
        dict_counter++;
    }
    if(cmd.name == "use"){
        int pages = dictionary[cmd.params[0]];
        access_page(cmd.params[0], pages);
    }
}

// ===== ALGORITHM EXECUTION  =====
void inserted_RAM(int pageId, int ptr, int mAddr, MMU& selected, int mark = 0){
    selected.table.inserted(pageId, ptr, mAddr, mark, selected.sim_time);
    if(selected.algorithm == Algorithms::FIFO || selected.algorithm == Algorithms::SC)
        FIFO_pointer = (FIFO_pointer + 1) % RAM_PAGES;
}

void removed_RAM(int pageId, int ptr, int mAddr, MMU& selected){
    selected.table.removed(pageId, ptr);
    if(selected.table.has_ptr_loaded(ptr) == false)
        selected.MMU_RAM.unique_pages.erase(ptr);
    selected.MMU_RAM.pop(mAddr);
}

int FIFO_replacement(page& new_page, MMU& selected){
    page page_to_remove = selected.MMU_RAM.memory[FIFO_pointer];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, FIFO_pointer, selected);
    return FIFO_pointer;
}

int SC_replacement(page& new_page, MMU& selected){
    while(true){
        page& page_to_remove = selected.MMU_RAM.memory[FIFO_pointer];
        int pageId = page_to_remove.pageId;
        int ptrId = page_to_remove.ptrId;
        int mark = selected.table.get_mark(pageId, ptrId);

        if(mark == 0){
            removed_RAM(pageId, ptrId, FIFO_pointer, selected);
            return FIFO_pointer;
        }

        selected.table.update_mark(pageId, ptrId, 0);
        FIFO_pointer = (FIFO_pointer + 1) % RAM_PAGES;
    }
}

int LRU_replacement(page& new_page, MMU& selected){
    int smallest_mark = INT_MAX;
    int mAddr = -1;

    for(int i = 0; i < RAM_PAGES; i++){
        page& candidate = selected.MMU_RAM.memory[i];
        int mark = selected.table.get_mark(candidate.pageId, candidate.ptrId); 
        if(mark < smallest_mark){
            smallest_mark = mark;
            mAddr = i;
        }
    }
    page& page_to_remove = selected.MMU_RAM.memory[mAddr];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    return mAddr;
}

int LFU_replacement(page& new_page, MMU& selected){
    return LRU_replacement(new_page, selected); 
}

int optimal_replacement(page& new_page, MMU& selected){
    std::set<int>& s = selected.MMU_RAM.unique_pages;
    int ptr = new_page.ptrId;
    int replacement = optimal_index.farthest_element(s);
    int mAddr = selected.table.first_mAddr(replacement);

    page page_to_remove = selected.MMU_RAM.memory[mAddr];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    if(selected.table.has_ptr_loaded(page_to_remove.ptrId) == false)
        s.erase(page_to_remove.ptrId);

    return mAddr;
}

int algorithm_router(page& new_page, MMU& selected){
    switch(selected.algorithm){
        case Algorithms::FIFO: return FIFO_replacement(new_page, selected);
        case Algorithms::SC: return SC_replacement(new_page, selected);
        case Algorithms::LRU: return LRU_replacement(new_page, selected);
        case Algorithms::LFU: return LFU_replacement(new_page, selected);
        default: return optimal_replacement(new_page, selected);
    }
}

bool check_optimal(MMU& selected) { return selected.algorithm == Algorithms::OPTIMAL; }

int basic_miss(page& new_page, MMU& selected){
    for(int i = 0; i < RAM_PAGES; ++i){
        if(selected.MMU_RAM.memory[i].ptrId == -1) return i;            
    }
    return -1;
}

void replace_page(page& new_page, MMU& selected){
    int mAddr;
    int ptr = new_page.ptrId;
    int mark = 0;

    if(!selected.MMU_RAM.is_full()) mAddr = basic_miss(new_page, selected);
    else mAddr = algorithm_router(new_page, selected);

    if(mAddr == -1) return;

    if(selected.algorithm == Algorithms::LRU) mark = LRU_pointer++;
    else if(selected.algorithm == Algorithms::LFU) mark = 1;

    selected.MMU_RAM.push(new_page, mAddr);
    inserted_RAM(new_page.pageId, ptr, mAddr, selected, mark);

    if(check_optimal(selected)){
        optimal_index.pop(ptr);
        selected.MMU_RAM.unique_pages.insert(ptr);
    }
}

void update_hit(page& new_page, MMU& selected){
    if(check_optimal(selected)) optimal_index.pop(new_page.ptrId);
    if(selected.algorithm == Algorithms::SC) selected.table.update_mark(new_page.pageId, new_page.ptrId, 1);
    if(selected.algorithm == Algorithms::LRU) selected.table.update_mark(new_page.pageId, new_page.ptrId, LRU_pointer++);
    if(selected.algorithm == Algorithms::LFU){
        int current_mark = selected.table.get_mark(new_page.pageId, new_page.ptrId);
        selected.table.update_mark(new_page.pageId, new_page.ptrId, current_mark + 1);
    }
}

void check_RAM(std::vector<page>& pages, int ptr, MMU& selected){
    for(size_t i = 0; i < pages.size(); ++i){
	    page& new_page = pages.at(i);
        if(selected.table.has_page_loaded(new_page.pageId, ptr)){
            selected.sim_time += 1;
            update_hit(new_page, selected);
        } else {
            selected.miss_counter++;
            selected.sim_time += 5;
            selected.thrashing_time += 5;
            replace_page(new_page, selected);        
        }
    }
}

void remove_fragmentation(int ptr, MMU& selected) {
    if (selected.ptr_frag.count(ptr)) {
        selected.fragmentation -= selected.ptr_frag[ptr];
        selected.ptr_frag.erase(ptr);
        selected.ptr_to_pid.erase(ptr);
    }
}

void delete_pId(command& cmd, MMU& selected){
    int pId = cmd.params[0];
    
    for(int i = 0; i < RAM_PAGES; ++i){
        page& candidate = selected.MMU_RAM.memory[i];
        if(candidate.pageId == -1 || candidate.ptrId == -1) continue;
        if(selected.table.get_pId(candidate.pageId, candidate.ptrId, pId))
            removed_RAM(candidate.pageId, candidate.ptrId, i, selected);
    }
    
    std::vector<int> to_delete;
    for (auto const& [ptr, pid] : selected.ptr_to_pid) {
        if (pid == pId) to_delete.push_back(ptr);
    }
    for (int ptr : to_delete) remove_fragmentation(ptr, selected);
}

void delete_ptr(command& cmd, MMU& selected){
    int ptr = cmd.params[0];
    for(int i = 0; i < RAM_PAGES; ++i){
        page& candidate = selected.MMU_RAM.memory[i];
        if(candidate.ptrId == ptr) removed_RAM(candidate.pageId, candidate.ptrId, i, selected);      
    }
    remove_fragmentation(ptr, selected);
}

void load_ptr(int ptr, MMU& selected){
    if (selected.table.table.find(ptr) == selected.table.table.end()) return;
    std::vector<symbol_entry>& entries = selected.table.lookup_ptr(ptr);
    std::vector<page> pages;
    for(size_t i = 0; i < entries.size(); ++i){
        pages.push_back(page(entries[i].pageId, ptr, entries[i].size));
    }
    check_RAM(pages, ptr, selected);
}

void use_ptr(command& cmd, MMU& selected){
    load_ptr(cmd.params[0], selected);
}

void create_ptr(command& cmd, MMU& selected){
    int page_amount = std::ceil(cmd.params[1] / (float) PAGE_SIZE);
    int frag = (page_amount * PAGE_SIZE) - cmd.params[1];
    
    selected.fragmentation += frag;
    selected.ptr_frag[selected.ptr_counter] = frag;
    selected.ptr_to_pid[selected.ptr_counter] = cmd.params[0];

    for(int i = 0; i < page_amount; ++i){
	    symbol_entry new_ptr(++selected.table.current_size, cmd.params[0], false, selected.ptr_counter, cmd.params[1]);
	    selected.table.push(new_ptr);
    }
    load_ptr(selected.ptr_counter, selected);
    selected.ptr_counter++;
}

// ===== COMMANDS LOGIC =====
Status execute_command(command& next_cmd, MMU& selected){
    if(next_cmd.name == "new") create_ptr(next_cmd, selected);
    else if(next_cmd.name == "use") use_ptr(next_cmd, selected);
    else if(next_cmd.name == "delete") delete_ptr(next_cmd, selected);
    else if(next_cmd.name == "kill") delete_pId(next_cmd, selected);
    else return Status::SYNTAX_ERROR;
    return Status::SUCCESS;
}

std::vector<int> parse_params(const std::string& line){
    std::vector<int> result;
    std::stringstream stream(line);
    std::string param;
    while (std::getline(stream, param, ',')) result.push_back(std::stoi(param));
    return result;
}

Status parse_line(const std::string& line, command& new_command){
    size_t open = line.find('(');
    size_t close = line.find(')');
    if(open == std::string::npos || close == std::string::npos) return Status::SYNTAX_ERROR;
    new_command.name = line.substr(0, open);
    std::string params = line.substr(open+1, close-open-1);
    new_command.params = parse_params(params);
    return Status::SUCCESS;
}

// ===== INTERFACE LOGIC (NCURSES) =====
void draw_mmu_state(MMU& mmu, int start_y, int start_x, std::string title) {
    attron(COLOR_PAIR(2) | A_BOLD);
    mvprintw(start_y, start_x, title.c_str());
    attroff(COLOR_PAIR(2) | A_BOLD);

    mvprintw(start_y + 1, start_x, "PAGE ID | PID | LOADED | L-ADDR | M-ADDR | D-ADDR");
    int line = 0;
    
    for(int i = 0; i < RAM_PAGES; ++i) {
        if (line > 15) break;
        page& p = mmu.MMU_RAM.memory[i];
        if (p.ptrId != -1) {
            symbol_entry& entry = mmu.table.lookup_pageId(p.pageId, p.ptrId);
            
            // Asignación dinámica de color según el PID de forma segura
            int process_color = 3 + (entry.pId % 6);
            
            attron(COLOR_PAIR(process_color));
            mvprintw(start_y + 2 + line, start_x, "%7d | %3d | %6d | %6d | %6d | %6d",
                entry.pageId, entry.pId, entry.loaded, entry.lAddr, entry.mAddr, entry.dAddr);
            attroff(COLOR_PAIR(process_color));
            
            line++;
        }
    }

    int sy = start_y + 19;
    mvprintw(sy, start_x, "--- ESTADISTICAS ---");
    
    std::set<int> active_pids;
    for (auto const& [ptr, pid] : mmu.ptr_to_pid) active_pids.insert(pid);
    mvprintw(sy + 1, start_x, "Procesos Activos: %lu", active_pids.size());
    mvprintw(sy + 2, start_x, "Sim-Time: %ds", mmu.sim_time);
    
    float ram_pct = (mmu.MMU_RAM.current_size * 4.0) / 400.0 * 100.0;
    mvprintw(sy + 3, start_x, "RAM: %d KB (%.1f%%)", (mmu.MMU_RAM.current_size * 4), ram_pct);
    
    int vram_pages = 0;
    for(size_t i = 1; i <= mmu.table.table.size(); ++i) {
        if (mmu.table.table.find(i) == mmu.table.table.end()) continue;
        for(size_t j = 0; j < mmu.table.table[i].size(); ++j) {
            if (!mmu.table.table[i][j].loaded && mmu.table.table[i][j].dAddr > 0) vram_pages++;
        }
    }
    float vram_kb = vram_pages * 4.0;
    float vram_pct = (vram_kb / 400.0) * 100.0;
    mvprintw(sy + 4, start_x, "V-RAM: %.0f KB (%.1f%% RAM)", vram_kb, vram_pct);

    float thrash_pct = mmu.sim_time == 0 ? 0 : ((float)mmu.thrashing_time / mmu.sim_time) * 100.0;
    if (thrash_pct > 50.0) {
        attron(COLOR_PAIR(1) | A_BOLD);
        mvprintw(sy + 5, start_x, "Thrashing: %ds (%.1f%%)", mmu.thrashing_time, thrash_pct);
        attroff(COLOR_PAIR(1) | A_BOLD);
    } else {
        mvprintw(sy + 5, start_x, "Thrashing: %ds (%.1f%%)", mmu.thrashing_time, thrash_pct);
    }
    mvprintw(sy + 6, start_x, "Fragmentacion: %d B", mmu.fragmentation);
}

void execute_program(std::ifstream& f){
    initscr(); start_color(); cbreak(); noecho(); nodelay(stdscr, TRUE); curs_set(0);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);

    // Paleta de colores de fondo de alto contraste para las filas de procesos
    init_pair(3, COLOR_WHITE, COLOR_RED);       // Proceso grupo 1
    init_pair(4, COLOR_BLACK, COLOR_GREEN);     // Proceso grupo 2
    init_pair(5, COLOR_BLACK, COLOR_YELLOW);    // Proceso grupo 3
    init_pair(6, COLOR_WHITE, COLOR_BLUE);      // Proceso grupo 4
    init_pair(7, COLOR_WHITE, COLOR_MAGENTA);   // Proceso grupo 5
    init_pair(8, COLOR_BLACK, COLOR_CYAN);      // Proceso grupo 6

    std::string line;
    bool paused = false;

    while (true) {
        int ch = getch();
        if (ch == 'p' || ch == 'P') paused = !paused;

        if (paused) {
            attron(A_BOLD);
            mvprintw(0, 0, "[ SIMULACION PAUSADA - Presione 'p' para reanudar ]");
            attroff(A_BOLD);
            refresh();
            usleep(100000);
            continue;
        }

        if (getline(f, line)) {
            command new_command;
            if (parse_line(line, new_command) == Status::SYNTAX_ERROR) continue;
            for(size_t i = 0; i < MMUs.size(); ++i) execute_command(new_command, MMUs.at(i));
        } else {
            break; 
        }

        clear();
        mvprintw(0, 0, "[ SIMULACION EN CURSO - Presione 'p' para pausar ]");
        
        draw_mmu_state(MMUs[0], 2, 2, "MMU-OPT");
        draw_mmu_state(MMUs[1], 2, 60, std::string("MMU-") + AlgorithmStrings[(int)MMUs[1].algorithm]);

        refresh();
        usleep(150000); 
    }

    nodelay(stdscr, FALSE);
    attron(A_BOLD);
    mvprintw(32, 2, "[ FIN DE SIMULACION - Presione cualquier tecla para salir ]");
    attroff(A_BOLD);
    getch();
    endwin();
}

void read_file(std::ifstream& f){
    std::string line;
    while (getline(f, line)) {
        command new_command;
        if (parse_line(line, new_command) == Status::SYNTAX_ERROR) continue;
        fill_optimal(new_command);
    }
}

Status open_file(const std::string& input_file) {
    std::ifstream file(input_file);
    if (!file.is_open()){
	    std::cerr << "Error: Could not open the file." << std::endl;
	    return Status::FILE_ERROR;
    }
    read_file(file);
    file.clear();
    file.seekg(0, std::ios::beg);
    execute_program(file);
    file.close();
    return Status::SUCCESS;
}

// ==== MAIN ====
int main(int argc, char **argv) {
    if (argc != 2) {
	    std::cerr << "Usage: ./optimal <input>" << std::endl;
        return 1;
    }

    int selection;
    std::cout << "Select an algorithm (1-4): \n";
    std::cout << "1. FIFO\n";
    std::cout << "2. Second Chance\n";
    std::cout << "3. LRU\n";
    std::cout << "4. LFU\n> ";
    std::cin >> selection;

    if(selection <= 0 || selection >= 5){
        std::cerr << "The number must be between 1 and 4" << std::endl;
        return 1;
    }

    MMUs.push_back(MMU(Algorithms::OPTIMAL));
    MMUs.push_back(MMU((Algorithms) selection));

    if (open_file(argv[1]) == Status::FILE_ERROR) return 1;

    return 0;
}