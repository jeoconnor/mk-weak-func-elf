#include <iostream>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <elf.h>
#include <map>
#include <vector>
#include <string>
#include <tuple>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <algorithm>

using namespace std;

using TableIntStr = map<int, string>;
using TableStrInt = map<string, int>;

// using SymbolTable = Table

// These are used for displaying field names
map<int, string> stt_name;
map<int, string> stb_name;
map<int, string> sht_name;

bool extract_function_names(Elf64_Ehdr* ehdr, vector<string>& mockfuncs, string& secname);
static tuple<void*, size_t> memory_map_file(string& file);
static bool verify_elf(Elf64_Ehdr* hdr);

class ElfFile {
public:
  ElfFile(string& _filename) {
    init(_filename);
  }

  ~ElfFile() {
    deinit();
  }

  void init(string& _filename) {
    filename = _filename;
    auto [_ehdr, _size] = memory_map_file(filename);
    ehdr = (Elf64_Ehdr*)_ehdr;
    size = _size;
    if (!verify_elf(ehdr)) {
      cout << "error: invalid elf file " << filename << endl;
      deinit();
    }
  }

  void deinit() {
    if (ehdr) {
      if (munmap(ehdr, size) < 0) {
	perror("munmap");
      }
      ehdr = nullptr;
      size = 0;
    }
  }

  bool ok() { return ehdr != nullptr && size != 0; }
  Elf64_Ehdr* Handle() { return ehdr; }
  int Size() { return size; }

  string filename;
  int size;
  Elf64_Ehdr* ehdr;

};

static void usage(const char* name)
{
  cout << "help...\n"
       << "more help...\n";
}

/**
 * For now just define enough entries to get by with my easy test
 * cases but fill these out later.
 */
static void init_tables()
{
  sht_name[SHT_NULL]      = "NULL";
  sht_name[SHT_SYMTAB]    = "SYMTAB";
  sht_name[SHT_STRTAB]    = "STRTAB";
  sht_name[SHT_RELA]      = "RELA";
  sht_name[SHT_PROGBITS]  = "PROGBITS";
  sht_name[SHT_NOBITS]    = "NOBITS";
  sht_name[SHT_NOTE]      = "NOTE";

  stt_name[STT_NOTYPE]    = "NOTYPE";
  stt_name[STT_OBJECT]    = "OBJECT";
  stt_name[STT_FUNC]      = "FUNC";
  stt_name[STT_SECTION]   = "SECTION";
  stt_name[STT_FILE]      = "FILE";

  stb_name[STB_LOCAL]     = "LOCAL";
  stb_name[STB_GLOBAL]    = "GLOBAL";
  stb_name[STB_WEAK]      = "WEAK";
}

Elf64_Shdr* get_section_header(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  return (Elf64_Shdr*)(buffer + ehdr->e_shoff);
}

tuple<Elf64_Sym*, int> get_symbol_header(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      return {(Elf64_Sym*)(buffer + shdr->sh_offset), nsyms};
    }
  }
  return {nullptr, 0};
}

char* get_section_string_buffer(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_STRTAB && secno == ehdr->e_shstrndx) {
      return (buffer + shdr->sh_offset);
    }
  }
  return nullptr;
}

char* get_symbol_string_buffer(Elf64_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_STRTAB && secno != ehdr->e_shstrndx) {
      return (char*)(buffer + shdr->sh_offset);
    }
  }
  return nullptr;
}

void get_last_directory_segment(string& s, const char delim, string& last_seg)
{
  int n = 0;
  int npos = n;
  for (auto p = s.begin(); p != s.end(); p++, n++) {
    if (*p == delim)
      npos = n;
  }
  if (npos > 0)
    npos++;
  last_seg = s.substr(npos);
}

bool file_has_mock_prefix(string& filename, string& prefix)
{
  string dirname;
  get_last_directory_segment(filename, '/', dirname);
  return (prefix == dirname.substr(0, prefix.size()));
}

static bool verify_elf(Elf64_Ehdr* hdr)
{
  if (hdr == nullptr) {
    printf("error: no file found\n");
    return false;
  }
  // Elf header?
  bool ok = (hdr->e_ident[EI_MAG0] == ELFMAG0 &&
	     hdr->e_ident[EI_MAG1] == ELFMAG1 &&
	     hdr->e_ident[EI_MAG2] == ELFMAG2 &&
	     hdr->e_ident[EI_MAG3] == ELFMAG3);

  // Relocatable object?
  ok = ok && (hdr->e_type == ET_REL);

  if (!ok) {
    printf("error: file not Elf relocatable object\n");
    return false;
  }

  return true;
}

