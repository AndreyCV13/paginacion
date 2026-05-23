#include <iostream>
#include <vector>
#include <unordered_map>
#include <climits>
#include <set>

#ifndef REVERSED_ACCESS_LIST_H
#define REVERSED_ACCESS_LIST_H

//Por si acaso :p
template <typename E>
struct access_list {
    //Attributes
    std::unordered_map<E, std::vector<int>> inverted_index;

    //Methods
    void push(E key, int index){
		inverted_index[key].push_back(index);
    }

   //Se podria hacer una optimizacion para solo devolver "logicamente el elemento sin borrarlo, o usar array :p"
   	int pop(E key){
		auto& v = inverted_index[key];
		if (v.empty()) return -1;
		int first = v.front();
        v.erase(v.begin());
		return first;
    }

    int farthest_element(std::set<E>& ptrs){
		int highest_element = -1;
		E page;

		for (auto& ptr : ptrs){
			if (inverted_index[ptr].size() == 0) return ptr;
			if (inverted_index[ptr].front() > highest_element){
			highest_element = inverted_index[ptr].front();
				page = ptr;
			}
		}
		//pop(page);
		return page;
    }

    void print(){
		std::cout << "Lista de accesos\n";
		for (const auto& [key, value] : inverted_index){
			std::cout << "Pagina " << key << " = ";
			for (int i = 0; i < value.size(); ++i) {
				std::cout << value[i] << ' ';
			}
			std::cout << '\n';
		}
		std::cout << std::endl;
    }
};

#endif
