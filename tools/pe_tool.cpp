// pe_tool - reads the export and import tables of Windows DLLs (PE32/PE32+).
// (c) by Andreas Weller - https://www.andreas-weller.de/
//
// Needs no Windows API, no Qt, no MinGW binutils. Used by tools/build-windows.ps1
// to (1) create the import library of the Veyon version that is installed on the
// computer and (2) check that every function the finished plugin imports really
// exists in the installed Veyon and Qt DLLs, which is what decides whether
// Windows can load the plugin.
//
//   pe_tool exports <dll>                    one exported name per line
//   pe_tool def     <dll> <out.def>          .def file for dlltool (import library)
//   pe_tool imports <dll>                    "<dll name> <symbol>" per imported function
//   pe_tool check   <plugin.dll> <folder>    every import that points to a DLL found in
//                                            <folder> must be exported by it; exit code 1
//                                            if something is missing
//
// Build: g++ -std=c++14 -O1 tools/pe_tool.cpp -o pe_tool.exe -static
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace
{

struct Pe
{
	std::vector<unsigned char> data;
	struct Section { std::uint32_t va, vsize, raw, rawsize; };
	std::vector<Section> sections;
	bool is64 = false;
	std::uint32_t exportRva = 0, exportSize = 0, importRva = 0;

	std::uint16_t u16(size_t o) const { return o + 2 <= data.size() ? std::uint16_t(data[o] | (data[o + 1] << 8)) : 0; }
	std::uint32_t u32(size_t o) const
	{
		return o + 4 <= data.size() ? std::uint32_t(data[o]) | (std::uint32_t(data[o + 1]) << 8)
		                                | (std::uint32_t(data[o + 2]) << 16) | (std::uint32_t(data[o + 3]) << 24) : 0;
	}
	std::uint64_t u64(size_t o) const { return std::uint64_t(u32(o)) | (std::uint64_t(u32(o + 4)) << 32); }

	// RVA -> file offset (0 if the RVA is not backed by the file)
	size_t offset(std::uint32_t rva) const
	{
		for (const Section& s : sections)
		{
			const std::uint32_t span = std::max(s.vsize, s.rawsize);
			if (rva >= s.va && rva < s.va + span)
				return s.raw + (rva - s.va);
		}
		return 0;
	}

	std::string cstr(size_t o) const
	{
		std::string s;
		while (o < data.size() && data[o])
			s += static_cast<char>(data[o++]);
		return s;
	}

	bool load(const std::string& path, std::string& error)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f) { error = "cannot open " + path; return false; }
		data.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		if (data.size() < 0x100 || data[0] != 'M' || data[1] != 'Z') { error = path + ": not a PE file"; return false; }
		const size_t pe = u32(0x3c);
		if (u32(pe) != 0x00004550) { error = path + ": no PE signature"; return false; }
		const std::uint16_t nsec = u16(pe + 6);
		const std::uint16_t optSize = u16(pe + 20);
		const size_t opt = pe + 24;
		const std::uint16_t magic = u16(opt);
		if (magic != 0x20b && magic != 0x10b) { error = path + ": unknown PE format"; return false; }
		is64 = magic == 0x20b;
		const size_t dirs = opt + (is64 ? 112 : 96);
		exportRva = u32(dirs);
		exportSize = u32(dirs + 4);
		importRva = u32(dirs + 8);
		const size_t sec = opt + optSize;
		for (std::uint16_t i = 0; i < nsec; ++i)
		{
			const size_t s = sec + size_t(i) * 40;
			sections.push_back({ u32(s + 12), u32(s + 8), u32(s + 20), u32(s + 16) });
		}
		return true;
	}

	std::vector<std::string> exportNames() const
	{
		std::vector<std::string> names;
		const size_t e = offset(exportRva);
		if (!exportRva || !e)
			return names;
		const std::uint32_t count = u32(e + 24);
		const size_t table = offset(u32(e + 32));
		for (std::uint32_t i = 0; i < count && table; ++i)
		{
			const size_t n = offset(u32(table + size_t(i) * 4));
			if (n)
				names.push_back(cstr(n));
		}
		return names;
	}

	std::string moduleName() const
	{
		const size_t e = offset(exportRva);
		return e ? cstr(offset(u32(e + 12))) : std::string();
	}

	// (dll name, symbol); ordinal imports are reported as "#<n>"
	std::vector<std::pair<std::string, std::string>> imports() const
	{
		std::vector<std::pair<std::string, std::string>> out;
		size_t d = offset(importRva);
		while (importRva && d && u32(d + 12) != 0)
		{
			const std::string dll = cstr(offset(u32(d + 12)));
			std::uint32_t thunkRva = u32(d);                 // OriginalFirstThunk
			if (!thunkRva)
				thunkRva = u32(d + 16);                      // FirstThunk
			size_t t = offset(thunkRva);
			const size_t step = is64 ? 8 : 4;
			while (t)
			{
				const std::uint64_t entry = is64 ? u64(t) : u32(t);
				if (!entry)
					break;
				const std::uint64_t ordinalFlag = is64 ? (std::uint64_t(1) << 63) : 0x80000000u;
				if (entry & ordinalFlag)
					out.emplace_back(dll, "#" + std::to_string(entry & 0xffff));
				else
					out.emplace_back(dll, cstr(offset(static_cast<std::uint32_t>(entry)) + 2));
				t += step;
			}
			d += 20;
		}
		return out;
	}
};

