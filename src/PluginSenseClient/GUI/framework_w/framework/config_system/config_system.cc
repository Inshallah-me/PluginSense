#include "../../includes.hh"

namespace core
{
	bool c_config_manager::save(const std::string& config_name)
	{
		CreateDirectoryA("C:\\phantosia", nullptr);
		std::ofstream file("C:\\phantosia\\" + config_name + ".cfg");
		if (!file.is_open()) return false;

		for (const auto& [name, entry] : this->fields) {
			file << name << "=";
			entry->save(file);
			file << "\n";
		}
		file.close();
		return true;
	}


	bool c_config_manager::load(const std::string& config_name)
	{
		std::ifstream file("C:\\phantosia\\" + config_name + ".cfg");
		if (!file.is_open()) return false;

		std::string line;
		while (std::getline(file, line)) {
			auto pos = line.find('=');
			if (pos == std::string::npos) continue;

			std::string key = line.substr(0, pos);
			std::string value = line.substr(pos + 1);

			auto it = fields.find(key);
			if (it != fields.end()) {
				it->second->load(value);
			}
		}
		return true;
	}

	std::vector<std::string> c_config_manager::get_all_configs(const std::string& folder)
	{
		std::vector<std::string> configs;

		WIN32_FIND_DATAA find_data;
		HANDLE hFind = FindFirstFileA((folder + "*.cfg").c_str(), &find_data);

		if (hFind == INVALID_HANDLE_VALUE)
			return configs;

		do {
			std::string file_name = find_data.cFileName;
			if (file_name.size() >= 4 && file_name.substr(file_name.size() - 4) == ".cfg") {
				configs.push_back(file_name.substr(0, file_name.size() - 4)); // remove ".cfg"
			}
		} while (FindNextFileA(hFind, &find_data) != 0);

		FindClose(hFind);
		return configs;
	}
}
