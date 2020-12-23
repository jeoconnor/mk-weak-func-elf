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

static tuple<void*, size_t> memory_map_file(string& file);
static unsigned char verify_elf(void* hdr);

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

template <class ElfNN_Ehdr, class ElfNN_Shdr>
ElfNN_Shdr* get_section_header(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  return (ElfNN_Shdr*)(buffer + ehdr->e_shoff);
}

tuple<Elf64_Sym*, int> get_symbol_header(Elf64_Ehdr* ehdr)
{
#if 1
  char* buffer = (char*)ehdr;
  Elf64_Shdr* shdr = (Elf64_Shdr*)(buffer + ehdr->e_shoff);
#else
  auto shdr = get_section_header(ehdr);
#endif
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      return {(Elf64_Sym*)(buffer + shdr->sh_offset), nsyms};
    }
  }
  return {nullptr, 0};
}

template <class ElfNN_Ehdr, class ElfNN_Shdr>
char* get_section_string_buffer(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  ElfNN_Shdr* shdr = (ElfNN_Shdr*)(buffer + ehdr->e_shoff);
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

bool file_has_select_prefix(string& filename, string& prefix)
{
  string dirname;
  get_last_directory_segment(filename, '/', dirname);
  return (prefix == dirname.substr(0, prefix.size()));
}

/**
 * Check if input ptr references a valid elf file.  Returns either
 * ELFCLASS32 or ELFCLASS64 on sucess or ELFCLASSNONE on failure.
 */
static unsigned char verify_elf(void* ptr)
{
  unsigned char ei_class = ELFCLASSNONE;

  if (ptr == nullptr) {
    printf("error: no file found\n");
    return ei_class;
  }

  auto hdr = (Elf64_Ehdr*)ptr;

  if (hdr->e_ident[EI_MAG0] != ELFMAG0 &&
      hdr->e_ident[EI_MAG1] != ELFMAG1 &&
      hdr->e_ident[EI_MAG2] != ELFMAG2 &&
      hdr->e_ident[EI_MAG3] != ELFMAG3) {
    cout << "error: Elf magic number not found\n";
    return ei_class;
  }

  if (hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
    cout << "error: file must be 2's complement, little-endian\n";
    return ei_class;
  }

  // Relocatable object?
  if (hdr->e_type != ET_REL) {
    cout << "error: file must be relocatable object\n";
    return ei_class;
  }

  /* check the class last since its value is returned on success */
  ei_class = hdr->e_ident[EI_CLASS];
  if ((ei_class != ELFCLASS32) &&
      (ei_class != ELFCLASS64)) {
    cout << "error: only Elf32 and Elf64 architectures supported\n";
    return ei_class;
  }

  return ei_class;
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

bool extract_function_names(Elf64_Ehdr* ehdr, vector<string>& funclist, string& secname)
{
  bool found = false;

  int initial_function_number = funclist.size();

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

  int custom_index = 0;

  if (secname.size() > 0) {
    // Find section index for custom section
    shdr = (Elf64_Shdr*)(elfbuf + ehdr->e_shoff);
    for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
      if (shdr->sh_name != 0) {
	if (secname == shstrbuf + shdr->sh_name) {
	  custom_index = secno;
	  cout << "custom index is " << custom_index << endl;
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
      if (symhdr->st_shndx == custom_index || secname.size() == 0) {
	string name = strbuf + symhdr->st_name;
	cout << "adding " << name << " to function list\n";
	funclist.push_back(name);
      }
    }
  }

  if (funclist.size() > initial_function_number)
    found = true;

  return found;
}

void extract_function_names(Elf64_Ehdr* ehdr, vector<string>& funclist)
{
  string secname("");
  (void)extract_function_names(ehdr, funclist, secname);
}

void patch_files(vector<string>& objfiles, vector<string>& function_names)
{
  for(auto pFile = objfiles.begin(); pFile != objfiles.end(); pFile++) {
    ElfFile elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    char* symbuf = get_symbol_string_buffer(ehdr);
    auto [shdr, nsyms] = get_symbol_header(ehdr);
    for (int idx=0; idx < nsyms; idx++, shdr++) {
      // XXX check also that it is GLOBAL
      if (shdr->st_name != 0 && ELF64_ST_TYPE(shdr->st_info) == STT_FUNC) {
	for (auto p=function_names.begin(); p!=function_names.end(); p++) {
	  string func_name(symbuf + shdr->st_name);
	  if (*p == func_name) {
	      cout << "patching " << func_name << " in " << *pFile << endl;
	      shdr->st_info = ELF64_ST_INFO(STB_WEAK, ELF64_ST_TYPE(shdr->st_info));
	  }
	}
      }
    }

    if (msync(ehdr, elfFile.Size(), MS_SYNC) != 0) {
      perror("msync");
    }
  }
}

int main(int argc, char** argv)
{
  vector<string> dupfiles;	// contain replacement function definitions
  vector<string> infiles;	// unclassified input files
  vector<string> funclist;	// use unordered_set?

  string select_prefix("mock");
  string section_name(".mock");

  bool list_flag = false;
  bool write_flag = false;	// This is the point but require explicit request

  int c;
  while ((c = getopt(argc, argv, "r:f:lw")) != -1) {
    switch (c) {
    case 'm':
      dupfiles.push_back(optarg);
      break;
    case 'f':
      funclist.push_back(optarg);
      break;
    case 'l':
      list_flag = true;
      break;
    case 'w':
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
        {"replacement-file", required_argument, 0, 'r'},
        {"function-name",    required_argument, 0, 'f'},
        {"write-flag",       no_argument,       0, 'w'},
	{0,               0,                 0,  0 }
    };
  }
#endif
  while (optind < argc) {
    string s = argv[optind];
    if (file_has_select_prefix(s, select_prefix))
      dupfiles.push_back(s);
    else
      infiles.push_back(s);
    optind++;
  }

  init_tables();

  /*
   * First build up a list of function names we want to replace from
   * the list of explicit mock files.  All global function names
   * defined in these files will be included.
   */
  for (auto pFile = dupfiles.begin(); pFile != dupfiles.end(); pFile++) {
    ElfFile elfFile(*pFile);
    
    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    extract_function_names(ehdr, funclist);
  }

  /*
   * Identify object files with a ".mock" section and add .mock
   * attribute labeled functions to the list of mock functions.
   * Separate the files into mock and non-mock lists.
   */

  vector<string> objfiles;	// contain functions to be replaced
  for(auto pFile = infiles.begin(); pFile != infiles.end(); pFile++) {
    ElfFile elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    if (!extract_function_names(ehdr, funclist, section_name)) {
      objfiles.push_back(*pFile);
    }
  }

  if (list_flag) {
    for (auto p=funclist.begin(); p<funclist.end(); p++) {
      cout << *p << endl;
    }
    exit(0);
  }

  /*
   * The non-mock files are the ones to modify
   */
  if (write_flag)
    patch_files(objfiles, funclist);

  exit(0);
}
