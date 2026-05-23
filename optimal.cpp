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

    page(int page = 0, int ptr = 0, int page_size = 0) : pageId(page), ptrId(ptr), size(page_size) {}

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
        std::cout << "ptr" << ptr << std::endl;

        return table.at(ptr);
    }

    symbol_entry& lookup_pageId( int pageId, int ptr){
        std::vector<symbol_entry>& pages = lookup_ptr(ptr);
        for(auto& page : pages){
            if(page.pageId == pageId) return page;
        }
        throw std::runtime_error("pageId no encontrado");
    }

    bool has_page_loaded( int pageId, int ptr){
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

    bool has_ptr_loaded(int ptr){
        return first_mAddr(ptr) != -1;
    }

    void inserted(int pageId, int ptr, int mAddr){
        symbol_entry& entry = lookup_pageId(pageId, ptr);
        entry.loaded = true;
    	entry.mAddr = mAddr;
	    entry.dAddr = 0;
	    entry.loadedT = global_timer;
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

    void push(page& new_page){
	    memory[current_size++] = new_page;
    }

    void push(page& new_page, int position){
	    memory[position] = new_page;
    }

    bool is_full(){
        return current_size >= RAM_PAGES;
    }

    void print(){
        std::cout << "RAM = ";
        for(int i = 0; i < current_size; ++i)
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

    MMU(Algorithms used_algorithm) : algorithm(used_algorithm) {}
};

// ==== MMUS ====

//ptr(1)...
int ptr_counter = 1;
int access_counter = 0;
std::vector<MMU> MMUs;

// ==== OPTIMAL VARIABLES ====
//Esto deberia estar en otro lado
//RAM optimal_RAM;
//symbol_table optimal_table;
access_list<int> optimal_index;
std::unordered_map<int, int> dictionary;

// ===== FILL THE REVERSED ACCESS LIST =====

void access_page(int key, int pages){
    for(int i = 0; i < pages; ++i)
    	optimal_index.push(key, access_counter++);
}

void fill_optimal(command& cmd){
    if(cmd.name == "new"){
        int pages = std::ceil(cmd.params[1] / (float) PAGE_SIZE);
        std::cout << "Cantidad =" << pages << std::endl;

        dictionary[ptr_counter] = pages;
        access_page(ptr_counter, pages);
        ptr_counter++;
    }
    if(cmd.name == "use"){
        int pages = dictionary[cmd.params[0]];
        access_page(cmd.params[0], pages);
    }
}

// ===== ALGORITHM EXECUTION  =====
//Depende del algoritmo...
//WIP

void inserted_RAM(int pageId, int ptr, int mAddr, MMU& selected){
    selected.table.inserted(pageId, ptr, mAddr);
}

void removed_RAM(int pageId, int ptr, MMU& selected){
    selected.table.removed(pageId, ptr);
}

void FIFO_replacement(page& new_page, MMU& selected){
    std::cout << "WIP" << std::endl;
}

void SC_replacement(page& new_page, MMU& selected){
    std::cout << "WIP" << std::endl;

}

void LRU_replacement(page& new_page, MMU& selected){
    std::cout << "WIP" << std::endl;

}

void LFU_replacement(page& new_page, MMU& selected){
    std::cout << "WIP" << std::endl;

}

void optimal_replacement(page& new_page, MMU& selected){
    std::set<int>& s = selected.MMU_RAM.unique_pages;
    int ptr = new_page.ptrId;

    //This is the ptr, not the index
    int replacement = optimal_index.farthest_element(s);
    int mAddr = selected.table.first_mAddr(replacement);

    //Update MMU and unique set
    page page_to_remove = selected.MMU_RAM.memory[mAddr];

    removed_RAM(page_to_remove.pageId, page_to_remove.ptrId, selected);
    if(selected.table.has_ptr_loaded(page_to_remove.ptrId) == false)
        s.erase(page_to_remove.ptrId);

    //Update MMU, access list and unique set
    selected.MMU_RAM.push(new_page, mAddr);
    inserted_RAM(new_page.pageId, ptr, mAddr, selected);
    optimal_index.pop(ptr);
    s.insert(ptr);
}

void algorithm_router(page& new_page, MMU& selected){
    switch(selected.algorithm){
        case Algorithms::FIFO:
            break;
        case Algorithms::SC:
            break;
        case Algorithms::LRU:
            break;
        case Algorithms::LFU:
            break;
        default:
            optimal_replacement(new_page, selected);
    }
}

bool check_optimal(MMU& selected){
    return selected.algorithm == Algorithms::OPTIMAL;
}

void basic_miss(page& new_page, MMU& selected){
    int mAddr = (selected.MMU_RAM.current_size);
    int ptr = new_page.ptrId;
    selected.MMU_RAM.push(new_page);
    inserted_RAM(new_page.pageId, ptr, mAddr, selected);

    if(check_optimal(selected)){
        optimal_index.pop(ptr);
        selected.MMU_RAM.unique_pages.insert(ptr);
    }
}

void check_RAM(std::vector<page>& pages, int ptr, MMU& selected){
    for(int i = 0; i < pages.size(); ++i){
	    page& new_page = pages.at(i);
        if(selected.table.has_page_loaded(new_page.pageId, ptr)){
            std::cout << "Hit!\n" << std::endl;
            global_timer++;

            //Siempre se consume índice
            if(check_optimal(selected)){
                optimal_index.pop(ptr);
            }

        } else{
            std::cout << "Miss!\n" << std::endl;
            global_timer += 5;

            if(!selected.MMU_RAM.is_full()) {
                basic_miss(new_page, selected);
            }else {
                algorithm_router(new_page, selected);
            }

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
	symbol_entry new_ptr(++selected.table.current_size, cmd.params[0], false, ptr_counter, cmd.params[1]);
	selected.table.push(new_ptr);
	//optimal_table.table.push_back(new_ptr);
    }

    load_ptr(ptr_counter, selected);
    ptr_counter++;
}

// ===== COMMANDS LOGIC =====

Status execute_command(command& next_cmd, MMU& selected){
    if(next_cmd.name == "new"){
	    create_ptr(next_cmd, selected);
    } else if(next_cmd.name == "use"){
	    use_ptr(next_cmd, selected);
    } else if(next_cmd.name == "delete")
	    std::cout << std::endl;
    else if(next_cmd.name == "kill")
	    std::cout << std::endl;
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
    ptr_counter = 1;


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
    MMUs.push_back(MMU(Algorithms::OPTIMAL));

    if (argc != 2) {
	std::cerr << "Usage: optimal <input>" << std::endl;
        return 1;
    }

    if (open_file(argv[1]) == Status::FILERROR)
	return 1;

    for(int i = 0; i < MMUs.size(); ++i){
        MMUs.at(i).table.print();
        MMUs.at(i).MMU_RAM.print();
    }


    //test();

    return 0;
}