std::string lower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	return s;
}

bool fileExists(const std::string& path)
{
	std::ifstream f(path, std::ios::binary);
	return bool(f);
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: pe_tool exports|imports <dll> | def <dll> <out.def> | check <plugin.dll> <folder>\n");
		return 2;
	}
	const std::string mode = argv[1];
	std::string error;
	Pe pe;
	if (!pe.load(argv[2], error))
	{
		std::fprintf(stderr, "%s\n", error.c_str());
		return 2;
	}

	if (mode == "exports")
	{
		for (const auto& n : pe.exportNames())
			std::printf("%s\n", n.c_str());
		return 0;
	}

	if (mode == "imports")
	{
		for (const auto& i : pe.imports())
			std::printf("%s %s\n", i.first.c_str(), i.second.c_str());
		return 0;
	}

	if (mode == "def" && argc >= 4)
	{
		std::string module = pe.moduleName();
		if (module.empty())
			module = argv[2];
		std::vector<std::string> names = pe.exportNames();
		std::sort(names.begin(), names.end());
		std::ofstream out(argv[3]);
		out << "LIBRARY \"" << module << "\"\nEXPORTS\n";
		for (const auto& n : names)
			out << n << "\n";
		std::printf("%zu exports written to %s\n", names.size(), argv[3]);
		return out ? 0 : 2;
	}

	if (mode == "check" && argc >= 4)
	{
		std::string folder = argv[3];
		if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
			folder += '\\';

		std::map<std::string, std::vector<std::string>> byDll;
		for (const auto& i : pe.imports())
			byDll[lower(i.first)].push_back(i.second);

		int missing = 0, checked = 0;
		std::set<std::string> systemDlls;
		for (const auto& entry : byDll)
		{
			const std::string path = folder + entry.first;
			if (!fileExists(path))
			{
				systemDlls.insert(entry.first);   // Windows / compiler runtime DLL, not part of Veyon
				continue;
			}
			Pe dll;
			if (!dll.load(path, error))
			{
				std::fprintf(stderr, "%s\n", error.c_str());
				return 2;
			}
			const auto names = dll.exportNames();
			const std::set<std::string> exported(names.begin(), names.end());
			int dllMissing = 0;
			for (const auto& sym : entry.second)
			{
				++checked;
				if (sym[0] != '#' && !exported.count(sym))
				{
					std::printf("MISSING  %s: %s\n", entry.first.c_str(), sym.c_str());
					++dllMissing;
				}
			}
			std::printf("%-22s %zu imports, %d missing\n", entry.first.c_str(), entry.second.size(), dllMissing);
			missing += dllMissing;
		}
		for (const auto& s : systemDlls)
			std::printf("%-22s (not in the Veyon folder: system / runtime DLL, not checked)\n", s.c_str());
		std::printf("%d imports checked against the Veyon folder, %d missing\n", checked, missing);
		return missing ? 1 : 0;
	}

	std::fprintf(stderr, "unknown mode\n");
	return 2;
}
