#include <iostream>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include "include/reversed_access_list.h"

// ===== CONSTANTS =====
#define PAGE_SIZE 4000
#define RAM_PAGES 100
#define RAM_SIZE (PAGE_SIZE * 100)


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
    MRU,
    RND
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

};

struct symbol_table{
    std::vector<symbol_entry> table;

    std::vector<page> lookup_pages(int ptr){
	std::vector<page> result;
	for(int i = 0; i < table.size(); ++i){
	    if(table[i].lAddr == ptr){
		page new_page(table[i].pageId, table[i].lAddr, table[i].size);
	    	result.push_back(new_page);
	    }
	}
	return result;
    }

    void print(){
	std::cout << "PageId\t| PId\t| Loaded \t| L-Addr \t| M-Addr \t| D-Addr \t| LoadedT \t| Mark \n";

	for(int i = 0; i < table.size(); ++i){
	    std::cout << table[i].pageId << "	| "
			<< table[i].pId << "	| "
			<< table[i].loaded << " \t\t| "
			<< table[i].lAddr << "\t\t| "
			<< table[i].mAddr << "\t\t| "
			<< table[i].dAddr << "\t\t| "
			<< table[i].loadedT << "\t\t| "
			<< table[i].mark << '\n';
	}
	std::cout << std::endl;
    }
};

struct RAM{
    page memory[RAM_PAGES];
    int current_size;

    void print(){
	std::cout << "RAM = ";
	for(int i = 0; i < current_size; ++i)
	    memory[i].print();
	std::cout << '\n' << std::endl;

    }
};

// ==== OPTIMAL VARIABLES ====
//Esto deberia estar en otro lado
RAM optimal_RAM;
symbol_table optimal_table;
access_list<int> optimal_index;

//ptr(1)...
int ptr_counter = 1;
int access_counter = 0;

// ===== FILL THE REVERSED ACCESS LIST =====

void access_page(int key){
    optimal_index.push(key, access_counter++);
}

void fill_optimal(command& cmd){
    if(cmd.name == "new"){
	access_page(ptr_counter);
	ptr_counter++;
    }
    if(cmd.name == "use"){
	access_page(cmd.params[0]);
    }
}

// ===== ALGORITHM EXECUTION  =====
//Depende del algoritmo...
//WIP

void insert_ptr(int ptr){
    std::vector<page> pages = optimal_table.lookup_pages(ptr);

    for(int i = 0; i < pages.size(); ++i){
	optimal_RAM.memory[optimal_RAM.current_size++] = pages[i];
    }
}

void use_ptr(command& cmd){
    insert_ptr(cmd.params[0]);
}

void create_ptr(command& cmd){
    int page_amount = (cmd.params[1] / PAGE_SIZE) + 1;

    for(int i = 0; i < page_amount; ++i){
	symbol_entry new_ptr(optimal_table.table.size() + 1, cmd.params[0], true, ptr_counter, cmd.params[1]);
	optimal_table.table.push_back(new_ptr);
    }

    insert_ptr(ptr_counter);
    ptr_counter++;
}

// ===== COMMANDS LOGIC =====

Status execute_command(command& next_cmd){
    if(next_cmd.name == "new"){
	create_ptr(next_cmd);
    } else if(next_cmd.name == "use"){
	use_ptr(next_cmd);
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

        execute_command(new_command);

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

    execute_program(file);

    file.close();
    return Status::OK;
}


// ==== TEST?? ====

void test(){
    optimal_index.print();
    std::cout << "pop in page 1 = " << optimal_index.pop(1) << std::endl;

    optimal_index.print();
    std::cout << "farthest page (page name) = " << optimal_index.lowest_element() << std::endl;
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

    if (open_file(argv[1]) == Status::FILERROR)
	return 1;

    optimal_index.print();
    optimal_table.print();
    optimal_RAM.print();

    //test();

    return 0;
}
