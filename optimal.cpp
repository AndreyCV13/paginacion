#include <iostream>
#include <cstddef>
#include <cstdint>
#include <fstream>
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

    symbol_entry(int page, int process = 1) : pageId(page), pId(process) {}
};

struct symbol_table{
    std::vector<symbol_entry> table;
};

struct page{
    int size = PAGE_SIZE;
};

struct RAM{
    page memory[RAM_PAGES];
    int current_size;
};

// ==== OPTIMAL VARIABLES ====
//Esto deberia estar en otro lado
RAM optimal_RAM;
symbol_table optimal_table;
access_list<int> optimal_index;
int page_counter = 0;

// ===== FILL THE REVERSED ACCESS LIST =====
//Asumamos que todos los punteros entran en una pagina :)
//Parámetros "harcoded"

void access_page(int key, int value){
    optimal_index.push(key, value);
    page_counter++;
}

void fill_optimal(command& cmd){
    int some_page = (page_counter % 3) + 1;

    if(cmd.name == "new" || cmd.name == "use"){
	access_page(some_page, page_counter);
    }
}

// ===== ALGORITHM EXECUTION  =====
//Depende del algoritmo...
//Asumamos que todos los punteros entran en una pagina :)
//WIP
void use_ptr(command& cmd){
   page new_page;
   optimal_RAM.memory[optimal_RAM.current_size++];
}

void create_ptr(command& cmd){
    int table_size = optimal_table.table.size();
    symbol_entry new_ptr(table_size + 1);
    optimal_table.table.push_back(new_ptr);

    use_ptr(cmd);
}

// ===== COMMANDS LOGIC =====

Status execute_command(command& next_cmd){
    if(next_cmd.name == "new"){
	std::cout << "Hay un new" << std::endl;
	create_ptr(next_cmd);
    } else if(next_cmd.name == "use"){
	std::cout << "Hay un use" << std::endl;
	use_ptr(next_cmd);
    } else if(next_cmd.name == "delete")
	std::cout << "Hay un delete" << std::endl;
    else if(next_cmd.name == "kill")
	std::cout << "Hay un kill" << std::endl;
    else {
	std::cerr << "Comando no identificado" << std::endl;
	return Status::SYNTAXERROR;
    }
    return Status::OK;
}

Status parse_line(const std::string& line, command& new_command){
    size_t open = line.find('(');
    size_t close = line.find(')');

    if(open == std::string::npos || close == std::string::npos)
	return Status::SYNTAXERROR;

    new_command.name = line.substr(0, open);

    //Parse parameters
    //parse_parameters()
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
    optimal_index.print_list();
    std::cout << "pop in page 1 = " << optimal_index.pop(1) << std::endl;

    optimal_index.print_list();
    std::cout << "farthest page (page name) = " << optimal_index.lowest_element() << std::endl;
    optimal_index.print_list();


    std::cout << "pop in page 2 = " << optimal_index.pop(2) << std::endl;
    optimal_index.print_list();

}

// ==== MAIN ====

int main(int argc, char **argv) {
    if (argc != 2) {
	std::cerr << "Usage: optimal <input>" << std::endl;
        return 1;
    }

    if (open_file(argv[1]) == Status::FILERROR)
	return 1;

    test();

    return 0;
}
