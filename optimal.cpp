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
#include "include/reversed_access_list.h"

// ===== CONSTANTS =====
#define PAGE_SIZE 4000
#define RAM_PAGES 100
#define RAM_SIZE (PAGE_SIZE * 100)

// ==== GLOBAL VARIABLES ====
int global_timer = 0;
int disk_counter = 1;

// ===== DEFINITIONS =====
enum class Status {
    OK,
    FILERROR,
    SYNTAXERROR
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

    void print(){
	std::cout << "Command: " << name << '\n';
	std::cout << "Params: ";

	for(int i = 0; i < params.size(); ++i)
	    std::cout << params.at(i) << ' ';
	std::cout << '\n' << std::endl;
    }
};


struct page{
    int pageId;
    int ptrId;
    int size;

    page(int page = -1, int ptr = -1, int page_size = 0) : pageId(page), ptrId(ptr), size(page_size) {}

    void print(){
	std::cout << ptrId << ' ';
    }
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

    void print(){
	std::cout << pageId << "	| "
		<< pId << "	| "
		<< loaded << " \t\t| "
		<< lAddr << "\t\t| "
		<< mAddr << "\t\t| "
		<< dAddr << "\t\t| "
		<< loadedT << "\t\t| "
		<< mark << '\n';

    }
};

struct symbol_table{
    //std::vector<symbol_entry> table;
    std::unordered_map<int, std::vector<symbol_entry>> table;
    int current_size;


    void push(symbol_entry new_entry){
	    //table.push_back(new_entry);
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

    void inserted(int pageId, int ptr, int mAddr, int mark){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.loaded = true;
    	entry.mAddr = mAddr;
	    entry.dAddr = 0;
	    entry.loadedT = global_timer;
        entry.mark = mark;
    }

    void removed(int pageId, int ptr){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.loaded = false;
        entry.mAddr = -1;
        entry.dAddr = disk_counter++;
        entry.loadedT = -1;
    }

    void print(){
        std::cout << "PageId\t| PId\t| Loaded \t| L-Addr \t| M-Addr \t| D-Addr \t| LoadedT \t| Mark \n";

        for(int i = 1; i <= table.size(); ++i){
            for(int j = 0; j < table.at(i).size(); ++j){
                table.at(i).at(j).print();
            }
        }
        std::cout << std::endl;
    }
};

struct RAM{
    page memory[RAM_PAGES];
    //Un set no es realmente necesasrio?
    std::set<int> unique_pages;
    int current_size;

    RAM() = default;

    void push(page& new_page, int position){
	    memory[position] = new_page;
        
        if (current_size < RAM_PAGES)
            current_size++;
    }

    void pop(int position){
	    memory[position].ptrId = -1;
        current_size--;
    }

    bool is_full(){
        return current_size >= RAM_PAGES;
    }

    void print(){
        std::cout << "RAM = ";
        for(int i = 0; i < RAM_PAGES; ++i)
            memory[i].print();
        std::cout << "\nUniques = ";
        for (const auto& element : unique_pages) {
                std::cout << element << " ";
        }
        std::cout << "\nSize  = " << current_size << '\n' << std::endl;
    }
};

struct MMU{
    RAM MMU_RAM;
    symbol_table table;
    Algorithms algorithm;
    int ptr_counter;
    int miss_counter;

    MMU(Algorithms used_algorithm) : MMU_RAM(), table(), 
        algorithm(used_algorithm), ptr_counter(1), miss_counter(0) {}

