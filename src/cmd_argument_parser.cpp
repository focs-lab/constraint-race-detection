#include <algorithm>
#include <string>
#include <vector>

class Parser {
   private:
    std::vector<std::string> arguments;

   public:
    Parser(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            arguments.push_back(argv[i]);
        }
    }

    bool hasArgument(const std::string& argument) const {
        return std::find(arguments.begin(), arguments.end(), argument) !=
               arguments.end();
    }

    std::string getArgument(const std::string& argument) const {
        auto itr = std::find(arguments.begin(), arguments.end(), argument);
        if (itr == arguments.end() || itr + 1 == arguments.end()) {
            return "";
        }
        return *(++itr);
    }
};