static tuple<void*, size_t> memory_map_file(string& file)
{
  if (file.size() == 0)
    return {nullptr, 0};

  int fd = open(file.c_str(), O_RDWR);
  if (fd < 0) {
    perror("open");
    return {nullptr, 0};
  }

  struct stat statbuf;
  if (fstat(fd, &statbuf)) {
    perror("stat");
    return {nullptr, 0};
  }

  auto ptr = mmap(NULL, statbuf.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
  if (ptr == MAP_FAILED || ptr == nullptr) {
    perror("mmap");
    return {nullptr, 0};
  }

  close(fd);

  return {ptr, statbuf.st_size};
}

tuple<Elf64_Ehdr*, size_t> memory_map_elf_file(string& infile)
{
  auto [ptr, size] =  memory_map_file(infile);
  return {(Elf64_Ehdr*)ptr, size};
}

bool extract_function_names(Elf64_Ehdr* ehdr, vector<string>& mockfuncs, string& secname)
{
  bool found = false;

  int initial_function_number = mockfuncs.size();

  // Section header
  if (!ehdr->e_shoff) {
    cout << "error: unable to find section header table\n";
    return false;
  }

  char* elfbuf = (char*)ehdr;
  
  Elf64_Sym* symhdr = nullptr;	// symbol table

  char* shstrbuf = nullptr;	// section header string buffer
  char* strbuf = nullptr;	// symbol name string buffer

  int numsyms = 0;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(elfbuf + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_STRTAB) {
      if (secno == ehdr->e_shstrndx)
	shstrbuf = elfbuf + shdr->sh_offset;
      else
	strbuf = elfbuf + shdr->sh_offset;
    } else if (shdr->sh_type == SHT_SYMTAB) {
      symhdr = (Elf64_Sym*)(elfbuf + shdr->sh_offset);
      numsyms = shdr->sh_size / shdr->sh_entsize;
    }
  }

  int mock_index = 0;

  if (secname.size() > 0) {
    // Find section index for custom section
    shdr = (Elf64_Shdr*)(elfbuf + ehdr->e_shoff);
    for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
      if (shdr->sh_name != 0) {
	if (secname == shstrbuf + shdr->sh_name) {
	  mock_index = secno;
	  cout << "custom index is " << mock_index << endl;
	  break;
	}
      }
    }
  }
  
  for (int n=0; n < numsyms; n++, symhdr++) {
    // looking for symbols pointing to the mock section index
    if (symhdr->st_name > 0 &&
	ELF64_ST_TYPE(symhdr->st_info) == STT_FUNC &&
	ELF64_ST_BIND(symhdr->st_info) == STB_GLOBAL) {
      if (symhdr->st_shndx == mock_index || secname.size() == 0) {
	string name = strbuf + symhdr->st_name;
	cout << "adding " << name << " to function list\n";
	mockfuncs.push_back(name);
      }
    }
  }

  if (mockfuncs.size() > initial_function_number)
    found = true;

  return found;
}

void extract_function_names(Elf64_Ehdr* ehdr, vector<string>& mockfuncs)
{
  string secname("");
  (void)extract_function_names(ehdr, mockfuncs, secname);
}

int main(int argc, char** argv)
{
  string infile;
  vector<string> mockfiles;
  vector<string> nonmockfiles;
  vector<string> objfiles;
  vector<string> mockfuncs;	// use unordered_set?

  string mock_prefix("mock");
  string section_name(".mock");
  string file_suffix("");

  bool list_flag = false;
  bool write_flag = false;	// This is the point but require explicit request

  int c;
  while ((c = getopt(argc, argv, "m:f:LW")) != -1) {
    switch (c) {
    case 'm':
      mockfiles.push_back(optarg);
      break;
    case 'f':
      mockfuncs.push_back(optarg);
      break;
    case 'L':
      list_flag = true;
      break;
    case 'W':
      write_flag = true;
      break;
    default:
      usage(argv[0]);
      break;
    }
  }
#if 0
  while (true) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
        {"input-file",    required_argument, 0, 'i'},
        {"output-file",   required_argument, 0, 'o'},
        {"mock-file",     required_argument, 0, 'm'},
        {"function-name", required_argument, 0, 'f'},
        {"write-flag",    no_argument,       0, 'W'},
	{0,               0,                 0,  0 }
    };
  }
#endif
  while (optind < argc) {
    string s = argv[optind];
    if (file_has_mock_prefix(s, mock_prefix))
      mockfiles.push_back(s);
    else
      objfiles.push_back(s);
    optind++;
  }

  init_tables();

  /*
   * First build up a list of function names we want to replace from
   * the list of explicit mock files.  All global function names
   * defined in these files will be included.
   */
  for (auto pFile = mockfiles.begin(); pFile != mockfiles.end(); pFile++) {
    ElfFile elfFile(*pFile);
    
    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    extract_function_names(ehdr, mockfuncs);
  }

  /*
   * Identify object files with a ".mock" section and add .mock
   * attribute labeled functions to the list of mock functions.
   * Separate the files into mock and non-mock lists.
   */
  for(auto pFile = objfiles.begin(); pFile != objfiles.end(); pFile++) {
    ElfFile elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    if (extract_function_names(ehdr, mockfuncs, section_name)) {
      mockfiles.push_back(*pFile);
    } else {
      nonmockfiles.push_back(*pFile);
    }
  }

  /*
   * The non-mock files are the ones to modify
   */
  for(auto pFile = nonmockfiles.begin(); pFile != nonmockfiles.end(); pFile++) {
    ElfFile elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    char* symbuf = get_symbol_string_buffer(ehdr);
    auto [shdr, nsyms] = get_symbol_header(ehdr);
    for (int idx=0; idx < nsyms; idx++, shdr++) {
      string func_name(symbuf + shdr->st_name);
      if (shdr->st_name != 0 && ELF64_ST_TYPE(shdr->st_info) == STT_FUNC) {
	for (auto p=mockfuncs.begin(); p<mockfuncs.end(); p++) {
	  if (*p == func_name) {
	    if (write_flag) {
	      cout << "patching " << func_name << " in " << *pFile << endl;
	      shdr->st_info = ELF64_ST_INFO(STB_WEAK, ELF64_ST_TYPE(shdr->st_info));
	    }
	  }
	}
      }
    }

    if (write_flag) {
      if (msync(ehdr, elfFile.Size(), MS_SYNC) != 0) {
	perror("msync");
      }
    }
  }

  exit(0);
}
