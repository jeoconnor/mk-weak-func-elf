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

// These are used for displaying field names
map<int, string> stt_name;
map<int, string> stb_name;
map<int, string> sht_name;

static tuple<void*, size_t> memory_map_file(string& file);
static unsigned char verify_elf(void* hdr);

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void process_files(vector<string>& infiles, vector<string>& dupfiles, vector<string>& funclist,
		   string& section_name, bool write_flag);

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
bool extract_function_names(ElfNN_Ehdr* ehdr, vector<string>& funclist, string& secname);

template <typename ElfNN_Ehdr>
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
    ehdr = (ElfNN_Ehdr*)_ehdr;
    size = _size;
    if (!verify_elf(ehdr)) {
      cout << "error: invalid elf file " << filename << endl;
      deinit();
    }
  }

  char check_arch() {
    return ehdr->e_ident[EI_CLASS];
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
  ElfNN_Ehdr* Handle() { return ehdr; }
  int Size() { return size; }

  string filename;
  int size;
  ElfNN_Ehdr* ehdr;

};

using ElfFile64 = ElfFile<Elf64_Ehdr>;
using ElfFile32 = ElfFile<Elf32_Ehdr>;

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

template <typename ElfNN_Ehdr,  typename ElfNN_Shdr>
ElfNN_Shdr* get_section_header(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  return (ElfNN_Shdr*)(buffer + ehdr->e_shoff);
}

template <typename ElfNN_Shdr, typename ElfNN_Sym, typename ElfNN_Ehdr>
tuple<ElfNN_Sym*, int> get_symbol_header(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  auto shdr = get_section_header<ElfNN_Ehdr, ElfNN_Shdr>(ehdr);
  for (int secno = 0; secno < ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_SYMTAB) {
      int nsyms = shdr->sh_size / shdr->sh_entsize;
      return {(ElfNN_Sym*)(buffer + shdr->sh_offset), nsyms};
    }
  }
  return {nullptr, 0};
}

template <typename ElfN_Ehdr, typename ElfNN_Shdr>
char* get_section_string_buffer(ElfN_Ehdr* ehdr)
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