    void print(bool table_flag = true){
        std::cout << "ALG - " << AlgorithmStrings[(int) algorithm] << std::endl;
        if(table_flag){
            table.print();
            MMU_RAM.print();
        }
        std::cout << "MISS COUNTER = " << miss_counter << std::endl;
    }
};

// ==== MMUS ====
//ptr(1)...
int dict_counter = 1;
int access_counter = 0;
std::vector<MMU> MMUs;

// ==== ALGORITHM VARIABLES ====
//OPTIMAL
//RAM optimal_RAM;
//symbol_table optimal_table;
access_list<int> optimal_index;
std::unordered_map<int, int> dictionary;

//FIFO-SC
int FIFO_pointer = 0;

//LRU
int LRU_pointer;

// ===== FILL THE REVERSED ACCESS LIST =====

void access_page(int key, int pages){
    for(int i = 0; i < pages; ++i)
    	optimal_index.push(key, access_counter++);
}

void fill_optimal(command& cmd){
    if(cmd.name == "new"){
        int pages = std::ceil(cmd.params[1] / (float) PAGE_SIZE);
        std::cout << "Cantidad =" << pages << std::endl;

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
//Depende del algoritmo...
//WIP

void inserted_RAM(int pageId, int ptr, int mAddr, MMU& selected, int mark = 0){
    selected.table.inserted(pageId, ptr, mAddr, mark);

    if(selected.algorithm == Algorithms::FIFO
        || selected.algorithm == Algorithms::SC)
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
        int pageId = candidate.pageId;
        int ptrId = candidate.ptrId;
        int mark = selected.table.get_mark(pageId, ptrId); 
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
    int smallest_mark = INT_MAX;
    int mAddr = -1;

    for(int i = 0; i < RAM_PAGES; i++){
        page& candidate = selected.MMU_RAM.memory[i];
        int pageId = candidate.pageId;
        int ptrId = candidate.ptrId;
        int mark = selected.table.get_mark(pageId, ptrId); 
        if(mark < smallest_mark){
            smallest_mark = mark;
            mAddr = i;
        }
    }

    page& page_to_remove = selected.MMU_RAM.memory[mAddr];
    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    return mAddr;
}

int optimal_replacement(page& new_page, MMU& selected){
    std::set<int>& s = selected.MMU_RAM.unique_pages;
    int ptr = new_page.ptrId;

    //This is the ptr, not the index
    int replacement = optimal_index.farthest_element(s);
    int mAddr = selected.table.first_mAddr(replacement);

    //Update MMU and unique set
    page page_to_remove = selected.MMU_RAM.memory[mAddr];

    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, mAddr, selected);
    if(selected.table.has_ptr_loaded(page_to_remove.ptrId) == false)
        s.erase(page_to_remove.ptrId);

    return mAddr;
}

int algorithm_router(page& new_page, MMU& selected){
    switch(selected.algorithm){
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

    return -1;
}

bool check_optimal(MMU& selected){
    return selected.algorithm == Algorithms::OPTIMAL;
}

int basic_miss(page& new_page, MMU& selected){
    for(int i = 0; i < RAM_PAGES; ++i){
        if(selected.MMU_RAM.memory[i].ptrId == -1){
            return i;            
        }
    }

    //selected.MMU_RAM.push(new_page);
    //inserted_RAM(new_page.pageId, ptr, mAddr, selected);
    return -1;
}

void replace_page(page& new_page, MMU& selected){
    int mAddr;
    int ptr = new_page.ptrId;
    int mark = 0;

    if(!selected.MMU_RAM.is_full()) 
        mAddr = basic_miss(new_page, selected);
    else 
        mAddr = algorithm_router(new_page, selected);

    if(mAddr == -1) {
        return;
    }

    std::cout << "mAddr = " << mAddr << std::endl; 
    
    //Set marks
    if(selected.algorithm == Algorithms::LRU){
        mark = LRU_pointer++;
    } else if(selected.algorithm == Algorithms::LFU){
        mark = 1;
    }

    selected.MMU_RAM.push(new_page, mAddr);
    inserted_RAM(new_page.pageId, ptr, mAddr, selected, mark);

    if(check_optimal(selected)){
        optimal_index.pop(ptr);
        selected.MMU_RAM.unique_pages.insert(ptr);
    }
}

//Updates something if necessary
void update_hit(page& new_page, MMU& selected){

    //Consume an indexe if OPTIMAL
    if(check_optimal(selected)){
        optimal_index.pop(new_page.ptrId);
    }
    if(selected.algorithm == Algorithms::SC)
        selected.table.update_mark(new_page.pageId, new_page.ptrId, 1);

    if(selected.algorithm == Algorithms::LRU)
        selected.table.update_mark(new_page.pageId, new_page.ptrId, LRU_pointer++);

    if(selected.algorithm == Algorithms::LFU){
        int current_mark = selected.table.get_mark(new_page.pageId, new_page.ptrId);
        selected.table.update_mark(new_page.pageId, new_page.ptrId, current_mark + 1);
    }
}

void check_RAM(std::vector<page>& pages, int ptr, MMU& selected){
    for(int i = 0; i < pages.size(); ++i){
	    page& new_page = pages.at(i);
        if(selected.table.has_page_loaded(new_page.pageId, ptr)){
            std::cout << "Hit!\n" << std::endl;
            global_timer++;
            update_hit(new_page, selected);
            
        } else{
            std::cout << "Miss!\n" << std::endl;
            selected.miss_counter++;
            global_timer += 5;
            replace_page(new_page, selected);        
        }
    }
}

void delete_pId(command& cmd, MMU& selected){
    int pId = cmd.params[0];
    for(int i = 0; i < RAM_PAGES; ++i){
        page& candidate = selected.MMU_RAM.memory[i];
        if(candidate.pageId == -1 || candidate.ptrId == -1)
            continue;
        if(selected.table.get_pId(candidate.pageId, candidate.ptrId, pId))
            removed_RAM(candidate.pageId, candidate.ptrId, i, selected);
    }
}

void delete_ptr(command& cmd, MMU& selected){
    int ptr = cmd.params[0];
    for(int i = 0; i < RAM_PAGES; ++i){
        page& candidate = selected.MMU_RAM.memory[i];
        if(candidate.ptrId == ptr){
            removed_RAM(candidate.pageId, candidate.ptrId, i, selected);      
        }
    }
}

//Si no encuentra ptr, simplemente lo ignora
//Se podrría simplemente cambiar los datos de RAM, mejor perfomance, pero meh
void load_ptr(int ptr, MMU& selected){
    //Debug
    //optimal_index.print();
    //selected.MMU_RAM.print();

    std::vector<symbol_entry>& entries = selected.table.lookup_ptr(ptr);
    std::vector<page> pages;

    for(int i = 0; i < entries.size(); ++i){
        int pageId = entries[i].pageId;
        int pageSize = entries[i].size;
        page new_page(pageId, ptr, pageSize);
        pages.push_back(new_page);
    }
    check_RAM(pages, ptr, selected);
}

void use_ptr(command& cmd, MMU& selected){
    load_ptr(cmd.params[0], selected);
}

void create_ptr(command& cmd, MMU& selected){
    int page_amount = std::ceil(cmd.params[1] / (float) PAGE_SIZE);

    for(int i = 0; i < page_amount; ++i){
	symbol_entry new_ptr(++selected.table.current_size, cmd.params[0], false, selected.ptr_counter, cmd.params[1]);
	selected.table.push(new_ptr);
	//optimal_table.table.push_back(new_ptr);
    }

    load_ptr(selected.ptr_counter, selected);
    selected.ptr_counter++;
}

// ===== COMMANDS LOGIC =====

Status execute_command(command& next_cmd, MMU& selected){
    if(next_cmd.name == "new"){
	    create_ptr(next_cmd, selected);
    } else if(next_cmd.name == "use"){
	    use_ptr(next_cmd, selected);
    } else if(next_cmd.name == "delete")
	    delete_ptr(next_cmd, selected);
    else if(next_cmd.name == "kill")
	    delete_pId(next_cmd, selected);
    else {
	    std::cerr << "Comando no identificado" << std::endl;
	    return Status::SYNTAXERROR;
    }
    return Status::OK;
}

std::vector<int> parse_params(const std::string& line){
    std::vector<int> result;
    std::string cleaned;

    std::stringstream stream(line);
    std::string param;

    while (std::getline(stream, param, ',')) {
        result.push_back(std::stoi(param));
    }

    return result;
}

Status parse_line(const std::string& line, command& new_command){
    size_t open = line.find('(');
    size_t close = line.find(')');

    if(open == std::string::npos || close == std::string::npos)
	return Status::SYNTAXERROR;

    new_command.name = line.substr(0, open);

    std::string params = line.substr(open+1, close-1);
    new_command.params = parse_params(params);

    //Prints
    std::cout << "Original Line: " << line << std::endl;
    new_command.print();

    return Status::OK;
}

// ===== FILE LOGIC =====

void execute_program(std::ifstream& f){
    uint64_t size = 0;
    std::string line;

    while (getline(f, line)) {
        command new_command;

        if (parse_line(line, new_command) == Status::SYNTAXERROR)
            continue;

        //Validate cmd?? (context)
        //validate_command()

        for(int i = 0; i < MMUs.size(); ++i)
            execute_command(new_command, MMUs.at(i));

        size++;
    }
}

void read_file(std::ifstream& f){
    uint64_t size = 0;
    std::string line;

    while (getline(f, line)) {
        command new_command;

        if (parse_line(line, new_command) == Status::SYNTAXERROR)
            continue;

        //Validate cmd?? (context)
        //validate_command()

        fill_optimal(new_command);
        size++;
    }


}

Status open_file(const std::string& input_file) {
    std::ifstream file(input_file);
    if (!file.is_open()){
	std::cerr << "Error: Could not open the file." << std::endl;
	return Status::FILERROR;
    }

    //Optimal algorithm
    read_file(file);


    file.clear();
    file.seekg(0, std::ios::beg);

    std::cout << "Ejecución del programa! \n\n\n" << std::endl;
    execute_program(file);

    file.close();
    return Status::OK;
}


// ==== TEST?? ====

void test(){
    optimal_index.print();
    std::cout << "pop in page 1 = " << optimal_index.pop(1) << std::endl;
    optimal_index.print();

    std::cout << "pop in page 2 = " << optimal_index.pop(2) << std::endl;
    optimal_index.print();

}

// ==== MAIN ====

int main(int argc, char **argv) {
    if (argc != 2) {
	std::cerr << "Usage: optimal <input>" << std::endl;
        return 1;
    }

    int selection;
    std::cout << "Select an algorithm (1-4): \n";
    std::cout << "1. FIFO\n";
    std::cout << "2. Second Chance\n";
    std::cout << "3. LRU\n";
    std::cout << "4. LFU" << std::endl;
    std::cin >> selection;

    if(selection <= 0 || selection >= 5){
        std::cerr << "The number must be between 1 and 4" << std::endl;
        return 1;
    }

    MMUs.push_back(MMU(Algorithms::OPTIMAL));
    MMUs.push_back(MMU((Algorithms) selection));

    const char* s = AlgorithmStrings[(int) MMUs.at(1).algorithm];
    std::cout << "Algorithm selected = " << s << std::endl;


    if (open_file(argv[1]) == Status::FILERROR)
	return 1;

    for(int i = 0; i < MMUs.size(); ++i){
        MMUs.at(i).print();
    }

    return 0;
}
