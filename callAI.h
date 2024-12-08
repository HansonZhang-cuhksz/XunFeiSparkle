#ifndef CALLAI_H
#define CALLAI_H

#include <string>
extern void create_workspace(std::string workspaceName);
extern void delete_workspace(std::string workspaceName);
extern void add_file(std::string workspaceName, std::string filePath, std::string fileName);
extern void delete_file(std::string workspaceName, std::string fileName);
extern void clear_memory(std::string workspaceName);
extern std::string request(std::string workspaceName, std::string msg);

#endif // CALLAI_H