template<typename ElfNN_Shdr, typename ElfNN_Ehdr>
char* get_symbol_string_buffer(ElfNN_Ehdr* ehdr)
{
  char* buffer = (char*)ehdr;
  ElfNN_Shdr* shdr = (ElfNN_Shdr*)(buffer + ehdr->e_shoff);
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

char check_arch(string& filename)
{
  ElfFile64 elfFile(filename);
  return elfFile.check_arch();
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

inline bool symbol_check_type(Elf64_Sym* sym) {
  return (sym->st_name > 0) &&
    (ELF64_ST_TYPE(sym->st_info) == STT_FUNC) &&
    (ELF64_ST_BIND(sym->st_info) == STB_GLOBAL);
}
  
inline bool symbol_check_type(Elf32_Sym* sym) {
  return (sym->st_name > 0) &&
    (ELF32_ST_TYPE(sym->st_info) == STT_FUNC) &&
    (ELF32_ST_BIND(sym->st_info) == STB_GLOBAL);
}
  
template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
bool extract_function_names(ElfNN_Ehdr* ehdr, vector<string>& funclist, string& secname)
{
  bool found = false;

  int initial_function_number = funclist.size();

  // Section header
  if (!ehdr->e_shoff) {
    cout << "error: unable to find section header table\n";
    return false;
  }

  char* elfbuf = (char*)ehdr;
  
  ElfNN_Sym* symhdr = nullptr;	// symbol table

  char* shstrbuf = nullptr;	// section header string buffer
  char* strbuf = nullptr;	// symbol name string buffer

  /*
   * Scan through the section header table locating the symbol string
   * buffer, the section header symbol string buffer and the symbol
   * table.  Result is pointers to each plus the number of entries in
   * the symbol table.
   */
  int numsyms = 0;		// No. entries in the symbol table
  ElfNN_Shdr* shdr = (ElfNN_Shdr*)(elfbuf + ehdr->e_shoff);
  for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
    if (shdr->sh_type == SHT_STRTAB) {
      if (secno == ehdr->e_shstrndx)
	shstrbuf = elfbuf + shdr->sh_offset;
      else
	strbuf = elfbuf + shdr->sh_offset;
    } else if (shdr->sh_type == SHT_SYMTAB) {
      symhdr = (ElfNN_Sym*)(elfbuf + shdr->sh_offset);
      numsyms = shdr->sh_size / shdr->sh_entsize;
    }
  }

  /*
   * Now scan through the section header symbol table looking for our
   * special section.
   */
  int custom_index = 0;
  if (secname.size() > 0) {
    // Find section index for custom section
    shdr = (ElfNN_Shdr*)(elfbuf + ehdr->e_shoff);
    for (int secno=0; secno<ehdr->e_shnum; secno++, shdr++) {
      if (shdr->sh_name != 0) {
	if (secname == shstrbuf + shdr->sh_name) {
	  custom_index = secno;
	  // cout << "custom index is " << custom_index << endl;
	  break;
	}
      }
    }
  }

  /*
   * Look for symbols referencing the special section
   */
  for (int n=0; n < numsyms; n++, symhdr++) {
#if 0
    if (symhdr->st_name > 0 &&
	ELF64_ST_TYPE(symhdr->st_info) == STT_FUNC &&
	ELF64_ST_BIND(symhdr->st_info) == STB_GLOBAL) {
#else
      if (symbol_check_type(symhdr)) {
#endif
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

void patch_file(Elf64_Sym *shdr, char* symbuf, vector<string>& function_names)
{
  // XXX check also that it is GLOBAL
  if (shdr->st_name != 0 && ELF64_ST_TYPE(shdr->st_info) == STT_FUNC) {
    for (auto p=function_names.begin(); p!=function_names.end(); p++) {
      string func_name(symbuf + shdr->st_name);
      if (*p == func_name) {
	// cout << "patching " << func_name << " in " << *pFile << endl;
	shdr->st_info = ELF64_ST_INFO(STB_WEAK, ELF64_ST_TYPE(shdr->st_info));
      }
    }
  }
}

void patch_file(Elf32_Sym *shdr, char* symbuf, vector<string>& function_names)
{
  // XXX check also that it is GLOBAL
  if (shdr->st_name != 0 && ELF32_ST_TYPE(shdr->st_info) == STT_FUNC) {
    for (auto p=function_names.begin(); p!=function_names.end(); p++) {
      string func_name(symbuf + shdr->st_name);
      if (*p == func_name) {
	// cout << "patching " << func_name << " in " << *pFile << endl;
	shdr->st_info = ELF32_ST_INFO(STB_WEAK, ELF32_ST_TYPE(shdr->st_info));
      }
    }
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void patch_files(vector<string>& objfiles, vector<string>& function_names)
{
  for(auto pFile = objfiles.begin(); pFile != objfiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    char* symbuf = get_symbol_string_buffer<ElfNN_Shdr>(ehdr);
    auto [shdr, nsyms] = get_symbol_header<ElfNN_Shdr, ElfNN_Sym>(ehdr);
    for (int idx=0; idx < nsyms; idx++, shdr++) {
      patch_file(shdr, symbuf, function_names);
    }

    if (msync(ehdr, elfFile.Size(), MS_SYNC) != 0) {
      perror("msync");
    }
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void extract_function_names(ElfNN_Ehdr* ehdr, vector<string>& funclist)
{
  string secname("");
  (void)extract_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(ehdr, funclist, secname);
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void extract_function_names(vector<string>& dupfiles, vector<string>& funclist)
{
  for (auto pFile = dupfiles.begin(); pFile != dupfiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);
    
    auto ehdr = elfFile.Handle();
    if (ehdr)
      extract_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(ehdr, funclist);
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void extract_labeled_function_names(vector<string>& infiles, vector<string>& funclist,
				    string& section_name, vector<string>& outfiles)
{
  for(auto pFile = infiles.begin(); pFile != infiles.end(); pFile++) {
    ElfFile<ElfNN_Ehdr> elfFile(*pFile);

    auto ehdr = elfFile.Handle();
    if (!ehdr)
      continue;

    if (!extract_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(ehdr, funclist, section_name)) {
      outfiles.push_back(*pFile);
    }
  }
}

template<typename ElfNN_Ehdr, typename ElfNN_Shdr, typename ElfNN_Sym>
void process_files(vector<string>& infiles, vector<string>& dupfiles, vector<string>& funclist,
		   string& section_name, bool write_flag)
{
  /*
   * First build up a list of function names we want to replace from
   * the list of explicit mock files.  All global function names
   * defined in these files will be included.
   */
  extract_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(dupfiles, funclist);

  /*
   * Identify object files with an identifed, i.e. labeled, section
   * and add those function names to the function list.
   * Also, saves the list of objfiles that did not contain labeled
   * sections.
   */
  vector<string> objfiles;	// contain functions to be replaced
  extract_labeled_function_names<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(infiles, funclist, section_name, objfiles);

  /*
   * The non-mock files are the ones to modify
   */
  if (write_flag)
    patch_files<ElfNN_Ehdr, ElfNN_Shdr, ElfNN_Sym>(objfiles, funclist);

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
    case 'r':
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
   * Figure out if we have elf32 or elf64 files by checking the first
   * one.
   */
  switch (check_arch(*(infiles.begin()))) {
  case ELFCLASS32:
    process_files<Elf32_Ehdr, Elf32_Shdr, Elf32_Sym>(infiles, dupfiles, funclist, section_name, write_flag);
    break;
  case ELFCLASS64:
    process_files<Elf64_Ehdr, Elf64_Shdr, Elf64_Sym>(infiles, dupfiles, funclist, section_name, write_flag);
    break;
  case ELFCLASSNONE:
  default:
    exit(1);
    break;
  }

  if (list_flag) {
    for_each(funclist.begin(), funclist.end(), [](auto p){ cout << p << endl; });
    exit(0);
  }

  exit(0);
}
